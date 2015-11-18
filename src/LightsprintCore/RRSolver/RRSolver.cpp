// --------------------------------------------------------------------------
// Dynamic GI solver.
// Copyright (c) 2006-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <set>
#include "Lightsprint/RRSolver.h"
#include "report.h"
#include "private.h"
#include "../RRStaticSolver/rrcore.h" // build of packed factors
#include "../RRStaticSolver/pathtracer.h" // pathTraceFrame()
#include <unordered_set>

namespace rr
{

// times in seconds
#define PAUSE_AFTER_CRITICAL_INTERACTION 0.2f // stops calculating after each interaction, improves responsiveness
#define IMPROVE_STEP_MIN 0.005f
#define IMPROVE_STEP_MAX 0.08f
#define IMPROVE_STEP_NO_INTERACTION 0.1f // length of one improve step when there are no interactions from user
#define IMPROVE_STEP_MIN_AFTER_BIG_RESET 0.035f // minimal length of first improve step after big reset (to avoid 1frame blackouts)
#define READING_RESULTS_PERIOD_MIN 0.1f // how often results are read back when scene doesn't change
#define READING_RESULTS_PERIOD_MAX 1.5f //
#define READING_RESULTS_PERIOD_GROWTH 1.3f // reading results period is increased this times at each read since scene change
// portions in <0..1>
#define MIN_PORTION_SPENT_IMPROVING 0.4f // at least 40% of our time is spent in improving
// kdyz se jen renderuje a improvuje (rrbugs), az do 0.6 roste vytizeni cpu(dualcore) a nesnizi se fps
// kdyz se navic detekuje primary, kazde zvyseni snizi fps

/////////////////////////////////////////////////////////////////////////////
//
// CalculateParameters

bool RRSolver::CalculateParameters::operator ==(const RRSolver::CalculateParameters& a) const
{
	return 1
		&& a.materialEmittanceMultiplier==materialEmittanceMultiplier
		&& a.materialEmittanceStaticQuality==materialEmittanceStaticQuality
		&& a.materialEmittanceVideoQuality==materialEmittanceVideoQuality
		&& a.materialEmittanceUsePointMaterials==materialEmittanceUsePointMaterials
		&& a.materialTransmittanceStaticQuality==materialTransmittanceStaticQuality
		&& a.materialTransmittanceVideoQuality==materialTransmittanceVideoQuality
		&& a.environmentStaticQuality==environmentStaticQuality
		&& a.environmentVideoQuality==environmentVideoQuality
		&& a.qualityIndirectDynamic==qualityIndirectDynamic
		&& a.qualityIndirectStatic==qualityIndirectStatic
		&& a.secondsBetweenDDI==secondsBetweenDDI
		;
}

/////////////////////////////////////////////////////////////////////////////
//
// UpdateParameters

bool RRSolver::UpdateParameters::operator ==(const RRSolver::UpdateParameters& a) const
{
	return 1
		&& a.applyLights==applyLights
		&& a.applyEnvironment==applyEnvironment
		&& a.applyCurrentSolution==applyCurrentSolution
		&& a.quality==quality
		&& a.qualityFactorRadiosity==qualityFactorRadiosity
		&& a.insideObjectsThreshold==insideObjectsThreshold
		&& a.rugDistance==rugDistance
		&& a.locality==locality
		&& a.aoIntensity==aoIntensity
		&& a.aoSize==aoSize
		&& a.measure_internal==measure_internal
		&& a.debugObject==debugObject
		&& a.debugTexel==debugTexel
		&& a.debugTriangle==debugTriangle
		&& a.debugRay==debugRay
		;
}

/////////////////////////////////////////////////////////////////////////////
//
// FilteringParameters

bool RRSolver::FilteringParameters::operator ==(const RRSolver::FilteringParameters& a) const
{
	return 1
		&& a.smoothingAmount==smoothingAmount
		&& a.spreadForegroundColor==spreadForegroundColor
		&& a.backgroundColor==backgroundColor
		&& a.wrap==wrap
		;
}

/////////////////////////////////////////////////////////////////////////////
//
// RRSolver

RRSolver::RRSolver()
{
	aborting = false;
	priv = new Private;
}

RRSolver::~RRSolver()
{
	delete priv;
}

void RRSolver::setScaler(const RRScaler* _scaler)
{
	priv->scaler = _scaler;
	// update fast conversion table for our setDirectIllumination
	for (unsigned i=0;i<256;i++)
	{
		RRVec3 c(i*priv->boostCustomIrradiance/255);
		if (_scaler) _scaler->getPhysicalScale(c);
		priv->customToPhysical[i] = c[0];
	}
	// tell realtime solver to update GI (=re-run DDI with new scaler)
	reportDirectIlluminationChange(-1,false,true,false);
}

const RRScaler* RRSolver::getScaler() const
{
	return priv->scaler;
}

void RRSolver::setEnvironment(RRBuffer* _environment0, RRBuffer* _environment1, RRReal _environmentAngleRad0, RRReal _environmentAngleRad1)
{
	// [#23] inc/dec refcount of environments entering/leaving solver
	// (so that later we can delete env from slot0, forget about it, and then move env from slot0 to slot1 for fadeout)
	if (_environment0 && _environment0!=priv->environment0) _environment0->createReference();
	if (_environment1 && _environment1!=priv->environment1) _environment1->createReference();
	if (priv->environment0 && _environment0!=priv->environment0) delete priv->environment0;
	if (priv->environment1 && _environment1!=priv->environment1) delete priv->environment1;

	if (priv->environment0!=_environment0 || priv->environment1!=_environment1 || priv->environmentAngleRad0!=_environmentAngleRad0 || priv->environmentAngleRad1!=_environmentAngleRad1)
	{
		priv->environment0 = _environment0;
		priv->environment1 = _environment1;
		priv->environmentAngleRad0 = _environmentAngleRad0;
		priv->environmentAngleRad1 = _environmentAngleRad1;
		// affects specular cubemaps
		priv->solutionVersion++;
	}
}

RRBuffer* RRSolver::getEnvironment(unsigned _environmentIndex, RRReal* _environmentAngleRad) const
{
	if (_environmentAngleRad)
		*_environmentAngleRad = _environmentIndex ? priv->environmentAngleRad1 : priv->environmentAngleRad0;
	return _environmentIndex ? priv->environment1 : priv->environment0;
}

void RRSolver::setEnvironmentBlendFactor(float _blendFactor)
{
	if (priv->environmentBlendFactor!=_blendFactor)
	{
		priv->environmentBlendFactor = _blendFactor;
		// affects specular cubemaps
		priv->solutionVersion++;
	}
}

float RRSolver::getEnvironmentBlendFactor() const
{
	return priv->environmentBlendFactor;
}

void RRSolver::setLights(const RRLights& _lights)
{
	if (!&_lights)
	{
		RRReporter::report(WARN,"setLights: Invalid input, lights=NULL.\n");
		return;
	}
	for (unsigned i=0;i<_lights.size();i++)
	{
		if (!_lights[i])
		{
			RRReporter::report(WARN,"setLights: Invalid input, lights[%d]=NULL.\n",i);
			return;
		}
	}
	priv->lights = _lights;
}

const RRLights& RRSolver::getLights() const
{
	return priv->lights;
}


void RRSolver::setStaticObjects(const RRObjects& _objects, const SmoothingParameters* _smoothing, const char* _cacheLocation, RRCollider::IntersectTechnique _intersectTechnique, RRSolver* _copyFrom)
{
	// check inputs
	if (!&_objects)
	{
		RRReporter::report(WARN,"setStaticObjects: Invalid input, objects=NULL.\n");
		return;
	}
	for (unsigned i=0;i<_objects.size();i++)
	{
		if (!_objects[i])
		{
			RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d]=NULL.\n",i);
			return;
		}
		if (_objects[i]->isDynamic)
			continue;
		if (!_objects[i]->getCollider())
		{
			RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d]->getCollider()=NULL.\n",i);
			return;
		}
		const RRMesh* mesh = _objects[i]->getCollider()->getMesh();
		if (!mesh)
		{
			RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d]->getCollider()->getMesh()=NULL.\n",i);
			return;
		}
		unsigned numTriangles = mesh->getNumTriangles();
		unsigned numVertices = mesh->getNumVertices();
		if (numTriangles && numVertices)
		{
			RRMesh::Triangle t;
			mesh->getTriangle(numTriangles-1,t);
			if (mesh->getPreImportTriangle(numTriangles-1)!=RRMesh::PreImportNumber(0,numTriangles-1)
				|| mesh->getPreImportVertex(t[0],numTriangles-1)!=RRMesh::PreImportNumber(0,t[0])
				|| mesh->getPreImportVertex(t[1],numTriangles-1)!=RRMesh::PreImportNumber(0,t[1])
				|| mesh->getPreImportVertex(t[1],numTriangles-1)!=RRMesh::PreImportNumber(0,t[1])
				)
			{
				// we expect inputs with trivial preimport numbers
				// without this check, we would later crash in RRSolver::updateVertexLookupTableDynamicSolver()
				RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d] is multiobject or has e.g. vertex stitching filter applied. Fix meshes with createArrays() or createVertexBufferRuler().\n",i);
				return;
			}
		}
	}

	// update illumination environment centers, clear caches
	// it is important to clear caches in both old objects and new objects. (we used to clear only new objects. 
	//  when user checked "Dynamic", object was first removed from static, but here it was not yet in dynamic, we did not invalidate its cache,
	//  cache survived to next fireball mode, first env update used cached old triangle numbers=possibly access invalid memory)
	for (unsigned j=0;j<3;j++)
	{
		const rr::RRObjects& objects = (j==0)?getStaticObjects():((j==1)?_objects:getDynamicObjects());
		for (unsigned i=0;i<objects.size();i++)
		{
			// update envmap centers
			objects[i]->updateIlluminationEnvMapCenter();

			// clear cached triangle numbers
			objects[i]->illumination.cachedGatherSize = 0;
		}
	}

	// copy only static objects
	// don't exclude empty ones, old users may depend on object numbers, removing empties would break their numbers
	// call x.removeEmptyObjects() before setStaticObjects(x) to remove them manually
	priv->staticObjects.clear();
	for (unsigned i=0;i<_objects.size();i++)
		if (!_objects[i]->isDynamic)
			priv->staticObjects.push_back(_objects[i]);


	priv->smoothing = _copyFrom ? _copyFrom->priv->smoothing : ( _smoothing ? *_smoothing : SmoothingParameters() );

	// delete old

	priv->deleteScene();

	// create new

	RRReportInterval report(_copyFrom?INF9:INF2,"Attaching %d static objects...\n",getStaticObjects().size());

	// create multi in custom scale
	unsigned origNumVertices = 0;
	unsigned origNumTriangles = 0;
	for (unsigned i=0;i<getStaticObjects().size();i++)
	{
		origNumVertices += getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices();
		origNumTriangles += getStaticObjects()[i]->getCollider()->getMesh()->getNumTriangles();
	}
	// old smoothing code with stitching here in multiobject
	//  stitching here (it's based on positions and normals only) would corrupt uvs in indexed render
	//  we don't pass maxDistanceBetweenUvsToStitch+texcoords as parameters, therefore stitching here calls createOptimizedVertices(,,0,NULL) and different uvs are stitched
	//priv->multiObject = _copyFrom ? _copyFrom->getMultiObject() : RRObject::createMultiObject(&getStaticObjects(),_intersectTechnique,aborting,priv->smoothing.vertexWeldDistance,fabs(priv->smoothing.maxSmoothAngle),priv->smoothing.vertexWeldDistance>=0,0,_cacheLocation);
	// new smoothing stitches in Object::buildTopIVertices, no stitching here in multiobject
	priv->multiObject = _copyFrom ? _copyFrom->getMultiObject() : RRObject::createMultiObject(&getStaticObjects(),_intersectTechnique,aborting,-1,0,false,0,_cacheLocation);
	priv->forcedMultiObject = _copyFrom ? true : false;

	// convert it to physical scale
	if (!getScaler())
		RRReporter::report(WARN,"scaler=NULL, call setScaler() if your data are in sRGB.\n");
	getStaticObjects().updateColorPhysical(getScaler());

	priv->staticSolverCreationFailed = false;

	// update minimalSafeDistance
	if (priv->multiObject)
	{
		if (aborting)
		{
			priv->minimalSafeDistance = 1e-6f;
		}
		else
		{
			RRVec3 mini,maxi,center;
			priv->multiObject->getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
			priv->minimalSafeDistance = (maxi-mini).avg()*1e-6f;
		}
	}

	// update staticSceneContainsLods
	priv->staticSceneContainsLods = false;
	std::set<const RRBuffer*> allTextures;
	if (priv->multiObject)
	{
		unsigned numTrianglesMulti = priv->multiObject->getCollider()->getMesh()->getNumTriangles();
		for (unsigned t=0;t<numTrianglesMulti;t++)
		{
			if (!aborting)
			{
				const RRMaterial* material = priv->multiObject->getTriangleMaterial(t,NULL,NULL);

				RRObject::LodInfo lodInfo;
				priv->multiObject->getTriangleLod(t,lodInfo);
				if (lodInfo.level)
					priv->staticSceneContainsLods = true;

				if (material)
				{
					allTextures.insert(material->diffuseReflectance.texture);
					allTextures.insert(material->diffuseEmittance.texture);
					allTextures.insert(material->specularTransmittance.texture);
				}
			}
		}
		size_t memoryOccupiedByTextures = 0;
		for (std::set<const RRBuffer*>::iterator i=allTextures.begin();i!=allTextures.end();i++)
		{
			if (*i)
				memoryOccupiedByTextures += (*i)->getBufferBytes();
		}

		RRReporter::report(_copyFrom?INF9:INF2,"Static scene: %d obj, %d(%d) tri, %d(%d) vert, %s tex, lods %s\n",
			getStaticObjects().size(),origNumTriangles,priv->multiObject->getCollider()->getMesh()->getNumTriangles(),origNumVertices,priv->multiObject->getCollider()->getMesh()->getNumVertices(),
			RRReporter::bytesToString(memoryOccupiedByTextures),
			priv->staticSceneContainsLods?"yes":"no");
	}

	// invalidate supercollider
	priv->superColliderDirty = true;
}

