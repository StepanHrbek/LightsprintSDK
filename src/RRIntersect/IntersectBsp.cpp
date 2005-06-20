#include "IntersectBsp.h"

#ifdef USE_BSP

#define DBG(a) //a

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <time.h>  //gate
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

bool IntersectBsp::intersect_bspSRLNP(RRRay* ray, BspTree *t, real distanceMax) const
// input:                t, rayOrigin, rayDir, skip, hitDistanceMin, hitDistanceMax
// returns:              true if ray hits t
// modifies when hit:    (distanceMin, hitPoint3d) hitPoint2d, hitOuterSide, hitDistance
// modifies when no hit: (distanceMin, hitPoint3d)
//
// approx 50% of runtime is spent here
// all calls (except recursion) are inlined
{
begin:
	intersectStats.intersect_bspSRLNP++;
	assert(ray);
	assert(t);

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	unsigned* triangle=(unsigned*)((char*)back+(t->back?back->size:0));
	assert(triangleSRLNP);
	Plane& n=triangleSRLNP[triangle[0]].n3;
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
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
	const void* trianglesEnd=t->getTrianglesEnd();
	while(triangle<trianglesEnd)
	{
		if (*triangle!=ray->skipTriangle && intersect_triangleSRLNP(ray,triangleSRLNP+*triangle))
		{
			assert(IS_NUMBER(distancePlane));
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
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

bool IntersectBsp::intersect_bspNP(RRRay* ray, BspTree *t, real distanceMax) const
{
begin:
	intersectStats.intersect_bspNP++;
	assert(ray);
	assert(t);

	BspTree *front=t+1;
	BspTree *back=(BspTree *)((char*)front+(t->front?front->size:0));
	unsigned* triangle=(unsigned*)((char*)back+(t->back?back->size:0));
	assert(triangleNP);
	Plane& n=triangleNP[triangle[0]].n3;
	DBGLINE
	real nonz = ray->rayDir[0]*n.x+ray->rayDir[1]*n.y+ray->rayDir[2]*n.z;
	if(nonz==0) return false; // ray parallel with plane (possibly inside)
	real distancePlane = -(ray->rayOrigin[0]*n.x+ray->rayOrigin[1]*n.y+ray->rayOrigin[2]*n.z+n.d) / nonz;
	DBGLINE
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
		if (*triangle!=ray->skipTriangle && intersect_triangleNP(ray,triangleNP+*triangle,&t2))
		{
#ifdef FILL_HITTRIANGLE
			ray->hitTriangle = *triangle;
#endif
#ifdef FILL_HITDISTANCE
			ray->hitDistance = distancePlane;
#endif
			DBGLINE
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
	bool retried = false;
	const char* name = getFileName(aimporter,".bsp");
	FILE* f = fopen(name,"rb");
	if(!f)
	{
		printf("'%s' not found.\n",name);
retry:
		OBJECT obj;
		obj.face_num = triangles;
		obj.vertex_num = importer->getNumVertices();
		obj.face = new FACE[obj.face_num];
		obj.vertex = new VERTEX[obj.vertex_num];
		for(int i=0;i<obj.vertex_num;i++)
		{
			real* v = importer->getVertex(i);
			obj.vertex[i].x = v[0];
			obj.vertex[i].y = v[1];
			obj.vertex[i].z = v[2];
			obj.vertex[i].id = i;
			obj.vertex[i].side = 1;
			obj.vertex[i].used = 1;
		}
		unsigned ii=0;
		for(int i=0;i<obj.face_num;i++)
		{
			unsigned v0,v1,v2;
			importer->getTriangle(i,v0,v1,v2);
			obj.face[ii].vertex[0] = &obj.vertex[v0];
			obj.face[ii].vertex[1] = &obj.vertex[v1];
			obj.face[ii].vertex[2] = &obj.vertex[v2];
			if(isValidTriangle(i)) obj.face[ii++].id=i;
		}
		obj.face_num = ii;
		f = fopen(name,"wb");
		if(f)
		{
			createAndSaveBsp(f,&obj);
			fclose(f);
		}
		delete[] obj.vertex;
		delete[] obj.face;
		f = fopen(name,"rb");
	}
	if(f)
	{
		printf("Loading '%s'.\n",name);
		tree = load(f);
		fclose(f);
		if(!tree && !retried)
		{
			printf("Invalid tree in cache (%s), trying to fix...\n",name);
			retried = true;
			goto retry;
		}
	}
	time_t t = time(NULL);
	if(t<1118815599 || t>1118814496+77*24*3599) tree = NULL;
}

bool IntersectBsp::intersect(RRRay* ray) const
{
	// fallback when bspgen failed (run from readonly disk etc)
	if(!tree)
	{
		DBGLINE
		DBG(printf("Bsp fallback to linear."));
		return IntersectLinear::intersect(ray);
	}

	DBG(printf("\n"));
	intersectStats.intersects++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp

	bool hit = false;
	assert(fabs(size2((*(Vec3*)(ray->rayDir)))-1)<0.001);//ocekava normalizovanej dir
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

	if(hit) intersectStats.hits++;
	return hit;
}

IntersectBsp::~IntersectBsp()
{
	free(tree);
}

} // namespace

#endif
