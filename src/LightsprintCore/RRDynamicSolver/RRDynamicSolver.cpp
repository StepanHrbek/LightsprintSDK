// --------------------------------------------------------------------------
// Dynamic GI solver.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <set>
#include "Lightsprint/RRDynamicSolver.h"
#include "report.h"
#include "private.h"
#include "../RRStaticSolver/rrcore.h" // build of packed factors
#include <boost/unordered_set.hpp>

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


RRDynamicSolver::RRDynamicSolver()
{
	aborting = false;
	priv = new Private;
}

RRDynamicSolver::~RRDynamicSolver()
{
	delete priv;
}

void RRDynamicSolver::setScaler(const RRScaler* _scaler)
{
	priv->scaler = _scaler;
	// update fast conversion table for our setDirectIllumination
	for (unsigned i=0;i<256;i++)
	{
		RRVec3 c(i*priv->boostCustomIrradiance/255);
		if (_scaler) _scaler->getPhysicalScale(c);
		priv->customToPhysical[i] = c[0];
	}
}

const RRScaler* RRDynamicSolver::getScaler() const
{
	return priv->scaler;
}

void RRDynamicSolver::setEnvironment(RRBuffer* _environment0, RRBuffer* _environment1)
{
	priv->environment0 = _environment0;
	priv->environment1 = _environment1;
	if (priv->packedSolver)
	{
		priv->packedSolver->setEnvironment(_environment0,_environment1,priv->environmentBlendFactor,getScaler());
	}
	// temporary diagnostic
	RRReporter::report(INF2,"******* setEnvironment(), env=0x%x,0x%x, bl=%f, fb=0x%x\n",priv->environment0,priv->environment1,priv->environmentBlendFactor,priv->packedSolver);
	// affects everything in fireball
	// affects only specular cubemaps in architect
	priv->solutionVersion++;
}

RRBuffer* RRDynamicSolver::getEnvironment(unsigned _environmentIndex) const
{
	return _environmentIndex ? priv->environment1 : priv->environment0;
}

void RRDynamicSolver::setEnvironmentBlendFactor(float _blendFactor)
{
	priv->environmentBlendFactor = _blendFactor;
	if (priv->packedSolver)
	{
		priv->packedSolver->setEnvironmentBlendFactor(_blendFactor);
	}
	// temporary diagnostic
	RRReporter::report(INF2,"******* setEnvironmentBlendFactor(), env=0x%x,0x%x, bl=%f, fb=0x%x\n",priv->environment0,priv->environment1,priv->environmentBlendFactor,priv->packedSolver);
	// affects everything in fireball
	// affects only specular cubemaps in architect
	priv->solutionVersion++;
}

float RRDynamicSolver::getEnvironmentBlendFactor() const
{
	return priv->environmentBlendFactor;
}

void RRDynamicSolver::setLights(const RRLights& _lights)
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

const RRLights& RRDynamicSolver::getLights() const
{
	return priv->lights;
}

void RRDynamicSolver::setEmittance(float emissiveMultiplier, unsigned quality, bool usePointMaterials)
{
	if (priv->packedSolver)
	{
		// loads emittance from materials to fireball
		priv->packedSolver->setEmittance(emissiveMultiplier,quality,usePointMaterials,getScaler());
		// next calculateCore() will call Fireball::illuminationReset(), to start using new emittance
		priv->dirtyCustomIrradiance = true;
	}
}