const RRObjects& RRSolver::getStaticObjects() const
{
	return priv->staticObjects;
}

void RRSolver::setDynamicObjects(const RRObjects& _objects)
{
	// check inputs
	if (!&_objects)
	{
		RRReporter::report(WARN,"setDynamicObjects: Invalid input, objects=NULL.\n");
		return;
	}
	for (unsigned i=0;i<_objects.size();i++)
	{
		if (!_objects[i])
		{
			RRReporter::report(WARN,"setDynamicObjects: Invalid input, objects[%d]=NULL.\n",i);
			return;
		}
		if (!_objects[i]->getCollider())
		{
			RRReporter::report(WARN,"setDynamicObjects: Invalid input, objects[%d]->getCollider()=NULL.\n",i);
			return;
		}
		if (!_objects[i]->getCollider()->getMesh())
		{
			RRReporter::report(WARN,"setDynamicObjects: Invalid input, objects[%d]->getCollider()->getMesh()=NULL.\n",i);
			return;
		}
	}
	// copy only dynamic objects
	// don't exclude empty ones, they can arbitrarily change in future, user can send empty one to solver and fill it later
	priv->dynamicObjects.clear();
	for (unsigned i=0;i<_objects.size();i++)
		if (_objects[i]->isDynamic)
			priv->dynamicObjects.push_back(_objects[i]);
	// convert it to physical scale
	if (!getScaler())
		RRReporter::report(WARN,"scaler=NULL, call setScaler() if your data are in sRGB.\n");
	getDynamicObjects().updateColorPhysical(getScaler());
	// invalidate supercollider
	priv->superColliderDirty = true;
}

