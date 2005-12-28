/*
prozatim odsunuto z headeru:

// get intersection
typedef       bool INTERSECT(rrCollider::RRRay*);
typedef       ObjectHandle ENUM_OBJECTS(rrCollider::RRRay*, INTERSECT);
void          setObjectEnumerator(ENUM_OBJECTS enumerator);
bool          intersect(rrCollider::RRRay* ray);

void          sceneSetColorFilter(const RRReal* colorFilter);

*/

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>

#include "LicGenOpt.h"
#include "rrcore.h"
#include "../MeshImporter/Filter.h"
#include "RRVision.h"

namespace rrVision
{

#define DBG(a) //a
#define scene ((Scene*)_scene)


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter

void RRObjectImporter::getTriangleNormals(unsigned t, TriangleNormals& out)
{
	rrCollider::RRMeshImporter::TriangleBody tb;
	getCollider()->getImporter()->getTriangleBody(t,tb);
	Vec3 norm = ortogonalTo(*(Vec3*)&tb.side1,*(Vec3*)&tb.side2);
	norm *= 1/size(norm);
	*(Vec3*)(&out.norm[0]) = norm;
	*(Vec3*)(&out.norm[1]) = norm;
	*(Vec3*)(&out.norm[2]) = norm;
}

void RRObjectImporter::getTriangleMapping(unsigned t, TriangleMapping& out)
{
	unsigned numTriangles = getCollider()->getImporter()->getNumTriangles();
	out.uv[0].m[0] = 1.0f*t/numTriangles;
	out.uv[0].m[1] = 0;
	out.uv[1].m[0] = 1.0f*(t+1)/numTriangles;
	out.uv[1].m[1] = 0;
	out.uv[2].m[0] = 1.0f*t/numTriangles;
	out.uv[2].m[1] = 1;
}


//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshFilter : public rrCollider::RRMeshFilter
{
public:
	RRTransformedMeshFilter(RRMeshImporter* mesh, const RRMatrix4x4* matrix)
		: RRMeshFilter(mesh)
	{
		m = matrix;
	}
	/*RRTransformedMeshFilter(RRObjectImporter* object)
	: RRMeshFilter(object->getCollider()->getImporter())
	{
	m = object->getWorldMatrix();
	}*/

	virtual void getVertex(unsigned v, Vertex& out) const
	{
		importer->getVertex(v,out);
		if(m)
		{
			Vertex tmp;
			tmp[0] = m->m[0][0] * out[0] + m->m[1][0] * out[1] + m->m[2][0] * out[2] + m->m[3][0];
			tmp[1] = m->m[0][1] * out[0] + m->m[1][1] * out[1] + m->m[2][1] * out[2] + m->m[3][1];
			tmp[2] = m->m[0][2] * out[0] + m->m[1][2] * out[1] + m->m[2][2] * out[2] + m->m[3][2];
			out = tmp;
		}
	}

private:
	const RRMatrix4x4* m;
};

//////////////////////////////////////////////////////////////////////////////
//
// Merges multiple object importers together.
// Instructions for deleting
//   ObjectImporters typically get underlying Collider as constructor parameter.
//   They are thus not allowed to destroy it (see general rules).
//   MultiObjectImporter creates Collider internally. To behave as others, it doesn't destroy it.
// Limitations:
//   All objects must share one surface numbering.

class RRMultiObjectImporter : public RRObjectImporter
{
public:
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, char* cacheLocation)
	{
		// only in top level of hierarchy: create multicollider
		rrCollider::RRCollider* multiCollider = NULL;
		rrCollider::RRMeshImporter** transformedMeshes = NULL;
		if(numObjects>1)
		{
			// create multimesh
			transformedMeshes = new rrCollider::RRMeshImporter*[numObjects+2];
				//!!! pri getWorldMatrix()==NULL by se misto WorldSpaceMeshe mohl pouzit original a pak ho neuvolnovat
			for(unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
			transformedMeshes[numObjects] = NULL;
			transformedMeshes[numObjects+1] = NULL;
			rrCollider::RRMeshImporter* multiMesh = rrCollider::RRMeshImporter::createMultiMesh(transformedMeshes,numObjects);
			// stitch vertices
			if(maxStitchDistance>=0) 
			{
				transformedMeshes[numObjects] = multiMesh; // remember for freeing time
				multiMesh = multiMesh->createOptimizedVertices(maxStitchDistance);
			}
			// create copy (faster access)
			// disabled because we know that current copy implementation always gives up
			// due to low efficiency
			if(0)
			{
				rrCollider::RRMeshImporter* tmp = multiMesh->createCopy();
				if(tmp)
				{
					transformedMeshes[numObjects+1] = multiMesh; // remember for freeing time
					multiMesh = tmp;
				}
			}

			// create multicollider
			multiCollider = rrCollider::RRCollider::create(multiMesh,intersectTechnique,cacheLocation);
		}

		// creates tree of objects
		return create(objects,numObjects,multiCollider,transformedMeshes);
	}

	virtual const rrCollider::RRCollider* getCollider() const
	{
		return multiCollider;
	}

	virtual unsigned     getTriangleSurface(unsigned t) const
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleSurface(t);
		return pack[1].getImporter()->getTriangleSurface(t-pack[0].getNumTriangles());
	}
	virtual RRSurface*   getSurface(unsigned s)
	{
		// assumption: all objects share the same surface library
		// -> this is not universal code
		return pack[0].getImporter()->getSurface(s);
	}