void RRDynamicSolver::setStaticObjects(const RRObjects& _objects, const SmoothingParameters* _smoothing, const char* _cacheLocation, RRCollider::IntersectTechnique _intersectTechnique, RRDynamicSolver* _copyFrom)
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
		if (!_objects[i]->getCollider())
		{
			RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d]->getCollider()=NULL.\n",i);
			return;
		}
		if (!_objects[i]->getCollider()->getMesh())
		{
			RRReporter::report(WARN,"setStaticObjects: Invalid input, objects[%d]->getCollider()->getMesh()=NULL.\n",i);
			return;
		}
	}

	priv->staticObjects = _objects;
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
	//priv->multiObjectCustom = _copyFrom ? _copyFrom->getMultiObjectCustom() : RRObject::createMultiObject(&getStaticObjects(),_intersectTechnique,aborting,priv->smoothing.vertexWeldDistance,fabs(priv->smoothing.maxSmoothAngle),priv->smoothing.vertexWeldDistance>=0,0,_cacheLocation);
	// new smoothing stitches in Object::buildTopIVertices, no stitching here in multiobject
	//  stitching here (it's based on positions and normals only) would corrupt uvs in indexed render
	priv->multiObjectCustom = _copyFrom ? _copyFrom->getMultiObjectCustom() : RRObject::createMultiObject(&getStaticObjects(),_intersectTechnique,aborting,-1,0,false,0,_cacheLocation);
	priv->forcedMultiObjectCustom = _copyFrom ? true : false;

	// convert it to physical scale
	if (!getScaler())
		RRReporter::report(WARN,"scaler=NULL, call setScaler() if your data are in sRGB.\n");
	priv->multiObjectPhysical = (priv->multiObjectCustom) ? priv->multiObjectCustom->createObjectWithPhysicalMaterials(getScaler(),aborting) : NULL; // no scaler -> physical==custom
	priv->staticSolverCreationFailed = false;

	// update minimalSafeDistance
	if (priv->multiObjectCustom)
	{
		if (aborting)
		{
			priv->minimalSafeDistance = 1e-6f;
		}
		else
		{
			RRVec3 mini,maxi,center;
			priv->multiObjectCustom->getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
			priv->minimalSafeDistance = (maxi-mini).avg()*1e-6f;
		}
	}

	// update staticSceneContainsLods
	priv->staticSceneContainsLods = false;
	std::set<const RRBuffer*> allTextures;
	if (priv->multiObjectCustom)
	{
		unsigned numTrianglesMulti = priv->multiObjectCustom->getCollider()->getMesh()->getNumTriangles();
		for (unsigned t=0;t<numTrianglesMulti;t++)
		{
			if (!aborting)
			{
				const RRMaterial* material = priv->multiObjectCustom->getTriangleMaterial(t,NULL,NULL);

				RRObject::LodInfo lodInfo;
				priv->multiObjectCustom->getTriangleLod(t,lodInfo);
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

		RRReporter::report(_copyFrom?INF9:INF2,"Static scene: %d obj, %d(%d) tri, %d(%d) vert, %dMB tex, lods %s\n",
			getStaticObjects().size(),origNumTriangles,priv->multiObjectCustom->getCollider()->getMesh()->getNumTriangles(),origNumVertices,priv->multiObjectCustom->getCollider()->getMesh()->getNumVertices(),
			unsigned(memoryOccupiedByTextures>>20),
			priv->staticSceneContainsLods?"yes":"no");
	}
}

const RRObjects& RRDynamicSolver::getStaticObjects() const
{
	return priv->staticObjects;
}

void RRDynamicSolver::setDynamicObjects(const RRObjects& _objects)
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

	priv->dynamicObjects = _objects;
}

const RRObjects& RRDynamicSolver::getDynamicObjects() const
{
	return priv->dynamicObjects;
}

void RRDynamicSolver::getAllBuffers(RRVector<RRBuffer*>& buffers) const
{
	typedef boost::unordered_set<RRBuffer*> Set;
	Set set;
	// fill set
	for (unsigned i=0;i<buffers.size();i++)
		set.insert(buffers[i]);
	for (unsigned i=0;i<getLights().size();i++)
		set.insert(getLights()[i]->rtProjectedTexture);
	if (getMultiObjectCustom())
	{
		RRObject::FaceGroups& faceGroups = getMultiObjectCustom()->faceGroups;
		for (unsigned g=0;g<faceGroups.size();g++)
		{
			RRMaterial* m = faceGroups[g].material;
			if (m)
			{
				set.insert(m->diffuseReflectance.texture);
				set.insert(m->specularReflectance.texture);
				set.insert(m->diffuseEmittance.texture);
				set.insert(m->specularTransmittance.texture);
			}
		}
	}
	set.insert(getEnvironment());
	// copy set to buffers
	buffers.clear();
	for (Set::const_iterator i=set.begin();i!=set.end();++i)
		if (*i)
			buffers.push_back(*i);
}