const RRObjects& RRSolver::getDynamicObjects() const
{
	return priv->dynamicObjects;
}

RRObjects RRSolver::getObjects() const
{
	rr::RRObjects allObjects = getStaticObjects();
	allObjects.insert(allObjects.end(),getDynamicObjects().begin(),getDynamicObjects().end());
	return allObjects;
}

RRObject* RRSolver::getObject(unsigned index) const
{
	if (index<getStaticObjects().size())
		return getStaticObjects()[index];
	index -= getStaticObjects().size();
	if (index<getDynamicObjects().size())
		return getDynamicObjects()[index];
	return NULL;
}

RRCollider* RRSolver::getCollider() const
{
	if (!priv->dynamicObjects.size() && getMultiObject())
		return getMultiObject()->getCollider();

	// check dynamic mesh versions (static meshes are not allowed to change), invalidate supercollider if they did change
	unsigned superColliderMeshVersion = 0;
	for (unsigned i=0;i<getDynamicObjects().size();i++)
	{
		const RRMeshArrays* arrays = dynamic_cast<const RRMeshArrays*>(getDynamicObjects()[i]->getCollider()->getMesh());
		if (arrays)
			superColliderMeshVersion += arrays->version;
	}
	if (priv->superColliderMeshVersion!=superColliderMeshVersion)
	{
		priv->superColliderMeshVersion = superColliderMeshVersion;
		// invalidate supercollider
		priv->superColliderDirty = true;
	}

	// build supercollider if it does not exist yet
	if (!priv->superCollider || priv->superColliderDirty)
	{
		priv->superColliderObjects.clear();
		if (getMultiObject())
			priv->superColliderObjects.push_back(getMultiObject());
		//priv->superColliderObjects.insert(priv->superColliderObjects.end(),priv->staticObjects.begin(),priv->staticObjects.end());
		priv->superColliderObjects.insert(priv->superColliderObjects.end(),priv->dynamicObjects.begin(),priv->dynamicObjects.end());
		delete priv->superCollider;
		bool aborting = false;
		priv->superCollider = RRCollider::create(NULL,&priv->superColliderObjects,RRCollider::IT_BVH_FAST,aborting);
		priv->superColliderDirty = false;
		
		// update superColliderMin/Max/Center
		unsigned numVerticesSum = 0;
		unsigned numPlanesSkipped = 0;
		priv->superColliderMin = RRVec3(1e37f);
		priv->superColliderMax = RRVec3(-1e37f);
		priv->superColliderCenter = RRVec3(0);
		priv->superColliderPlane = NULL;
		if (getMultiObject())
		{
			getMultiObject()->getCollider()->getMesh()->getAABB(&priv->superColliderMin,&priv->superColliderMax,&priv->superColliderCenter);
			numVerticesSum = getMultiObject()->getCollider()->getMesh()->getNumVertices();
			priv->superColliderCenter *= (RRReal)numVerticesSum;
		}
		for (unsigned testPlanes=0;testPlanes<2;testPlanes++) // first pass=all but planes, second pass=only planes
		{
			for (unsigned i=0;i<priv->dynamicObjects.size();i++)
			{
				const RRObject* object = priv->dynamicObjects[i];
				const RRMesh* mesh = object->getCollider()->getMesh();
				RRVec3 mini,maxi,center;
				mesh->getAABB(&mini,&maxi,&center);
				RRVec3 size(maxi-mini);
				if (testPlanes && size.mini())
				{
					// do nothing (non-plane that was already processed in first pass)
				}
				else
				if (testPlanes || size.mini())
				{
					// process bbox (non-planes in first pass, planes in second pass)
					for (unsigned j=0;j<8;j++)
					{
						RRVec3 bbPoint((j&1)?maxi.x:mini.x,(j&2)?maxi.y:mini.y,(j&4)?maxi.z:mini.z);
						object->getWorldMatrixRef().transformPosition(bbPoint);
						for (unsigned a=0;a<3;a++)
						{
							priv->superColliderMin[a] = RR_MIN(priv->superColliderMin[a],bbPoint[a]);
							priv->superColliderMax[a] = RR_MAX(priv->superColliderMax[a],bbPoint[a]);
						}
					}
					object->getWorldMatrixRef().transformPosition(center);
					unsigned numVertices = mesh->getNumVertices();
					numVerticesSum += numVertices;
					priv->superColliderCenter += center*(RRReal)numVertices;
				}
				else
				{
					// (planes in first pass)
					priv->superColliderPlane = object;
					numPlanesSkipped++;
				}
			}
			// skip second pass if we already found non-plane geometry
			if (getMultiObject() || numPlanesSkipped<priv->dynamicObjects.size())
				break;
		}
		priv->superColliderCenter /= (RRReal)numVerticesSum;
	}

	return priv->superCollider;
}