	virtual const RRColor* getTriangleAdditionalRadiantExitance(unsigned t) const 
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalRadiantExitance(t);
		return pack[1].getImporter()->getTriangleAdditionalRadiantExitance(t-pack[0].getNumTriangles());
	}
	virtual const RRColor* getTriangleAdditionalRadiantExitingFlux(unsigned t) const 
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalRadiantExitingFlux(t);
		return pack[1].getImporter()->getTriangleAdditionalRadiantExitingFlux(t-pack[0].getNumTriangles());
	}

	virtual const RRMatrix4x4* getWorldMatrix()
	{
		return NULL;
	}
	virtual const RRMatrix4x4* getInvWorldMatrix()
	{
		return NULL;
	}

	virtual ~RRMultiObjectImporter()
	{
		// Never delete lowest level of tree = input importers.
		// Delete only higher levels = multi mesh importers created by our create().
		if(pack[0].getNumObjects()>1) delete pack[0].getImporter();
		if(pack[1].getNumObjects()>1) delete pack[1].getImporter();
		// Only for top level of tree:
		if(multiCollider) 
		{
			// Delete multiMesh importer created by us.
			delete multiCollider->getImporter();
			// Delete multiCollider created by us.
			delete multiCollider;
			// Delete transformers created by us.
			unsigned numObjects = pack[0].getNumObjects() + pack[1].getNumObjects();
			for(unsigned i=0;i<numObjects+2;i++) delete transformedMeshes[i];
			delete[] transformedMeshes;
		}
	}

private:
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider* multiCollider = NULL, rrCollider::RRMeshImporter** transformedMeshes = NULL)
		// All parameters (meshes, array of meshes) are destructed by caller, not by us.
		// Array of meshes must live during this call.
		// Meshes must live as long as created multimesh.
	{
		switch(numObjects)
		{
		case 0: 
			return NULL;
		case 1: 
			assert(objects);
			return objects[0];
		default: 
			assert(objects); 
			unsigned num1 = numObjects/2;
			unsigned num2 = numObjects-numObjects/2;
			unsigned tris[2] = {0,0};
			for(unsigned i=0;i<numObjects;i++) 
			{
				assert(objects[i]);
				assert(objects[i]->getCollider());
				assert(objects[i]->getCollider()->getImporter());
				tris[(i<num1)?0:1] += objects[i]->getCollider()->getImporter()->getNumTriangles();
			}

			// create multiobject
			return new RRMultiObjectImporter(
				create(objects,num1),num1,tris[0],
				create(objects+num1,num2),num2,tris[1],
				multiCollider,transformedMeshes);
		}
	}

	RRMultiObjectImporter(RRObjectImporter* mesh1, unsigned mesh1Objects, unsigned mesh1Triangles, 
		RRObjectImporter* mesh2, unsigned mesh2Objects, unsigned mesh2Triangles,
		rrCollider::RRCollider* amultiCollider, rrCollider::RRMeshImporter** atransformedMeshes)
	{
		multiCollider = amultiCollider;
		transformedMeshes = atransformedMeshes;
		pack[0].init(mesh1,mesh1Objects,mesh1Triangles);
		pack[1].init(mesh2,mesh2Objects,mesh2Triangles);
	}

	struct ObjectPack
	{
		void init(RRObjectImporter* importer, unsigned objects, unsigned triangles)
		{
			assert(numObjects);
			packImporter = importer;
			numObjects = objects;
			numTriangles = triangles;
		}
		RRObjectImporter* getImporter() const {return packImporter;}
		unsigned          getNumObjects() const {return numObjects;}
		unsigned          getNumTriangles() const {return numTriangles;}
	private:
		RRObjectImporter* packImporter;
		unsigned          numObjects;
		unsigned          numTriangles;
	};

	ObjectPack        pack[2];
	rrCollider::RRCollider* multiCollider;
	rrCollider::RRMeshImporter** transformedMeshes;
};

