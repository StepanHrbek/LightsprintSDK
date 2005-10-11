#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include "rrcore.h"
#include "RREngine.h"

/*
#ifdef ONE
#include "geometry.cpp"
#include "rrcore.cpp"
#include "intersections.cpp"
#ifdef SUPPORT_INTERPOL
#include "interpol.cpp"
#endif
#ifdef SUPPORT_DYNAMIC
#include "dynamic.cpp"
#endif
#else
#include "geometry.h"
#include "rrcore.h"
#include "intersections.h"
#ifdef SUPPORT_INTERPOL
#include "interpol.h"
#endif
#ifdef SUPPORT_DYNAMIC
#include "dynamic.h"
#endif
#endif
*/


namespace rrEngine
{

#define DBG(a) //a
#define scene ((Scene*)_scene)


//////////////////////////////////////////////////////////////////////////////
//
// Base class for mesh import filters.

class RRFilteredMeshImporter : public rrCollider::RRMeshImporter
{
public:
	RRFilteredMeshImporter(const RRMeshImporter* mesh)
	{
		importer = mesh;
		numVertices = importer ? importer->getNumVertices() : 0;
		numTriangles = importer ? importer->getNumTriangles() : 0;
	}
	virtual ~RRFilteredMeshImporter()
	{
		// Delete only what was created by us, not params.
		// So don't delete importer.
	}

	// vertices
	virtual unsigned     getNumVertices() const
	{
		return numVertices;
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		importer->getVertex(v,out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return numTriangles;
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		importer->getTriangle(t,out);
	}

	// preimport/postimport conversions
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		return importer->getPreImportVertex(postImportVertex, postImportTriangle);
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		return importer->getPostImportVertex(preImportVertex, preImportTriangle);
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		return importer->getPreImportTriangle(postImportTriangle);
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		return importer->getPostImportTriangle(preImportTriangle);
	}

protected:
	const RRMeshImporter* importer;
	unsigned        numVertices;
	unsigned        numTriangles;
};


//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshImporter : public RRFilteredMeshImporter
{
public:
	RRTransformedMeshImporter(RRMeshImporter* mesh, const RRReal* matrix)
		: RRFilteredMeshImporter(mesh)
	{
		m = matrix;
	}
	/*RRTransformedMeshImporter(RRObjectImporter* object)
	: RRFilteredMeshImporter(object->getCollider()->getImporter())
	{
	m = object->getWorldMatrix();
	}*/

	virtual void getVertex(unsigned v, Vertex& out) const
	{
		importer->getVertex(v,out);
		if(m)
		{
			Vertex tmp;
			tmp[0] = m[0] * out[0] + m[4] * out[1] + m[ 8] * out[2] + m[12];
			tmp[1] = m[1] * out[0] + m[5] * out[1] + m[ 9] * out[2] + m[13];
			tmp[2] = m[2] * out[0] + m[6] * out[1] + m[10] * out[2] + m[14];
			out = tmp;
		}
	}

private:
	const RRReal* m;
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
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects)
	{
		return create(objects,numObjects,true);
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

	virtual const RRReal* getTriangleAdditionalRadiantExitance(unsigned t) const 
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalRadiantExitance(t);
		return pack[1].getImporter()->getTriangleAdditionalRadiantExitance(t-pack[0].getNumTriangles());
	}
	virtual const RRReal* getTriangleAdditionalRadiantExitingFlux(unsigned t) const 
	{
		if(t<pack[0].getNumTriangles()) return pack[0].getImporter()->getTriangleAdditionalRadiantExitingFlux(t);
		return pack[1].getImporter()->getTriangleAdditionalRadiantExitingFlux(t-pack[0].getNumTriangles());
	}

	virtual const RRReal* getWorldMatrix()
	{
		return NULL;
	}
	virtual const RRReal* getInvWorldMatrix()
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
			for(unsigned i=0;i<numObjects;i++) delete transformedMeshes[i];
			delete[] transformedMeshes;
		}
	}

private:
	static RRObjectImporter* create(RRObjectImporter* const* objects, unsigned numObjects, bool createCollider)
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

			// only in top level of hierarchy: create multicollider
			rrCollider::RRCollider* multiCollider = NULL;
			rrCollider::RRMeshImporter** transformedMeshes = NULL;
			if(createCollider)
			{
				// create multimesh
				transformedMeshes = new rrCollider::RRMeshImporter*[numObjects];
				for(unsigned i=0;i<numObjects;i++) transformedMeshes[i] = objects[i]->createWorldSpaceMesh();
				rrCollider::RRMeshImporter* multiMesh = rrCollider::RRMeshImporter::createMultiMesh(transformedMeshes,numObjects);

				// create multicollider
				multiCollider = rrCollider::RRCollider::create(multiMesh,objects[0]->getCollider()->getTechnique());
			}

			// create multiobject
			return new RRMultiObjectImporter(
				create(objects,num1,false),num1,tris[0],
				create(objects+num1,num2,false),num2,tris[1],
				multiCollider,
				transformedMeshes);
		}
	}

	RRMultiObjectImporter(RRObjectImporter* mesh1, unsigned mesh1Objects, unsigned mesh1Triangles, 
		RRObjectImporter* mesh2, unsigned mesh2Objects, unsigned mesh2Triangles, 
		rrCollider::RRCollider* collider, rrCollider::RRMeshImporter** transformers)
	{
		multiCollider = collider;
		transformedMeshes = transformers;
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


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectImporter instance factory

rrCollider::RRMeshImporter* RRObjectImporter::createWorldSpaceMesh()
{
	return new RRTransformedMeshImporter(getCollider()->getImporter(),getWorldMatrix());
}

RRObjectImporter* RRObjectImporter::createMultiObject(RRObjectImporter* const* objects, unsigned numObjects)
{
	return RRMultiObjectImporter::create(objects,numObjects);
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
			const real* addExitingFlux=importer->getTriangleAdditionalRadiantExitingFlux(fi);
			const real* addExitance=importer->getTriangleAdditionalRadiantExitance(fi);
			Vec3 sumExitance=(addExitance?*(Vec3*)addExitance:Vec3(0,0,0)) + (addExitingFlux?(*(Vec3*)addExitingFlux)/t->area:Vec3(0,0,0));
			obj->objSourceExitingFlux+=abs(t->setSurface(s,sumExitance));
		}
		else
			t->surface=NULL;
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
	assert(object<scene->objects);
	scene->objRemoveStatic(object);
}