const RRObject* RRSolver::getAABB(RRVec3* _mini, RRVec3* _maxi, RRVec3* _center) const
{
	if (!priv->dynamicObjects.size())
	{
		if (getMultiObject())
		{
			getMultiObject()->getCollider()->getMesh()->getAABB(_mini,_maxi,_center);
		}
		else
		{
			if (_mini) *_mini = RRVec3(0);
			if (_maxi) *_maxi = RRVec3(0);
			if (_center) *_center = RRVec3(0);
		}
		return 0;
	}
	else
	{
		// values precalculated in getCollider()
		getCollider();
		if (_mini) *_mini = priv->superColliderMin;
		if (_maxi) *_maxi = priv->superColliderMax;
		if (_center) *_center = priv->superColliderCenter;
		return priv->superColliderPlane;
	}
}

void RRSolver::getAllBuffers(RRVector<RRBuffer*>& _buffers, const RRVector<unsigned>* _layers) const
{
	if (!this)
		return;
	typedef std::unordered_set<RRBuffer*> Set;
	Set set;
	// fill set
	// - original contents of vector
	for (unsigned i=0;i<_buffers.size();i++)
		set.insert(_buffers[i]);
	// - maps from lights
	for (unsigned i=0;i<getLights().size();i++)
		set.insert(getLights()[i]->rtProjectedTexture);
	// - maps from materials
	/* shorter but slower (more allocations)
	RRMaterials materials;
	getObjects().getAllMaterials(materials);
	for (int i=0;i<materials.size();i++)
		if (materials[i])
		{
			set.insert(materials[i]->diffuseReflectance.texture);
			set.insert(materials[i]->specularReflectance.texture);
			set.insert(materials[i]->diffuseEmittance.texture);
			set.insert(materials[i]->specularTransmittance.texture);
			set.insert(materials[i]->bumpMap.texture);
		}
	*/
	const RRObjects& objects = getDynamicObjects();
	for (int i=-1;i<(int)objects.size();i++)
	{
		const RRObject* object = (i==-1)?getMultiObject():objects[i];
		if (object)
		{
			const RRObject::FaceGroups& faceGroups = object->faceGroups;
			for (unsigned g=0;g<faceGroups.size();g++)
			{
				RRMaterial* m = faceGroups[g].material;
				if (m)
				{
					set.insert(m->diffuseReflectance.texture);
					set.insert(m->specularReflectance.texture);
					set.insert(m->diffuseEmittance.texture);
					set.insert(m->specularTransmittance.texture);
					set.insert(m->bumpMap.texture);
				}
			}
		}
	}
	// - environment
	set.insert(getEnvironment());
	// - illumination layers
	if (_layers)
	{
		for (unsigned k=0;k<2;k++)
		{
			const RRObjects& objects = k?getDynamicObjects():getStaticObjects();
			for (unsigned i=0;i<objects.size();i++)
				for (unsigned j=0;j<_layers->size();j++)
					set.insert(objects[i]->illumination.getLayer((*_layers)[j]));
		}
	}
	// copy set back to vector
	_buffers.clear();
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			_buffers.push_back(*i);
}

RRObject* RRSolver::getMultiObject() const
{
	return priv->multiObject;
}

bool RRSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRVec3& out) const
{
	if (priv->packedSolver)
	{
		return priv->packedSolver->getTriangleMeasure(triangle,vertex,measure,priv->scaler,out);
	}
	else
	if (priv->scene)
	{
		return priv->scene->getTriangleMeasure(triangle,vertex,measure,priv->scaler,out);
	}
	return false;
}