void RRScene::sceneFreezeGeometry(bool yes)
{
	scene->freeze(yes);
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter instance factory

rrCollider::RRMeshImporter* RRObjectImporter::createWorldSpaceMesh()
{
	return new RRTransformedMeshFilter(getCollider()->getImporter(),getWorldMatrix());
}

RRObjectImporter* RRObjectImporter::createMultiObject(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, char* cacheLocation)
{
	return RRMultiObjectImporter::create(objects,numObjects,intersectTechnique,maxStitchDistance,cacheLocation);
}



/*
1-sided vs 2-sided surface

Expected behaviour:

 Exactly as mgfdoc specifies.

 What does mgfdoc say:

 The sides entity is used to set the number of sides for the current material.
 If a surface is two-sided, then it will appear
 identical when viewed from either the front or the back.
 If a surface is one-sided,
 then it appears invisible when viewed from the back side.
 This means
 that a transmitting object will affect the light coming in through the
 front surface and ignore the characteristics of the back surface,
 unless the index of refraction is set.
 If the index of refraction is set, then the object will act as a
 solid piece of dielectric material.
 In either case, the transmission properties of the exiting surface
 should be the same as the incident surface for the model to be
 physically valid.
 The default number of sides is two.

Real behaviour:

 I'm not sure if I understand it.
 Let's independently specify our behaviour:

 1. 1-sided surface is surface of solid object from real world.
    If it has no transmittance, surface can't be hit from the inner side.
    (=There is darkness inside real world solid objects.)
    But if it happens, energy should be thrown away and ray discontinued.
    Presence of such rays may be visualised by
    -sides1outer:receive=0 -sides1inner:receive=1 -sides2outer:receive=0 -sides2inner:receive=0

    Let's imagine some possibilities:
      a) rays that hit inner side are thrown away
      b) rays that hit inner side continue as if nothing happened

    Our implementation makes a) by default.
    b) is available by -sides1inner:catch=0

 2. There is no 2-sided surface in real world.
    2-sided is probably something solid, thin object with >0 thickness.
    Or CG hack without real-world sense.
    What if we have 2-sided surface enlightened only from one side?

    Let's imagine possibilities how to handle it:
      a) dark from the other side (thin metal layer)
      b) equally enlightened from (and emiting to) both sides (thin cloth)
      c) appear equally enlightened from both sides but emit light only to outer side (CG hack)

    Our implementation makes c) by default.
    b) is available by -sides2inner:emit=1
    a) would require some coding and more memory

*/

RRSideBits sideBits[3][2]={
	{{0},{0}},
	{{1,1,1,1,1,1},{0,0,1,0,0,0}}, // definition of default 1-sided (outer side, inner side)
	{{1,1,1,1,1,1},{1,0,1,1,1,1}}, // definition of default 2-sided (outer side, inner side)
};

RRScene::RRScene()
{
	_scene=new Scene();
}

RRScene::~RRScene()
{
	delete scene;
}

RRScene::ObjectHandle RRScene::objectCreate(RRObjectImporter* importer)
{
	assert(importer);
	if(!importer) return UINT_MAX;
	rrCollider::RRMeshImporter* meshImporter = importer->getCollider()->getImporter();
	Object *obj=new Object(meshImporter->getNumVertices(),meshImporter->getNumTriangles());
	obj->importer = importer;

	// import vertices
	assert(sizeof(rrCollider::RRMeshImporter::Vertex)==sizeof(Vec3));
	for(unsigned i=0;i<obj->vertices;i++) 
		meshImporter->getVertex(i,*(rrCollider::RRMeshImporter::Vertex*)&obj->vertex[i]);

#ifdef SUPPORT_TRANSFORMS
	obj->transformMatrix=(Matrix*)importer->getWorldMatrix();
	obj->inverseMatrix=(Matrix*)importer->getInvWorldMatrix();
	// vyzada si prvni transformaci
	obj->matrixDirty=true;
#endif

	// import triangles
	// od nuly nahoru insertuje emitory, od triangles-1 dolu ostatni
	DBG(printf(" triangles...\n"));
	int tbot=0;
// bohuzel pak nesedej triangle_handly, poradi se ted uz nesmi menit
//#ifdef SUPPORT_DYNAMIC
//	int ttop=obj->triangles-1;
//#endif
	for(unsigned fi=0;fi<obj->triangles;fi++) 
	{
		rrCollider::RRMeshImporter::Triangle tv;
		meshImporter->getTriangle(fi,tv);
		unsigned si = importer->getTriangleSurface(fi);
		RRSurface* s=importer->getSurface(si);
		assert(s);
		// rozhodne zda vlozit face dolu mezi emitory nebo nahoru mezi ostatni
		Triangle *t;
//#ifdef SUPPORT_DYNAMIC
//		if(s && s->diffuseEmittance) t=&obj->triangle[tbot++]; else t=&obj->triangle[ttop--];
//#else
		t=&obj->triangle[tbot++];
//#endif
		assert(t>=obj->triangle && t<&obj->triangle[obj->triangles]);
		// vlozi ho, seridi geometrii atd
/*		rrCollider::RRMeshImporter::Vertex v[3];
		meshImporter->getVertex(tv[0],v[0]);
		meshImporter->getVertex(tv[1],v[1]);
		meshImporter->getVertex(tv[2],v[2]);*/
		int geom=t->setGeometry(
			&obj->vertex[tv[0]],
			&obj->vertex[tv[1]],
			&obj->vertex[tv[2]],
#ifdef SUPPORT_TRANSFORMS
			obj->transformMatrix
#else
			NULL
#endif
			);
		if(t->isValid) 
		{
			const RRColor* addExitingFlux=importer->getTriangleAdditionalRadiantExitingFlux(fi);
			const RRColor* addExitance=importer->getTriangleAdditionalRadiantExitance(fi);
			Vec3 sumExitance=(addExitance?*(Vec3*)addExitance:Vec3(0,0,0)) + (addExitingFlux?(*(Vec3*)addExitingFlux)/t->area:Vec3(0,0,0));
			obj->objSourceExitingFlux+=abs(t->setSurface(s,sumExitance));
		}
		else
		{
			t->surface=NULL; // marks invalid triangles
			t->area=0; // just to have consistency through all invalid triangles
		}
	}
	   
#ifdef SUPPORT_DYNAMIC
	obj->trianglesEmiting=tbot;
#endif
	// preprocessuje objekt
	DBG(printf(" bounds...\n"));
	obj->detectBounds();
	{
		DBG(printf(" edges...\n"));
		obj->buildEdges(); // build edges only for clusters and/or interpol
	}
	if(RRGetState(RRSS_USE_CLUSTERS))
	{
		DBG(printf(" clusters...\n"));
		obj->buildClusters(); 
		// clusters first, ivertices then (see comment in Cluster::insert)
	}
	DBG(printf(" ivertices...\n"));
	obj->buildTopIVertices();
	// priradi objektu jednoznacny a pri kazdem spusteni stejny identifikator
	obj->id=0;//!!!
	obj->name=NULL;
#ifdef SUPPORT_TRANSFORMS
	obj->transformBound();
#endif
	// vlozi objekt do sceny
#ifdef SUPPORT_DYNAMIC
	if (0) 
	{
		scene->objInsertDynamic(obj); 
		return -1-scene->objects;
	}
	else
#endif
	{
		scene->objInsertStatic(obj);
		return scene->objects-1;
	}
}

void RRScene::objectDestroy(ObjectHandle object)
{
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return;
	}
	scene->objRemoveStatic(object);
}