void RRScene::sceneSetColorFilter(const RRReal* colorFilter)
{
	__frameNumber++;
	scene->selectColorFilter(0,colorFilter);
	scene->resetStaticIllumination(false);
	scene->distribute(0.001f);
}

RRScene::Improvement RRScene::sceneResetStatic(bool resetFactors)
{
	__frameNumber++;
	scene->updateMatrices();
	return scene->resetStaticIllumination(resetFactors);
}

RRScene::Improvement RRScene::sceneImproveStatic(bool endfunc(void*), void* context)
{
	__frameNumber++;
	return scene->improveStatic(endfunc, context);
}

void RRScene::compact()
{
}

const RRReal* RRScene::getVertexIrradiance(ObjectHandle object, unsigned vertex)
{
	assert(object<scene->objects);
	Object* obj = scene->object[object];
#if CHANNELS==1
	Channels rad = obj->getVertexIrradiance(vertex);
	static RRColor tmp;
	tmp[0] = rad*__colorFilter[0];
	tmp[1] = rad*__colorFilter[1];
	tmp[2] = rad*__colorFilter[2];
	return tmp;
#else
	static Channels rad = obj->getVertexIrradiance(vertex);
	return (RRReal*)&rad;
#endif
}

const RRReal* RRScene::getTriangleIrradiance(ObjectHandle object, unsigned triangle, unsigned vertex)
{
	assert(object<scene->objects);
	Object* obj = scene->object[object];
	assert(triangle<obj->triangles);
	Triangle* tri = &obj->triangle[triangle];
	if(!tri->surface) return 0;
	//if(vertex>=3) return (tri->energyDirect + tri->getEnergyDynamic()) / tri->area;
	vertex=(vertex+3-tri->rotations)%3;

	Channels reflIrrad = Channels(0);
	if(RRGetState(RRSS_GET_REFLECTED))
	{
		unsigned oldSource = RRSetState(RRSS_GET_SOURCE,0);
		reflIrrad = tri->topivertex[vertex]->irradiance();
		RRSetState(RRSS_GET_SOURCE,oldSource);
	}
	Channels irrad = (RRGetState(RRSS_GET_SOURCE)?tri->getSourceIrradiance():Channels(0)) + reflIrrad; // irradiance in W/m^2
	static RRColor tmp;
#if CHANNELS == 1
	tmp[0] = irrad*__colorFilter[0];
	tmp[1] = irrad*__colorFilter[1];
	tmp[2] = irrad*__colorFilter[2];
#else
	tmp[0] = irrad.x*__colorFilter[0];
	tmp[1] = irrad.y*__colorFilter[1];
	tmp[2] = irrad.z*__colorFilter[2];
#endif
	return tmp;
}

const RRReal* RRScene::getTriangleRadiantExitance(ObjectHandle object, unsigned triangle, unsigned vertex)
{
	assert(object<scene->objects);
	Object* obj = scene->object[object];
	assert(triangle<obj->triangles);
	Triangle* tri = &obj->triangle[triangle];
	if(!tri->surface) return 0;

	static Vec3 tmp = *(Vec3*)getTriangleIrradiance(object,triangle,vertex) * *(Vec3*)(tri->surface->diffuseReflectanceColor);
	return &tmp.x;
}

unsigned RRScene::getPointRadiosity(unsigned n, RRScene::InstantRadiosityPoint* point)
{
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
	assert(object<scene->objects);
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
	RRSetState(RRSS_INTERSECT_TECHNIQUE,rrCollider::RRCollider::IT_BSP_FASTEST);
	RRSetStateF(RRSSF_IGNORE_SMALLER_AREA,SMALL_REAL);
	RRSetStateF(RRSSF_IGNORE_SMALLER_ANGLE,0.001f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_AREA,0.01f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_ANGLE,0.01f);
	RRSetStateF(RRSSF_MIN_FEATURE_SIZE,0);

	//RRSetStateF(RRSSF_SUBDIVISION_SPEED,0);
	//RRSetStateF(RRSSF_MIN_FEATURE_SIZE,10.037f); //!!!
}

unsigned RRGetState(RRSceneState state)
{
	assert(state>=0 && state<RRSS_LAST);
	return RRSSValue[state].u;
}

unsigned RRSetState(RRSceneState state, unsigned value)
{
	assert(state>=0 && state<RRSS_LAST);
	unsigned tmp = RRSSValue[state].u;
	RRSSValue[state].u = value;
	return tmp;
}

real RRGetStateF(RRSceneState state)
{
	assert(state>=0 && state<RRSS_LAST);
	return RRSSValue[state].r;
}

real RRSetStateF(RRSceneState state, real value)
{
	assert(state>=0 && state<RRSS_LAST);
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

} // namespace