void RRSolver::reportMaterialChange(bool dirtyShadows, bool dirtyGI)
{
	REPORT(RRReporter::report(INF1,"<MaterialChange>\n"));
	// here we dirty shadowmaps and DDI
	//  we dirty DDI only if shadows are dirtied too, DDI with unchanged shadows would lead to the same GI
	if (dirtyShadows)
		reportDirectIlluminationChange(-1,dirtyShadows,dirtyGI,false);
	// here we dirty factors in solver
	if (dirtyGI)
	{
		//if (priv->packedSolver)
		//{
		//	// don't set dirtyMaterials, it would switch to architect solver and probably confuse user
		//	RR_LIMITED_TIMES(1,RRReporter::report(INF2,"To make material change affect indirect light, switch to Architect solver or rebuild Fireball.\n"));
		//}
		priv->dirtyMaterials = true;
	}
}

void RRSolver::reportDirectIlluminationChange(int lightIndex, bool dirtyShadows, bool dirtyGI, bool dirtyRange)
{
	REPORT(RRReporter::report(INF1,"<IlluminationChange>\n"));
	// invalidate supercollider (-1=geometry change, supercollider has precalculated inverse matrices inside, needs rebuild)
	if (lightIndex==-1)
		priv->superColliderDirty = true;
}

void RRSolver::reportInteraction()
{
	REPORT(RRReporter::report(INF1,"<Interaction>\n"));
	priv->lastInteractionTime.setNow();
}

void RRSolver::setDirectIllumination(const unsigned* directIllumination)
{
	if (priv->customIrradianceRGBA8 || directIllumination)
	{
		priv->customIrradianceRGBA8 = directIllumination;
		priv->dirtyCustomIrradiance = true;
		priv->readingResultsPeriodSeconds = READING_RESULTS_PERIOD_MIN;
		priv->readingResultsPeriodSteps = 0;
	}
}

const unsigned* RRSolver::getDirectIllumination()
{
	return priv->customIrradianceRGBA8;
}

void RRSolver::setDirectIlluminationBoost(RRReal boost)
{
	if (priv->boostCustomIrradiance != boost)
	{
		priv->boostCustomIrradiance = boost;
		setScaler(getScaler()); // update customToPhysical[] byte->float conversion table
	}
}

void RRSolver::calculateDirtyLights(CalculateParameters* _params)
{
	// replace NULL by default parameters
	static CalculateParameters s_params;
	if (!_params) _params = &s_params;

	if (_params->materialTransmittanceStaticQuality || _params->materialTransmittanceVideoQuality)
	{
		//RRReportInterval report(INF3,"Updating illumination passing through transparent materials...\n");

		// potential optimization: quit if there is zero lights with castShadows && !dirtySM && !dirtyGI
		// there tends to be much fewer lights than materials, but the condition is rarely true, speedup is unclear

		unsigned versionSum[2] = {0,0}; // 0=static, 1=video
#if 0
		// detect texture changes only in static objects
		RRObject* object = getMultiObject();
		if (object)
		{
			for (unsigned g=0;g<object->faceGroups.size();g++)
			{
				const RRMaterial* material = object->faceGroups[g].material;
				if (material && material->specularTransmittance.texture)
					versionSum[material->specularTransmittance.texture->getDuration()?1:0] += material->specularTransmittance.texture->version;
			}
		}
#else
		// detect texture changes also in dynamic objects
		// this is bit dangerous, as adding dynobj changes versionSum, which triggers expensive SM and GI update, it often but not always is what user expects
		const RRObjects& dynobjects = getDynamicObjects();
		for (int i=-1;i<(int)dynobjects.size();i++)
		{
			RRObject* object = (i<0)?getMultiObject():dynobjects[i];
			if (object)
			{
				for (unsigned g=0;g<object->faceGroups.size();g++)
				{
					const RRMaterial* material = object->faceGroups[g].material;
					if (material && material->specularTransmittance.texture)
							versionSum[material->specularTransmittance.texture->getDuration()?1:0] += material->specularTransmittance.texture->version;
				}
			}
		}
#endif
		unsigned materialTransmittanceQuality = RR_MAX( (priv->materialTransmittanceVersionSum[0]!=versionSum[0])?_params->materialTransmittanceStaticQuality:0, (priv->materialTransmittanceVersionSum[1]!=versionSum[1])?_params->materialTransmittanceVideoQuality:0 );
		if (materialTransmittanceQuality)
		{
			if (priv->materialTransmittanceVersionSum[1]!=versionSum[1] && _params->materialTransmittanceVideoQuality>1)
				priv->lastGIDirtyBecauseOfVideoTime.setNow();
			priv->materialTransmittanceVersionSum[0] = versionSum[0];
			priv->materialTransmittanceVersionSum[1] = versionSum[1];
			reportDirectIlluminationChange(-1,materialTransmittanceQuality>0,materialTransmittanceQuality>1,false);
		}
	}
}

class EndByTime : public RRStaticSolver::EndFunc
{
public:
	virtual bool requestsEnd()
	{
		return endTime.secondsPassed()>0 || *aborting;
	}
	virtual bool requestsRealtimeResponse()
	{
		// used by architect solver, go for realtime responsiveness, slower calculation
		return true;
	}
	bool* aborting;
	RRTime endTime;
};