void RRScene::setSkyLight(RRSkyLight* skyLight)
{
	scene->setSkyLight(skyLight);
}

/*
void RRScene::sceneSetColorFilter(const RRReal* colorFilter)
{
	__frameNumber++;
	scene->selectColorFilter(0,colorFilter);
	scene->resetStaticIllumination(false);
	scene->distribute(0.001f);
}
*/

RRScene::Improvement RRScene::sceneResetStatic(bool resetFactors)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return FINISHED;
	__frameNumber++;
	scene->updateMatrices();
	return scene->resetStaticIllumination(resetFactors);
}

RRScene::Improvement RRScene::sceneImproveStatic(bool endfunc(void*), void* context)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return FINISHED;
	__frameNumber++;
	return scene->improveStatic(endfunc, context);
}

RRReal RRScene::sceneGetAccuracy()
{
	return scene->avgAccuracy()/100;
}

const RRColor* RRScene::getVertexIrradiance(ObjectHandle object, unsigned vertex)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return NULL;
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return NULL;
	}
	Object* obj = scene->object[object];
#if CHANNELS==1
	Channels rad = obj->getVertexIrradiance(vertex);
	static RRColor tmp;
	tmp.m[0] = rad*__colorFilter.m[0];
	tmp.m[1] = rad*__colorFilter.m[1];
	tmp.m[2] = rad*__colorFilter.m[2];
	return &tmp;
