#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include "core.h"
#include "RREngine.h"

namespace rrEngine
{

#define DBG(a) a //!!!
#define c_useClusters false
#define scene ((Scene*)_scene)

RRScene::RRScene()
{
	_scene=new Scene();
}

RRScene::~RRScene()
{
	delete scene;
}

OBJECT_HANDLE RRScene::objectCreate(RRSceneObjectImporter* importer)
{
	assert(importer);	
	TObject *obj=new TObject(importer->getNumVertices(),importer->getNumTriangles());
	
	// import vertices
	DBG(printf(" vertices...\n"));
	for (unsigned v=0;v<obj->vertices;v++) 
	{
		importer->getVertex(v,obj->vertex[v].x,obj->vertex[v].y,obj->vertex[v].z);
#ifdef TEST_SCENE
		//if(v<10000) // too slow for big scenes
		//  for (int i=0;i<v;i++)
		//    if (obj->vertex[v]==obj->vertex[i]) {duplicit++;break;}
#endif
	}
#ifndef SUPPORT_DYNAMIC
	// in static mode: convert vertices to scenespace
	MATRIX mat;
	memcpy(mat,importer->getWorldMatrix(),sizeof(MATRIX));
	for (unsigned v=0;v<obj->vertices;v++) 
	{
		obj->vertex[v].transform(&mat);
	}
#endif

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
		Surface* s=importer->getSurface(si);
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
			&obj->vertex[v0],
			&obj->vertex[v1],
			&obj->vertex[v2]/*,
			&n*/);
		if(geom>=0)
		{
			// geometrie je v poradku
			obj->energyEmited+=fabs(t->setSurface(s));
		} else {
			// geometrie je invalidni, trojuhelnik zahazujem
			printf("# Removing invalid triangle %d (reason %d)\n",fi,geom);
			printf("  [%.2f %.2f %.2f] [%.2f %.2f %.2f] [%.2f %.2f %.2f]\n",obj->vertex[v0].x,obj->vertex[v0].y,obj->vertex[v0].z,obj->vertex[v1].x,obj->vertex[v1].y,obj->vertex[v1].z,obj->vertex[v2].x,obj->vertex[v2].y,obj->vertex[v2].z);
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
	// create bsp tree
	//!!!
	   
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

} // namespace