// calculates radiosity in existing times (improveStep = seconds to spend in improving),
//  does no timing adjustments
void RRSolver::calculateCore(float improveStep,CalculateParameters* _params)
{
	if (!getMultiObject()) return;

	// replace NULL by default parameters
	static CalculateParameters s_params;
	if (!_params) _params = &s_params;

	bool dirtyFactors = false;
	bool forceEmittanceReload = false;
	if (priv->dirtyMaterials)
	{
		priv->dirtyMaterials = false;
		if (priv->packedSolver)
			forceEmittanceReload = true;
		else
			dirtyFactors = true;
		//RR_SAFE_DELETE(priv->packedSolver); intentionally not deleted, material change is not expected to unload packed solver (even though it becomes incorrect)
	}
	if (!priv->scene && !priv->staticSolverCreationFailed
		&& !priv->packedSolver
		)
	{
		REPORT(RRReportInterval report(INF3,"Opening new radiosity solver...\n"));
		dirtyFactors = true;
		// create new
		priv->scene = RRStaticSolver::create(priv->multiObject,&priv->smoothing,aborting);
		if (!priv->scene && !aborting) priv->staticSolverCreationFailed = true; // set after failure so that we don't try again
		if (priv->scene) updateVertexLookupTableDynamicSolver();
		if (aborting) RR_SAFE_DELETE(priv->scene); // this is fundamental structure, so when aborted, try to create it fully next time
	}
	if (dirtyFactors)
	{
		dirtyFactors = false;
		priv->dirtyCustomIrradiance = false;
		REPORT(RRReportInterval report(INF3,"Resetting solver energies and factors...\n"));
		RR_SAFE_DELETE(priv->packedSolver);
		if (priv->scene)
		{
			priv->scene->illuminationReset(true,true,_params->materialEmittanceMultiplier,priv->customIrradianceRGBA8,priv->customToPhysical,NULL);
		}
		priv->solutionVersion++;
		priv->readingResultsPeriodSteps = 0;
	}

	if (priv->packedSolver)
	{
		{
			RRReportInterval report(INF3,"Updating illumination from emissive materials...\n");
			// loads emittance from materials to fireball
			//!!! videa nejsou updatnuta, sampluju minuly snimek
			if (priv->packedSolver->setMaterialEmittance(forceEmittanceReload,_params->materialEmittanceMultiplier,_params->materialEmittanceStaticQuality,_params->materialEmittanceVideoQuality,_params->materialEmittanceUsePointMaterials,getScaler()))
			{
				// Fireball::illuminationReset() must be called to start using new emittance
				priv->dirtyCustomIrradiance = true;
				if (_params->materialEmittanceVideoQuality) // rarely might be dirty because of static image change. worst case scenario = additional GI improves delayed 1 sec
					priv->lastGIDirtyBecauseOfVideoTime.setNow();
			}
		}

		{
			RRReportInterval report(INF3,"Updating illumination from environment...\n");
			// loads environment to fireball
			//!!! videa nejsou updatnuta, sampluju minuly snimek
			if (priv->packedSolver->setEnvironment(priv->environment0,priv->environment1,_params->environmentStaticQuality,_params->environmentVideoQuality,priv->environmentBlendFactor,getScaler()))
			{
				priv->solutionVersion++;
				if (_params->environmentVideoQuality) // rarely might be dirty because of static image change. worst case scenario = additional GI improves delayed 1 sec
					priv->lastGIDirtyBecauseOfVideoTime.setNow();
			}
		}
	}
	else
	if (priv->scene)
	{
		// architect: when materialEmittanceMultiplier changes, enforce illuminationReset()
		if (_params->materialEmittanceMultiplier!=priv->scene->materialEmittanceMultiplier)
		{
			priv->dirtyCustomIrradiance = true;
		}
	}

	if (priv->dirtyCustomIrradiance)
	{
		REPORT(RRReportInterval report(INF3,"Updating solver energies...\n"));
		if (priv->packedSolver)
		{
			priv->packedSolver->illuminationReset(priv->customIrradianceRGBA8,priv->customToPhysical);
		}
		else
		if (priv->scene)
		{
			priv->scene->illuminationReset(false,true,_params->materialEmittanceMultiplier,priv->customIrradianceRGBA8,priv->customToPhysical,NULL);
		}
		priv->solutionVersion++;
		priv->readingResultsPeriodSteps = 0;
		// following improvement should be so big that single frames after big reset are not visibly darker
		// so...calculate at least 20ms?
		improveStep = RR_MAX(improveStep,IMPROVE_STEP_MIN_AFTER_BIG_RESET);
		priv->dirtyCustomIrradiance = false;
	}

	REPORT(RRReportInterval report(INF3,"Radiosity...\n"));
	RRTime now;
	if (priv->packedSolver)
	{
		unsigned oldVer = priv->packedSolver->getSolutionVersion();

		// when video affects GI, we must avoid additional improves (video 15fps + our window 30fps = every odd frame would be more improved, brighter)
		bool giAffectedByVideo = now.secondsSince(priv->lastGIDirtyBecauseOfVideoTime)<1;

		priv->packedSolver->illuminationImprove(_params->qualityIndirectDynamic,giAffectedByVideo?_params->qualityIndirectDynamic:_params->qualityIndirectStatic);
		if (priv->packedSolver->getSolutionVersion()>oldVer)
		{
			// dirtyResults++ -> solutionVersion will increment in a few miliseconds -> user will update lightmaps and redraw scene
			priv->dirtyResults++;
		}
	}
	else
	if (priv->scene)
	{
		EndByTime endByTime;
		endByTime.aborting = &aborting;
		endByTime.endTime.addSeconds(improveStep);
		if (priv->scene->illuminationImprove(endByTime)==RRStaticSolver::IMPROVED)
		{
			// dirtyResults++ -> solutionVersion will increment in a few miliseconds -> user will update lightmaps and redraw scene
			priv->dirtyResults++;
		}
	}
	//REPORT(RRReporter::report(INF3,"imp %d det+res+read %d game %d\n",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));

	if (priv->dirtyResults>priv->readingResultsPeriodSteps && now.secondsSince(priv->lastReadingResultsTime)>=priv->readingResultsPeriodSeconds)
	{
		priv->lastReadingResultsTime = now;
		if (priv->readingResultsPeriodSeconds<READING_RESULTS_PERIOD_MAX) priv->readingResultsPeriodSeconds *= READING_RESULTS_PERIOD_GROWTH;
		priv->readingResultsPeriodSteps++;
		priv->dirtyResults = 0;
		priv->solutionVersion++;
	}
}