#else
	static Channels rad = obj->getVertexIrradiance(vertex);
	return (RRColor*)&rad;
#endif
}

const RRColor* RRScene::getTriangleIrradiance(ObjectHandle object, unsigned triangle, unsigned vertex)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return NULL;
	// pokus nejak kompenzovat ze jsem si ve freeze interne zrusil n objektu a nahradil je 1 multiobjektem
	//if(isFrozen())
	//{
	//	if(scene->objects!=1) 
	//	{
	//		assert(0);
	//		return NULL;
	//	}
	//	Object* obj = scene->object[0];
	//	obj->importer->getCollider()->getImporter()->get
	//}
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return NULL;
	}
	Object* obj = scene->object[object];
	if(triangle>=obj->triangles)
	{
		assert(0);
		return NULL;
	}
	Triangle* tri = &obj->triangle[triangle];
	if(!tri->surface) return NULL;
	Channels irrad;

	// enhanced by final gathering
	if(vertex<3 && RRGetState(RRSS_GET_FINAL_GATHER))
	{
		vertex=(vertex+3-tri->rotations)%3;

		Channels reflIrrad = Channels(0);
		if(RRGetState(RRSS_GET_REFLECTED))
		{
			// get normal
			RRObjectImporter* objectImporter = obj->importer;
			RRObjectImporter::TriangleNormals normals;
			objectImporter->getTriangleNormals(triangle,normals);
			Vec3 normal = *(Vec3*)(&normals.norm[vertex]);
			assert(fabs(size2(normal)-1)<0.001);
			// get point
			rrCollider::RRMeshImporter* meshImporter = objectImporter->getCollider()->getImporter();
			rrCollider::RRMeshImporter::Triangle triangleIndices;
			meshImporter->getTriangle(triangle,triangleIndices);
			unsigned vertexIdx = triangleIndices[vertex];
			rrCollider::RRMeshImporter::Vertex vertexBody;
			meshImporter->getVertex(vertexIdx,vertexBody);
			Vec3 point = *(Vec3*)&vertexBody;
			// transform to worldspace
			Matrix* world = (Matrix*)objectImporter->getWorldMatrix();
			if(world)
			{
				point.transform(world);
				normal.rotate(world);
				normal *= 1/size(normal);
				assert(fabs(size2(normal)-1)<0.001);
			}
			//!!! solved by fudge 1mm offset in gatherIrradiance. point += normal*0.001f; //!!! fudge number. to avoid immediate hits of other faces connected to vertex. could be done also by more advanced skipTriangle with list of all faces connected to vertex
			// get irradiance
			reflIrrad = scene->gatherIrradiance(point,normal,tri);
		}
		irrad = (RRGetState(RRSS_GET_SOURCE)?tri->getSourceIrradiance():Channels(0)) + reflIrrad; // irradiance in W/m^2
	}
	else

	// enhanced by smoothing
	if(vertex<3 && RRGetState(RRSS_GET_SMOOTH))
	{
		vertex=(vertex+3-tri->rotations)%3;

		Channels reflIrrad = Channels(0);
		if(RRGetState(RRSS_GET_REFLECTED))
		{
			unsigned oldSource = RRSetState(RRSS_GET_SOURCE,0);
			reflIrrad = tri->topivertex[vertex]->irradiance();
			RRSetState(RRSS_GET_SOURCE,oldSource);
		}
		irrad = (RRGetState(RRSS_GET_SOURCE)?tri->getSourceIrradiance():Channels(0)) + reflIrrad; // irradiance in W/m^2
	}
	else

	// basic, fast
	{
		if(!RRGetState(RRSS_GET_SOURCE) && !RRGetState(RRSS_GET_REFLECTED)) 
			irrad = Channels(0);
		else
		if(RRGetState(RRSS_GET_SOURCE) && !RRGetState(RRSS_GET_REFLECTED)) 
			irrad = tri->getSourceIrradiance();
		else
		if(RRGetState(RRSS_GET_SOURCE) && RRGetState(RRSS_GET_REFLECTED)) 
			irrad = (tri->energyDirectIncident + tri->getEnergyDynamic()) / tri->area;
		else
			irrad = (tri->energyDirectIncident + tri->getEnergyDynamic()) / tri->area - tri->getSourceIrradiance();
	}

	static RRColor tmp;
