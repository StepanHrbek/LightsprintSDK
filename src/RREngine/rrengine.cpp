#include <assert.h>
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

RRScene::ObjectHandle RRScene::objectCreate(RRSceneObjectImporter* importer)
{
	assert(importer);
	if(!importer) return UINT_MAX;
	Object *obj=new Object(importer->getNumVertices(),importer->getNumTriangles());
	obj->importer = importer;

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
	for (unsigned fi=0;fi<obj->triangles;fi++) {
		unsigned v0,v1,v2;
		importer->getTriangle(fi,v0,v1,v2);
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
		int geom=t->setGeometry(
			(Vec3*)(importer->getVertex(v0)),
			(Vec3*)(importer->getVertex(v1)),
			(Vec3*)(importer->getVertex(v2)),
			obj->transformMatrix);
		if(t->isValid) 
		{
			const real* addExitingFlux=importer->getTriangleAdditionalRadiantExitingFlux(fi);
			const real* addExitance=importer->getTriangleAdditionalRadiantExitance(fi);
			Vec3 sumExitance=(addExitance?*(Vec3*)addExitance:Vec3(0,0,0)) + (addExitingFlux?(*(Vec3*)addExitingFlux)/t->area:Vec3(0,0,0));
			obj->energyEmited+=abs(t->setSurface(s,sumExitance));
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
	// bsp tree
	DBG(printf(" tree...\n"));
	obj->intersector = rrIntersect::RRIntersect::create(importer,(rrIntersect::RRIntersect::IntersectTechnique)RRGetState(RRSS_INTERSECT_TECHNIQUE));
	obj->transformBound();
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

	Channels refl = Channels(0);
	if(RRGetState(RRSS_GET_REFLECTED))
	{
		unsigned oldSource = RRSetState(RRSS_GET_SOURCE,0);
		refl = tri->topivertex[vertex]->irradiance();
		RRSetState(RRSS_GET_SOURCE,oldSource);
	}
	Channels rad = (RRGetState(RRSS_GET_SOURCE)?tri->getSourceIrradiance()/tri->area:Channels(0)) + refl;
	static RRColor tmp;
#if CHANNELS == 1
	tmp[0] = rad*__colorFilter[0];
	tmp[1] = rad*__colorFilter[1];
	tmp[2] = rad*__colorFilter[2];
#else
	tmp[0] = rad.x*__colorFilter[0];
	tmp[1] = rad.y*__colorFilter[1];
	tmp[2] = rad.z*__colorFilter[2];
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

void RRScene::getInfo(char* buf, unsigned type)
{
	switch(type)
	{
	case 0: scene->infoScene(buf); break;
	case 1: scene->infoImprovement(buf,2); break;
	case 2: rrIntersect::intersectStats.getInfo(buf,900,2); break;
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
	RRSetState(RRSS_INTERSECT_TECHNIQUE,rrIntersect::RRIntersect::IT_BSP_FASTEST);
	RRSetStateF(RRSSF_IGNORE_SMALLER_AREA,SMALL_REAL);
	RRSetStateF(RRSSF_IGNORE_SMALLER_ANGLE,0.001f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_AREA,0.01f);
	RRSetStateF(RRSSF_FIGHT_SMALLER_ANGLE,0.01f);
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