RRObject* RRDynamicSolver::getMultiObjectCustom() const
{
	return priv->multiObjectCustom;
}

const RRObject* RRDynamicSolver::getMultiObjectPhysical() const
{
	return priv->multiObjectPhysical;
}

bool RRDynamicSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRVec3& out) const
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

void RRDynamicSolver::reportMaterialChange(bool dirtyShadows, bool dirtyGI)
{
	REPORT(RRReporter::report(INF1,"<MaterialChange>\n"));
	// here we dirty shadowmaps and DDI
	//  we dirty DDI only if shadows are dirtied too, DDI with unchanged shadows would lead to the same GI
	if (dirtyShadows)
		reportDirectIlluminationChange(-1,dirtyShadows,dirtyGI);
	// here we dirty factors in solver
	if (dirtyGI)
	{
		if (priv->packedSolver)
		{
			// don't set dirtyMaterials, it would switch to architect solver and probably confuse user
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::INF2,"To make material change affect indirect light, switch to Architect solver or rebuild Fireball.\n"));
		}
		else
		{
			priv->dirtyMaterials = true;
		}
	}
	//if (priv->multiObjectPhysical) priv->multiObjectPhysical->update(aborting);
}

void RRDynamicSolver::reportDirectIlluminationChange(int lightIndex, bool dirtyShadows, bool dirtyGI)
{
	REPORT(RRReporter::report(INF1,"<IlluminationChange>\n"));
}

void RRDynamicSolver::reportInteraction()
{
	REPORT(RRReporter::report(INF1,"<Interaction>\n"));
	priv->lastInteractionTime = GETTIME;
}

void RRDynamicSolver::setDirectIllumination(const unsigned* directIllumination)
{
	if (priv->customIrradianceRGBA8 || directIllumination)
	{
		priv->customIrradianceRGBA8 = directIllumination;
		priv->dirtyCustomIrradiance = true;
		priv->readingResultsPeriodSeconds = READING_RESULTS_PERIOD_MIN;
		priv->readingResultsPeriodSteps = 0;
	}
}

void RRDynamicSolver::setDirectIlluminationBoost(RRReal boost)
{
	if (priv->boostCustomIrradiance != boost)
	{
		priv->boostCustomIrradiance = boost;
		setScaler(getScaler()); // update customToPhysical[] byte->float conversion table
	}
}

class EndByTime : public RRStaticSolver::EndFunc
{
public:
	virtual bool requestsEnd()
	{
		#if PER_SEC==1
			// floating point time without overflows
			return (GETTIME>endTime) || *aborting;
		#else
			// fixed point time with overlaps
			TIME now = GETTIME;
			TIME end = endTime;
			TIME max = (TIME)(end+ULONG_MAX/2);
			return ( end<now && now<max ) || ( now<max && max<end ) || ( max<end && end<now ) || *aborting;
		#endif
	}
	virtual bool requestsRealtimeResponse()
	{
		// used by architect solver, go for realtime responsiveness, slower calculation
		return true;
	}
	bool* aborting;
	TIME endTime;
};

template <class C, int I, int InitialNoSmooth>
class Smoother
{
public:
	Smoother()
	{
		loaded = 0;
		i = 0;
		toSkip = InitialNoSmooth;
	}
	C smooth(C c)
	{
		if (toSkip)
		{
			toSkip--;
			return c;
		}
		if (loaded<I) loaded++;
		history[i] = c;
		i++; i%=I;
		C avg = 0;
		for (unsigned i=0;i<loaded;i++) avg += history[i];
		return avg/loaded;
	}
private:
	C history[I];
	unsigned loaded;
	unsigned i;
	unsigned toSkip;
};