// adjusts timing, does no radiosity calculation (but calls calculateCore that does)
void RRSolver::calculate(CalculateParameters* _params)
{
	RRTime calcBeginTime;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastInteractionTime*1.0f,lastCalcEndTime*1.0f);

	// adjust userStep
	float lastUserStep = calcBeginTime.secondsSince(priv->lastCalcEndTime);
	if (!lastUserStep) lastUserStep = 0.00001f; // fight with low timer precision, avoid 0, initial userStep=0 means 'unknown yet' which forces too long improve (IMPROVE_STEP_NO_INTERACTION)
	if (priv->lastInteractionTime.secondsSince(priv->lastCalcEndTime)>=0)
	{
		// reportInteraction was called between this and previous calculate
		if (lastUserStep<1.0f)
		{
			priv->userStep = lastUserStep;
		}
		//REPORT(RRReporter::report(INF1,"User %d ms.\n",(int)(1000*lastUserStep)));
	} else {
		// no reportInteraction was called between this and previous calculate
		// -> increase userStep
		//    (this slowly increases calculation time)
		priv->userStep = priv->lastCalcEndTime.secondsSince(priv->lastInteractionTime);
		//REPORT(RRReporter::report(INF1,"User %d ms (accumulated to %d).\n",(int)(1000*lastUserStep),(int)(1000*priv->userStep)));
	}

	// adjust improveStep
	if (!priv->userStep || !priv->calcStep || !priv->improveStep)
	{
		REPORT(RRReporter::report(INF1,"Reset to NO_INTERACT(%f,%f,%f).\n",priv->userStep,priv->calcStep,priv->improveStep));
		priv->improveStep = IMPROVE_STEP_NO_INTERACTION;
	}
	else
	{		
		// try to balance our (calculate) time and user time 1:1
		priv->improveStep *= 
			// kdyz byl userTime 1000 a nahle spadne na 1 (protoze user hmatnul na klavesy),
			// toto zajisti rychly pad improveStep z 80 na male hodnoty zajistujici plynuly render,
			// takze napr. rozjezd kamery nezacne strasnym cukanim
			(priv->calcStep>6*priv->userStep)?0.4f: 
			// standardni vyvazovani s tlumenim prudkych vykyvu
			( (priv->calcStep>priv->userStep)?0.8f:1.2f );
		// always spend at least 40% of our time in improve, don't spend too much by reading results etc
		priv->improveStep = RR_MAX(priv->improveStep,MIN_PORTION_SPENT_IMPROVING*priv->calcStep);
		// stick in interactive bounds
		priv->improveStep = RR_CLAMP(priv->improveStep,IMPROVE_STEP_MIN,IMPROVE_STEP_MAX);
	}

	// calculate
	calculateCore(priv->improveStep,_params);

	// adjust calcStep
	priv->lastCalcEndTime.setNow();
	float lastCalcStep = priv->lastCalcEndTime.secondsSince(calcBeginTime);
	if (!lastCalcStep) lastCalcStep = 0.00001f; // fight low timer precision, avoid 0, initial calcStep=0 means 'unknown yet'
	if (lastCalcStep<1.0)
	{
		if (!priv->calcStep)
			priv->calcStep = lastCalcStep;
		else
			priv->calcStep = 0.6f*priv->calcStep + 0.4f*lastCalcStep;
	}

	if (_params)
		priv->previousCalculateParameters = *_params;
}


/////////////////////////////////////////////////////////////////////////////
//
// misc

void RRSolver::checkConsistency()
{
	RRReporter::report(INF1,"Solver diagnose:\n");
	if (!getMultiObject())
	{
		RRReporter::report(WARN,"  No static objects in solver, see setStaticObjects().\n");
		return;
	}
	if (!getLights().size())
	{
		RRReporter::report(WARN,"  No lights in solver, see setLights().\n");
	}
	const unsigned* detected = priv->customIrradianceRGBA8;
	if (!detected)
	{
		RRReporter::report(WARN,"  setDirectIllumination() not called yet (or called with NULL), no realtime lighting.\n");
		return;
	}

	if (!priv->scene&&priv->packedSolver) RRReporter::report(INF1,"  Solver type: Fireball\n"); else
		if (priv->scene&&!priv->packedSolver) RRReporter::report(INF1,"  Solver type: Architect\n"); else
		if (!priv->scene&&!priv->packedSolver) RRReporter::report(WARN,"  Solver type: none\n"); else
		if (priv->scene&&priv->packedSolver) RRReporter::report(WARN,"  Solver type: both\n");

	// boost
	if (priv->boostCustomIrradiance<=0.1f || priv->boostCustomIrradiance>=10)
	{
		RRReporter::report(WARN,"  setDirectIlluminationBoost(%f) was called, is it intentional? Scene may get too %s.\n",
			priv->boostCustomIrradiance,
			(priv->boostCustomIrradiance<=0.1f)?"dark":"bright");
	}

	// histogram
	unsigned numTriangles = getMultiObject()->getCollider()->getMesh()->getNumTriangles();
	unsigned numLit = 0;
	unsigned histo[256][3];
	memset(histo,0,sizeof(histo));
	for (unsigned i=0;i<numTriangles;i++)
	{
		histo[(detected[i]>> 0)&255][0]++;
		histo[(detected[i]>> 8)&255][1]++;
		histo[(detected[i]>>16)&255][2]++;
		if (detected[i]&0xffffff) numLit++;
	}
	if (histo[0][0]+histo[0][1]+histo[0][2]==3*numTriangles)
	{
		RRReporter::report(WARN,"  setDirectIllumination() was called with array of zeros, no lights in scene?\n");
		return;
	}
	RRReporter::report(INF1,"  Triangles with realtime direct illumination: %d/%d.\n",numLit,numTriangles);

	// average irradiance
	unsigned avg[3] = {0,0,0};
	for (unsigned i=0;i<256;i++)
		for (unsigned j=0;j<3;j++)
			avg[j] += histo[i][j]*i;
	for (unsigned j=0;j<3;j++)
		avg[j] /= numTriangles;
	RRReporter::report(INF1,"  Average realtime direct irradiance: %d %d %d.\n",avg[0],avg[1],avg[2]);

	// scaler
	RRReal avgPhys = 0;
	unsigned hist3[3] = {0,0,0};
	for (unsigned i=0;i<256;i++)
	{
		hist3[(priv->customToPhysical[i]<0)?0:((priv->customToPhysical[i]==0)?1:2)]++;
		avgPhys += priv->customToPhysical[i];
	}
	avgPhys /= 256;
	if (hist3[0]||hist3[1]!=1)
	{
		RRReporter::report(WARN,"  Wrong scaler set, see setScaler().\n");
		RRReporter::report(WARN,"    Scaling to negative/zero/positive result: %d/%d/%d (should be 0/1/255).\n",hist3[0],hist3[1],hist3[2]);
	}
	RRReporter::report(INF1,"  Scaling from custom scale 0..255 to average physical scale %f.\n",avgPhys);
}