#if CHANNELS == 1
	tmp.m[0] = irrad*__colorFilter.m[0];
	tmp.m[1] = irrad*__colorFilter.m[1];
	tmp.m[2] = irrad*__colorFilter.m[2];
#else
	tmp.m[0] = irrad.x*__colorFilter.m[0];
	tmp.m[1] = irrad.y*__colorFilter.m[1];
	tmp.m[2] = irrad.z*__colorFilter.m[2];
#endif
	return &tmp;
}

const RRColor* RRScene::getTriangleRadiantExitance(ObjectHandle object, unsigned triangle, unsigned vertex)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return NULL;
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return NULL;
	}
	Object* obj = scene->object[object];
	if(triangle>=obj->triangles) 
	{
		assert(0);
		return NULL;
	}
	Triangle* tri = &obj->triangle[triangle];
	if(!tri->surface) return 0;

	static Vec3 tmp = *(Vec3*)getTriangleIrradiance(object,triangle,vertex) * *(Vec3*)(&tri->surface->diffuseReflectanceColor);
	return (RRColor*)&tmp.x;
}

unsigned RRScene::getPointRadiosity(unsigned n, RRScene::InstantRadiosityPoint* point)
{
	if(!licenseStatusValid || licenseStatus!=VALID) return 0;
	return scene->getInstantRadiosityPoints(n,point);
}

void RRScene::getStats(unsigned* faces, RRReal* sourceExitingFlux, unsigned* rays, RRReal* reflectedIncidentFlux) const
{
	scene->getStats(faces,sourceExitingFlux,rays,reflectedIncidentFlux);
}

void RRScene::getInfo(char* buf, unsigned type)
{
	switch(type)
	{
	case 0: scene->infoScene(buf); break;
	case 1: scene->infoImprovement(buf,2); break;
	case 2: rrCollider::RRIntersectStats::getInstance()->getInfo(buf,900,2); break;
	case 3: scene->infoStructs(buf); break;
	}
}

void* RRScene::getScene()
{
	return _scene;
}

void* RRScene::getObject(ObjectHandle object)
{
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return NULL;
	}
	return scene->object[object];
}

union StateValue
{
	unsigned u;
	real r;
};

//////////////////////////////////////////////////////////////////////////////
//
// set/get state

static StateValue RRSSValue[RRSS_LAST];