// calculates radiosity in existing times (improveStep = seconds to spend in improving),
//  does no timing adjustments
void RRDynamicSolver::calculateCore(float improveStep,CalculateParameters* _params)
{
	if (!getMultiObjectCustom()) return;

	// replace NULL by default parameters
	static CalculateParameters s_params;
	if (!_params) _params = &s_params;

	bool dirtyFactors = false;
	if (priv->dirtyMaterials)
	{
		priv->dirtyMaterials = false;
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
		priv->scene = RRStaticSolver::create(priv->multiObjectPhysical,&priv->smoothing,aborting);
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
			priv->scene->illuminationReset(true,true,priv->customIrradianceRGBA8,priv->customToPhysical,NULL);
		}
		priv->solutionVersion++;
		priv->readingResultsPeriodSteps = 0;
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
			priv->scene->illuminationReset(false,true,priv->customIrradianceRGBA8,priv->customToPhysical,NULL);
		}
		priv->solutionVersion++;
		priv->readingResultsPeriodSteps = 0;
		// following improvement should be so big that single frames after big reset are not visibly darker
		// so...calculate at least 20ms?
		improveStep = RR_MAX(improveStep,IMPROVE_STEP_MIN_AFTER_BIG_RESET);
		priv->dirtyCustomIrradiance = false;
	}

	REPORT(RRReportInterval report(INF3,"Radiosity...\n"));
	TIME now = GETTIME;
	if (priv->packedSolver)
	{
		unsigned oldVer = priv->packedSolver->getSolutionVersion();
		priv->packedSolver->illuminationImprove(_params->qualityIndirectDynamic,_params->qualityIndirectStatic);
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
		endByTime.endTime = (TIME)(now+improveStep*PER_SEC);
		if (priv->scene->illuminationImprove(endByTime)==RRStaticSolver::IMPROVED)
		{
			// dirtyResults++ -> solutionVersion will increment in a few miliseconds -> user will update lightmaps and redraw scene
			priv->dirtyResults++;
		}
	}
	//REPORT(RRReporter::report(INF3,"imp %d det+res+read %d game %d\n",(int)(1000*improveStep),(int)(1000*calcStep-improveStep),(int)(1000*userStep)));

#if 0
	// show speed
	static Smoother<unsigned,20,3> s1,s2,s3;
	TIME thisFrameEnd = GETTIME;
	static TIME prevFrameEnd = 0;
	if (prevFrameEnd)
		printf("##### frame time %d+%d=%dms #####\n",s1.smooth((now - prevFrameEnd)*1000/PER_SEC),s2.smooth((thisFrameEnd - now)*1000/PER_SEC),s3.smooth((thisFrameEnd - prevFrameEnd)*1000/PER_SEC));
	prevFrameEnd = thisFrameEnd;
	reportDirectIlluminationChange(true);
#endif

	if (priv->dirtyResults>priv->readingResultsPeriodSteps && now>=(TIME)(priv->lastReadingResultsTime+priv->readingResultsPeriodSeconds*PER_SEC))
	{
		priv->lastReadingResultsTime = now;
		if (priv->readingResultsPeriodSeconds<READING_RESULTS_PERIOD_MAX) priv->readingResultsPeriodSeconds *= READING_RESULTS_PERIOD_GROWTH;
		priv->readingResultsPeriodSteps++;
		priv->dirtyResults = 0;
		priv->solutionVersion++;
	}
}

