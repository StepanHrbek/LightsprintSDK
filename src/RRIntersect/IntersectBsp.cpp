#include "IntersectBsp.h"

#ifdef USE_BSP

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include "bsp.h"
#include "cache.h"

namespace rrIntersect
{

BspTree* load(FILE *f)
{
	if(!f) return NULL;
	unsigned size;
	size_t readen = fread(&size,sizeof(size),1,f);
	if(!readen) return NULL;
	size &= 0x3fffffff;
	fseek(f,-(int)sizeof(unsigned),SEEK_CUR);
	BspTree* tree = (BspTree*)malloc(size);
	readen = fread(tree,1,size,f);
	if(readen == size) return tree;
	free(tree);
	return NULL;
}

#define DELTA_BSP 0.01f // tolerance to numeric errors (absolute distance in scenespace)
// higher number = slower intersection
// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)

bool IntersectBsp::intersect_bspSRLNP(RRRay* ray, BspTree *t, real distanceMax) CONST
// input:                t, rayOrigin, rayDir, skip, hitDistanceMin, hitDistanceMax
// returns:              true if ray hits t
// modifies when hit:    (distanceMin, hitPoint3d) hitPoint2d, hitOuterSide, hitDistance
// modifies when no hit: (distanceMin, hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	intersectStats.bsp++;
	assert(t);
	#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
	#endif

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	TRIANGLE_HANDLE* triangle=(TRIANGLE_HANDLE*)((char*)back+(t->back?back->size:0));
	assert(triangleSRLNP);
	Normal n=triangleSRLNP[triangle[0]].n3;
	real distancePlane=intersect_plane_distance(ray,n);
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bspSRLNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspSRLNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=ray->skip && intersect_triangleSRLNP(ray,triangleSRLNP+*triangle))
		{
			ray->hitTriangle = *triangle;
			ray->hitDistance = distancePlane;
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	ray->hitDistanceMin = distancePlane-DELTA_BSP;
	goto begin;
}

bool IntersectBsp::intersect_bspNP(RRRay* ray, BspTree *t, real distanceMax) CONST
{
begin:
	intersectStats.bsp++;
	assert(t);
#ifdef TEST_SCENE
	if (!t) return false; // prazdny strom
#endif

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	TRIANGLE_HANDLE* triangle=(TRIANGLE_HANDLE*)((char*)back+(t->back?back->size:0));
	assert(triangleNP);
	Normal n=triangleNP[triangle[0]].n3;
	real distancePlane=intersect_plane_distance(ray,n);
	bool frontback =
		n[0]*(ray->rayOrigin[0]+ray->rayDir[0]*ray->hitDistanceMin)+
		n[1]*(ray->rayOrigin[1]+ray->rayDir[1]*ray->hitDistanceMin)+
		n[2]*(ray->rayOrigin[2]+ray->rayDir[2]*ray->hitDistanceMin)+
		n[3]>0;

	// test only one half
	if (distancePlane<ray->hitDistanceMin || distancePlane>distanceMax)
	{
		if(frontback)
		{
			if(!t->front) return false;
			t=front;
		} else {
			if(!t->back) return false;
			t=back;
		}
		goto begin;
	}

	// test first half
	if(frontback)
	{
		if(t->front && intersect_bspNP(ray,front,distancePlane+DELTA_BSP)) return true;
	} else {
		if(t->back && intersect_bspNP(ray,back,distancePlane+DELTA_BSP)) return true;
	}

	// test plane
	update_hitPoint3d(ray,distancePlane);
	void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		RRObjectImporter::TriangleSRL t2;
		importer->getTriangleSRL(*triangle,&t2);
		if (*triangle!=ray->skip && intersect_triangleNP(ray,triangleNP+*triangle,&t2))
		{
			ray->hitTriangle = *triangle;
			ray->hitDistance = distancePlane;
			return true;
		}
		triangle++;
	}

	// test other half
	if(frontback)
	{
		if(!t->back) return false;
		t=back;
	} else {
		if(!t->front) return false;
		t=front;
	}
	ray->hitDistanceMin=distancePlane-DELTA_BSP;
	goto begin;
}

IntersectBsp::IntersectBsp(RRObjectImporter* aimporter) : IntersectLinear(aimporter)
{
	tree = NULL;
	const char* name = getFileName(aimporter,".bsp");
	FILE* f = fopen(name,"rb");
	if(!f)
	{
		OBJECT obj;
		obj.face_num = triangles;
		obj.vertex_num = importer->getNumVertices();
		obj.face = new FACE[obj.face_num];
		obj.vertex = new VERTEX[obj.vertex_num];
		for(int i=0;i<obj.vertex_num;i++)
		{
			importer->getVertex(i,obj.vertex[i].x,obj.vertex[i].y,obj.vertex[i].z);
			obj.vertex[i].id = i;
			obj.vertex[i].side = 1;
			obj.vertex[i].used = 1;
		}
		for(int i=0;i<obj.face_num;i++)
		{
			unsigned v0,v1,v2,s;
			importer->getTriangle(i,v0,v1,v2,s);
			obj.face[i].vertex[0] = &obj.vertex[v0];
			obj.face[i].vertex[1] = &obj.vertex[v1];
			obj.face[i].vertex[2] = &obj.vertex[v2];
			obj.face[i].material = s;
		}
		f = fopen(name,"wb");
		if(f)
		{
			createAndSaveBsp(f,&obj);
			fclose(f);
		}
		f = fopen(name,"rb");
	}
	if(f)
	{
		tree = load(f);
	}
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(Point3 eye,Vec3 direction,Triankle *skip,
//  Triankle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
bool IntersectBsp::intersect(RRRay* ray) const
{
	// fallback when bspgen failed (run from readonly disk etc)
	if(!tree) 
	{
		DBG(printf("Bsp fallback to linear."));
		return IntersectLinear::intersect(ray);
	}

	DBG(printf("\n"));
	intersectStats.shots++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	bool hit = false;
	assert(fabs(sizeSquare((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
	assert(tree);
	if(triangleSRLNP)
	{
		hit = intersect_bspSRLNP(ray,tree,ray->hitDistanceMax);
	} else 
	if(triangleNP)
	{
		hit = intersect_bspNP(ray,tree,ray->hitDistanceMax);
	} else {
		assert(0);
	}

	return hit;
}

IntersectBsp::~IntersectBsp()
{
	free(tree);
}

} // namespace

#endif
