#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "rrengine.h" // dbgray
#include "rrcore.h"
#include "intersections.h"

using namespace rrIntersect;
namespace rrEngine
{

#define DBG(a) //a

//////////////////////////////////////////////////////////////////////////////
//
// intersections
//
unsigned __shot=0;

// return first intersection with object
// but not with *skip and not more far than *hitDistance

bool Object::intersection(Point3 eye,Vec3 direction,Triangle *skip,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{
	DBG(printf("\n"));
	__shot++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
	assert(fabs(size2(direction)-1)<0.001); // normalized dir expected
#ifdef SUPPORT_TRANSFORMS
	// transform from scenespace to objectspace
	// translation+rotation allowed, no scaling, so direction stays normalized
	eye = eye.transformed(inverseMatrix);
	direction = normalized(direction.rotated(inverseMatrix));
#endif


	RRRay ray;
	ray.flags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE;
	ray.rayOrigin[0] = eye.x;
	ray.rayOrigin[1] = eye.y;
	ray.rayOrigin[2] = eye.z;
	ray.rayDir[0] = direction.x;
	ray.rayDir[1] = direction.y;
	ray.rayDir[2] = direction.z;
#ifdef _MSC_VER
	ray.skipTriangle = ((unsigned)((U64)skip-(U64)triangle))/sizeof(Triangle);
#else
	ray.skipTriangle = ((unsigned)((U32)skip-(U32)triangle))/sizeof(Triangle);
#endif
	ray.hitDistanceMin = 0;
	ray.hitDistanceMax = *hitDistance;
	assert(intersector);
	bool res = intersector->intersect(&ray);
	if(res)
	{
		assert(ray.hitTriangle>=0 && ray.hitTriangle<triangles);
		*hitTriangle = &triangle[ray.hitTriangle];
		// compensate for our rotations
		switch((*hitTriangle)->rotations)
		{
			case 0:
				break;
			case 1:
				{real u=ray.hitPoint2d[0];
				real v=ray.hitPoint2d[1];
				ray.hitPoint2d[0]=v;
				ray.hitPoint2d[1]=1-u-v;}
				break;
			case 2:
				{real u=ray.hitPoint2d[0];
				real v=ray.hitPoint2d[1];
				ray.hitPoint2d[0]=1-u-v;
				ray.hitPoint2d[1]=u;}
				break;
			default:
				assert(0);
		}
		assert((*hitTriangle)->u2.y==0);
#ifdef HITS_FIXED
		hitPoint2d->u=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitU);
		hitPoint2d->v=(HITS_UV_TYPE)(HITS_UV_MAX*i_hitV);
#else
		// prepocet u,v ze souradnic (rightside,leftside)
		//  do *hitPoint2d s ortonormalni bazi (u3,v3)
		assert(ray.hitTriangle>=0 && ray.hitTriangle<triangles);
		ray.hitPoint2d[0]=ray.hitPoint2d[0]*triangle[ray.hitTriangle].u2.x+ray.hitPoint2d[1]*triangle[ray.hitTriangle].v2.x;
		ray.hitPoint2d[1]=ray.hitPoint2d[1]*triangle[ray.hitTriangle].v2.y;
#endif
		hitPoint2d->u = ray.hitPoint2d[0];
		hitPoint2d->v = ray.hitPoint2d[1];
		*hitOuterSide = ray.hitOuterSide;
		*hitDistance = ray.hitDistance;
	}
	return res;
}

#include <memory.h>
DbgRay rrEngine::dbgRay[MAX_DBGRAYS];
unsigned rrEngine::dbgRays=0;
#define LOG_RAY(aeye,adir,adist) { memcpy(dbgRay[dbgRays].eye,&aeye,sizeof(Vec3)); memcpy(dbgRay[dbgRays].dir,&adir,sizeof(Vec3)); dbgRay[dbgRays].dist=adist; ++dbgRays%=MAX_DBGRAYS; }

// return first intersection with "scene minus *skip minus dynamic objects"
bool Scene::intersectionStatic(Point3 eye,Vec3 direction,Triangle *skip,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv

	bool hit=false;
	for(unsigned o=0;o<staticObjects;o++)
		if(object[o]->bound.intersect(eye,direction,*hitDistance))
			if(object[o]->intersection(eye,direction,skip,hitTriangle,hitPoint2d,hitOuterSide,hitDistance))
				 hit=true;
	LOG_RAY(eye,direction,hit?*hitDistance:-1);
	return hit;
}

#ifdef SUPPORT_DYNAMIC

// return first intersection with scene
// but only when that intersection is with dynobj
// sideeffect: inserts hit to triangle and triangle to hitTriangles

bool Scene::intersectionDynobj(Point3 eye,Vec3 direction,Triangle *skip,Object *dynobj,
  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv

	if(!dynobj->intersection(eye,direction,skip,hitTriangle,hitPoint2d,hitOuterSide,hitDistance))
		return false;
	for(unsigned o=0;o<objects;o++)
	{
		if(object[o]!=dynobj && object[o]->bound.intersect(eye,direction,*hitDistance))
		{
			Triangle *hitTriangle;
			Hit hitPoint2d;
			bool hitOuterSide;
			real hitDistTmp = *hitDistance;
			if(object[o]->intersection(eye,direction,skip,&hitTriangle,&hitPoint2d,&hitOuterSide,&hitDistTmp))
				return false;
		}
	}

	// inserts hit triangle to hitTriangles
	if(!(*hitTriangle)->hits.hits) hitTriangles.insert(*hitTriangle);
	return true;
}

#endif

} // namespace