// adjusts timing, does no radiosity calculation (but calls calculateCore that does)
void RRDynamicSolver::calculate(CalculateParameters* _params)
{
	TIME calcBeginTime = GETTIME;
	//printf("%f %f %f\n",calcBeginTime*1.0f,lastInteractionTime*1.0f,lastCalcEndTime*1.0f);

	// adjust userStep
	float lastUserStep = (float)((calcBeginTime-priv->lastCalcEndTime)/(float)PER_SEC);
	if (!lastUserStep) lastUserStep = 0.00001f; // fight with low timer precision, avoid 0, initial userStep=0 means 'unknown yet' which forces too long improve (IMPROVE_STEP_NO_INTERACTION)
	if (priv->lastInteractionTime && priv->lastInteractionTime>=priv->lastCalcEndTime)
	{
		// reportInteraction was called between this and previous calculate
		if (priv->lastCalcEndTime && lastUserStep<1.0f)
		{
			priv->userStep = lastUserStep;
		}
		//REPORT(RRReporter::report(INF1,"User %d ms.\n",(int)(1000*lastUserStep)));
	} else {
		// no reportInteraction was called between this and previous calculate
		// -> increase userStep
		//    (this slowly increases calculation time)
		priv->userStep = priv->lastInteractionTime ?
			(float)((priv->lastCalcEndTime-priv->lastInteractionTime)/(float)PER_SEC) // time from last interaction (if there was at least one)
			: IMPROVE_STEP_NO_INTERACTION; // safety time for situations there was no interaction yet
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
	priv->lastCalcEndTime = GETTIME;
	float lastCalcStep = (float)((priv->lastCalcEndTime-calcBeginTime)/(float)PER_SEC);
	if (!lastCalcStep) lastCalcStep = 0.00001f; // fight low timer precision, avoid 0, initial calcStep=0 means 'unknown yet'
	if (lastCalcStep<1.0)
	{
		if (!priv->calcStep)
			priv->calcStep = lastCalcStep;
		else
			priv->calcStep = 0.6f*priv->calcStep + 0.4f*lastCalcStep;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// misc

void RRDynamicSolver::checkConsistency()
{
	RRReporter::report(INF1,"Solver diagnose:\n");
	if (!getMultiObjectCustom())
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
	unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
	unsigned numLit = 0;
	unsigned histo[256][3];
	memset(histo,0,sizeof(histo));
	for (unsigned i=0;i<numTriangles;i++)
	{
		histo[detected[i]>>24][0]++;
		histo[(detected[i]>>16)&255][1]++;
		histo[(detected[i]>>8)&255][2]++;
		if (detected[i]>>8) numLit++;
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

unsigned RRDynamicSolver::getSolutionVersion() const
{
	return priv->solutionVersion;
}

RRDynamicSolver::InternalSolverType RRDynamicSolver::getInternalSolverType() const
{
	if (priv->scene)
		return priv->packedSolver?BOTH:ARCHITECT;
	return priv->packedSolver?FIREBALL:NONE;
}

bool RRDynamicSolver::containsLightSource() const
{
	for (unsigned i=0;i<getLights().size();i++)
		if (getLights()[i] && getLights()[i]->enabled)
			return true;
	return getEnvironment()
		|| (getMultiObjectCustom() && getMultiObjectCustom()->faceGroups.containsEmittance());
}

bool RRDynamicSolver::containsRealtimeGILightSource() const
{
	for (unsigned i=0;i<getLights().size();i++)
		if (getLights()[i] && getLights()[i]->enabled)
			return true;
	return (getEnvironment() && getInternalSolverType()==FIREBALL) // Fireball calculates skybox realtime GI, Architect does not
		|| (getMultiObjectCustom() && getMultiObjectCustom()->faceGroups.containsEmittance());
}

void RRDynamicSolver::allocateBuffersForRealtimeGI(int lightmapLayerNumber, int diffuseCubeSize, int specularCubeSize, int gatherCubeSize, bool allocateNewBuffers, bool changeExistingBuffers) const
{
	// allocate vertex buffers
	if (lightmapLayerNumber>=0 && getMultiObjectCustom())
	{
		getStaticObjects().allocateBuffersForRealtimeGI(lightmapLayerNumber,0,0,-1,allocateNewBuffers,changeExistingBuffers);
		RRObjectIllumination& multiIllumination = getMultiObjectCustom()->illumination;
		if (!multiIllumination.getLayer(lightmapLayerNumber))
			multiIllumination.getLayer(lightmapLayerNumber) =
				RRBuffer::create(rr::BT_VERTEX_BUFFER,getMultiObjectCustom()->getCollider()->getMesh()->getNumVertices(),1,1,BF_RGBF,false,NULL); // [multiobj indir is indexed]
	}
	// allocate cube maps
	if (diffuseCubeSize>=0 || specularCubeSize>=0)
	{
		getStaticObjects().allocateBuffersForRealtimeGI(-1,0,specularCubeSize,gatherCubeSize,allocateNewBuffers,changeExistingBuffers);
		getDynamicObjects().allocateBuffersForRealtimeGI(-1,diffuseCubeSize,specularCubeSize,gatherCubeSize,allocateNewBuffers,changeExistingBuffers);
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