unsigned RRSolver::getSolutionVersion() const
{
	return priv->solutionVersion;
}

RRSolver::InternalSolverType RRSolver::getInternalSolverType() const
{
	if (priv->scene)
		return priv->packedSolver?BOTH:ARCHITECT;
	return priv->packedSolver?FIREBALL:NONE;
}

bool RRSolver::containsLightSource() const
{
	for (unsigned i=0;i<getLights().size();i++)
		if (getLights()[i] && getLights()[i]->enabled)
			return true;
	return getEnvironment()
		|| (getMultiObject() && getMultiObject()->faceGroups.containsEmittance());
}

bool RRSolver::containsRealtimeGILightSource() const
{
	for (unsigned i=0;i<getLights().size();i++)
		if (getLights()[i] && getLights()[i]->enabled)
			return true;
	return (getEnvironment() && getInternalSolverType()==FIREBALL) // Fireball calculates skybox realtime GI, Architect does not
		|| (getMultiObject() && getMultiObject()->faceGroups.containsEmittance());
}

void RRSolver::allocateBuffersForRealtimeGI(int layerLightmap, int layerEnvironment, unsigned diffuseEnvMapSize, unsigned specularEnvMapSize, unsigned refractEnvMapSize, bool allocateNewBuffers, bool changeExistingBuffers, float specularThreshold, float depthThreshold) const
{
	// allocate vertex buffers (don't touch cubes)
	if (layerLightmap>=0)
	{
		if (getMultiObject())
		{
			getStaticObjects().allocateBuffersForRealtimeGI(layerLightmap,-1,0,0,0,allocateNewBuffers,changeExistingBuffers,specularThreshold,depthThreshold); // -1=don't touch cubes (0 would delete them, next allocateBuffersForRealtimeGI would recreate them)
			RRObjectIllumination& multiIllumination = getMultiObject()->illumination;
			if (!multiIllumination.getLayer(layerLightmap))
				multiIllumination.getLayer(layerLightmap) =
					RRBuffer::create(BT_VERTEX_BUFFER,getMultiObject()->getCollider()->getMesh()->getNumVertices(),1,1,BF_RGBF,false,NULL); // [multiobj indir is indexed]
		}
		for (unsigned i=0;i<getDynamicObjects().size();i++)
		{
			// delete unwanted vertex buffers sometimes hanging around, e.g. after changing object from static to dynamic
			// if we don't (and user doesn't), dynamic object would use old vertex colors for diffuse reflections
			RRBuffer*& buffer = getDynamicObjects()[i]->illumination.getLayer(layerLightmap);
			RR_SAFE_DELETE(buffer);
		}
	}
	// allocate cube maps (don't touch vertex buffers)
	if (layerEnvironment>=0)
	{
		// don't allocate cubes for static objects that only need diffuse reflection [#27]
		getStaticObjects().allocateBuffersForRealtimeGI(-1,layerEnvironment,0,specularEnvMapSize,refractEnvMapSize,allocateNewBuffers,changeExistingBuffers,specularThreshold,depthThreshold);
		getDynamicObjects().allocateBuffersForRealtimeGI(-1,layerEnvironment,diffuseEnvMapSize,specularEnvMapSize,refractEnvMapSize,allocateNewBuffers,changeExistingBuffers,specularThreshold,depthThreshold);
	}
}

void RRSolver::pathTraceFrame(RRCamera& _camera, RRBuffer* _frame, unsigned _accumulated, const PathTracingParameters& _parameters)
{
	if (!_frame)
		return;
	//RRReportInterval report(INF2,"Pathtracing frame...\n");
	unsigned w = _frame->getWidth();
	unsigned h = _frame->getHeight();
	const RRCollider* collider = getCollider();
	PathtracerJob ptj(this);
#pragma omp parallel for schedule(dynamic)
	for (int j=0;j<(int)h;j++)
	{
		Gatherer gatherer(ptj,this,_parameters,true,false,UINT_MAX);
		gatherer.ray.rayLengthMin = priv->minimalSafeDistance; // necessary, e.g. 2011_BMW_5_series_F10_535_i_v1.1.rr3
		gatherer.ray.rayLengthMax = 1e10f;
		for (unsigned i=0;i<w;i++)
		{
			unsigned index = i+j*w;
			RRVec4 c = _frame->getElement(index);
			//for (int sy=0; sy<2; sy++)
			//for (int sx=0; sx<2; sx++)
			//for (int s=0; s<2; s++)
			{
				float r1=rand()*(2.f/RAND_MAX), dx=r1<1 ? sqrtf(r1)-1: 1-sqrtf(2-r1);
				float r2=rand()*(2.f/RAND_MAX), dy=r2<1 ? sqrtf(r2)-1: 1-sqrtf(2-r2);
				//RRVec2 positionInWindow((sx+.5+dx+2*i)/w-1,1-(sy+.5+dy+2*j)/h);
				RRVec2 positionInWindow(2*(dx+i)/w-1,2*(dy+j)/h-1);
				RRVec3 color = gatherer.getIncidentRadiance(_camera.getRayOrigin(positionInWindow),_camera.getRayDirection(positionInWindow).normalized(),NULL,UINT_MAX);
				c = (c*RRReal(_accumulated)+RRVec4(color,0))/(_accumulated+1);
			}
			_frame->setElement(index,c);
		}
	}
}

unsigned RR_INTERFACE_ID_LIB()
{
	return RR_INTERFACE_ID_APP();
}

const char* RR_INTERFACE_DESC_LIB()
{
	return RR_INTERFACE_DESC_APP();
}

} // namespace