void RRResetStates()
{
	memset(RRSSValue,0,sizeof(RRSSValue));
	RRSetState(RRSS_USE_CLUSTERS,0);
	RRSetStateF(RRSSF_SUBDIVISION_SPEED,1);
	RRSetState(RRSS_GET_SOURCE,1);
	RRSetState(RRSS_GET_REFLECTED,1);
	RRSetState(RRSS_GET_SMOOTH,1);
	RRSetState(RRSS_GET_FINAL_GATHER,0);
	RRSetStateF(RRSSF_MIN_FEATURE_SIZE,0);
	RRSetStateF(RRSSF_MAX_SMOOTH_ANGLE,M_PI/10+0.01f);
	RRSetStateF(RRSSF_IGNORE_SMALLER_AREA,SMALL_REAL);
	RRSetStateF(RRSSF_IGNORE_SMALLER_ANGLE,0.001f);
	RRSetState(RRSS_FIGHT_NEEDLES,0);
	RRSetStateF(RRSSF_FIGHT_SMALLER_AREA,0.01f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_ANGLE,0.01f);

	//RRSetStateF(RRSSF_SUBDIVISION_SPEED,0);
	//RRSetStateF(RRSSF_MIN_FEATURE_SIZE,10.037f); //!!!
}

unsigned RRGetState(RRSceneState state)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValue[state].u;
}

unsigned RRSetState(RRSceneState state, unsigned value)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	unsigned tmp = RRSSValue[state].u;
	RRSSValue[state].u = value;
	return tmp;
}

real RRGetStateF(RRSceneState state)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValue[state].r;
}

real RRSetStateF(RRSceneState state, real value)
{
	if(state<0 || state>=RRSS_LAST) 
	{
		assert(0);
		return 0;
	}
	real tmp = RRSSValue[state].r;
	RRSSValue[state].r = value;
	return tmp;
}

//////////////////////////////////////////////////////////////////////////////
//
// global init/done

class RREngine
{
public:
	RREngine();
	~RREngine();
};

RREngine::RREngine()
{
	core_Init();
	RRResetStates();
}

RREngine::~RREngine()
{
	core_Done();
}

static RREngine engine;


//////////////////////////////////////////////////////////////////////////////
//
// Obfuscation

unsigned char mask[16]={138,41,237,122,49,191,254,91,34,169,59,207,152,76,187,106};

#define MASK1(str,i) (str[i]^mask[(i)%16])
#define MASK2(str,i) MASK1(str,i),MASK1(str,i+1)
#define MASK3(str,i) MASK2(str,i),MASK1(str,i+2)
#define MASK4(str,i) MASK2(str,i),MASK2(str,i+2)
#define MASK5(str,i) MASK4(str,i),MASK1(str,i+4)
#define MASK6(str,i) MASK4(str,i),MASK2(str,i+4)
#define MASK7(str,i) MASK4(str,i),MASK3(str,i+4)
#define MASK8(str,i) MASK4(str,i),MASK4(str,i+4)
#define MASK10(str,i) MASK8(str,i),MASK2(str,i+8)
#define MASK12(str,i) MASK8(str,i),MASK4(str,i+8)
#define MASK14(str,i) MASK8(str,i),MASK6(str,i+8)
#define MASK16(str,i) MASK8(str,i),MASK8(str,i+8)
#define MASK18(str,i) MASK16(str,i),MASK2(str,i+16)
#define MASK32(str,i) MASK16(str,i),MASK16(str,i+16)
#define MASK44(str,i) MASK32(str,i),MASK12(str,i+32)
#define MASK64(str,i) MASK32(str,i),MASK32(str,i+32)
#define MASK108(str,i) MASK64(str,i),MASK44(str,i+64)

const char* unmask(const char* str,char* buf)
{
	for(unsigned i=0;;i++)
	{
		char c = str[i] ^ mask[i%16];
		buf[i] = c;
		if(!c) break;
	}
	return buf;
}


//////////////////////////////////////////////////////////////////////////////
//
// License


LicenseStatus registerLicense(char* licenseOwner, char* licenseNumber)
{
	licenseStatusValid = true;
	licenseStatus = VALID;
	return VALID;
}

} // namespace
