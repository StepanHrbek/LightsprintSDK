#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include "core.h"
#include "RREngine.h"

/*
#ifdef ONE
#include "geometry.cpp"
#include "core.cpp"
#include "intersections.cpp"
#ifdef SUPPORT_INTERPOL
#include "interpol.cpp"
#endif
#ifdef SUPPORT_DYNAMIC
#include "dynamic.cpp"
#endif
#else
#include "geometry.h"
#include "core.h"
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

#define DBG(a) a //!!!
#define c_useClusters false
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

RRScene::OBJECT_HANDLE RRScene::objectCreate(RRSceneObjectImporter* importer)
{
	assert(importer);	
	TObject *obj=new TObject(importer->getNumVertices(),importer->getNumTriangles());
	obj->importer = importer;

	/*/ import vertices
	DBG(printf(" vertices...\n"));
	for (unsigned v=0;v<obj->vertices;v++) 
	{
		importer->getVertex(v,obj->vertex[v].x,obj->vertex[v].y,obj->vertex[v].z);
#ifdef TEST_SCENE
		//if(v<10000) // too slow for big scenes
		//  for (int i=0;i<v;i++)
		//    if (obj->vertex[v]==obj->vertex[i]) {duplicit++;break;}
#endif
	}*/

	// import triangles
	// od nuly nahoru insertuje emitory, od triangles-1 dolu ostatni
	DBG(printf(" triangles...\n"));
	int tbot=0;
#ifdef SUPPORT_DYNAMIC
	int ttop=obj->triangles-1;
#endif
	for (unsigned fi=0;fi<obj->triangles;fi++) {
		unsigned v0,v1,v2,si;
		importer->getTriangle(fi,v0,v1,v2,si);
		RRSurface* s=importer->getSurface(si);
		assert(s);
		// rozhodne zda vlozit face dolu mezi emitory nebo nahoru mezi ostatni
		Triangle *t;
#ifdef SUPPORT_DYNAMIC
		if(s && s->diffuseEmittance) t=&obj->triangle[tbot++]; else t=&obj->triangle[ttop--];
#else
		t=&obj->triangle[tbot++];
#endif
		assert(t>=obj->triangle && t<&obj->triangle[obj->triangles]);
		// vlozi ho, seridi geometrii atd
		/*Normal n;
		n.x=f->normal.a;
		n.y=f->normal.b;
		n.z=f->normal.c;
		n.d=f->normal.d;*/
		int geom=t->setGeometry(
			(Vec3*)(importer->getVertex(v0)),
			(Vec3*)(importer->getVertex(v1)),
			(Vec3*)(importer->getVertex(v2))/*,
			&n*/);
		if(geom>=0)
		{
			// geometrie je v poradku
			obj->energyEmited+=fabs(t->setSurface(s));
		} else {
			// geometrie je invalidni, trojuhelnik zahazujem
			printf("# Removing invalid triangle %d (reason %d)\n",fi,geom);
			//printf("  [%.2f %.2f %.2f] [%.2f %.2f %.2f] [%.2f %.2f %.2f]\n",obj->vertex[v0].x,obj->vertex[v0].y,obj->vertex[v0].z,obj->vertex[v1].x,obj->vertex[v1].y,obj->vertex[v1].z,obj->vertex[v2].x,obj->vertex[v2].y,obj->vertex[v2].z);
			--obj->triangles;
#ifdef SUPPORT_DYNAMIC
			if(s->diffuseEmittance) tbot--;else ttop++;//undo insert
			byte tmp[sizeof(Triangle)];
			memcpy(tmp,&obj->triangle[obj->triangles],sizeof(Triangle));
			memcpy(&obj->triangle[obj->triangles],&obj->triangle[ttop],sizeof(Triangle));
			memcpy(&obj->triangle[ttop],tmp,sizeof(Triangle));
			ttop--;
#else
			tbot--; //undo insert
#endif
		}
	}
	   
#ifdef SUPPORT_DYNAMIC
	obj->trianglesEmiting=tbot;
	obj->transformMatrix=&w->object[o].transform;
	obj->inverseMatrix=&w->object[o].inverse;
	// vyzada si prvni transformaci
	obj->matrixDirty=true;
#endif
	// preprocessuje objekt
	DBG(printf(" bounds...\n"));
	obj->detectBounds();
	{
		DBG(printf(" edges...\n"));
		obj->buildEdges(); // build edges only for clusters and/or interpol
	}
	if(c_useClusters) 
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
	obj->intersector = newIntersect(importer);   //!!! won't be freed
	// vlozi objekt do sceny
#ifdef SUPPORT_DYNAMIC
	if (w->object[o].pos.num!=1 || w->object[o].rot.num!=1) 
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

void RRScene::objectDestroy(OBJECT_HANDLE object)
{
	assert(object<scene->objects);
	scene->objRemoveStatic(object);
}

void RRScene::sceneResetStatic()
{
	scene->resetStaticIllumination(false);
}

bool RRScene::sceneImproveStatic(ENDFUNC endfunc)
{
	return scene->improveStatic(endfunc);
}

void RRScene::compact()
{
}

float RRScene::triangleGetRadiosity(OBJECT_HANDLE object, unsigned triangle, unsigned vertex)
{
	assert(object<scene->objects);
	return scene->object[object]->triangle[triangle].topivertex[vertex]->radiosity();
}

void* RRScene::getScene()
{
	return _scene;
}

void* RRScene::getObject(OBJECT_HANDLE object)
{
	assert(object<scene->objects);
	return scene->object[object];
}

union StateValue
{
	unsigned u;
	real r;
};

static StateValue RRSSValue[RRSS_LAST];

void RRResetStates()
{
	memset(RRSSValue,0,sizeof(RRSSValue));
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

} // namespace
