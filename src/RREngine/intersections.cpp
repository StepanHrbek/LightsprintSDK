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

Triangle* Object::intersection(RRRay& ray, const Point3& eye, const Vec3& direction)
{
	DBG(printf("\n"));
	__shot++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
	assert(fabs(size2(direction)-1)<0.001); // normalized dir expected

	// transform from scenespace to objectspace
#ifdef SUPPORT_TRANSFORMS
	// translation+rotation allowed, no scaling, so direction stays normalized
	*(Vec3*)(ray.rayOrigin) = eye.transformed(inverseMatrix);
	*(Vec3*)(ray.rayDir) = direction.rotated(inverseMatrix);
#ifdef SUPPORT_SCALE
	// translation+rotation+scale allowed
	real scale = size(*(Vec3*)(ray.rayDir)); // kolikrat je mesh ve worldu zmenseny
	*(Vec3*)(ray.rayDir) /= scale;
	real hitDistanceMin = ray.hitDistanceMin;
	real hitDistanceMax = ray.hitDistanceMax;
	ray.hitDistanceMin *= scale;
	ray.hitDistanceMax *= scale;
#endif
#else
	// no transformation
	*(Vec3*)(ray.rayOrigin) = eye;
	*(Vec3*)(ray.rayDir) = direction;
#endif
	assert(intersector);

	if(!intersector->intersect(&ray))
		return NULL;

	// compensate scale
#ifdef SUPPORT_TRANSFORMS
#ifdef SUPPORT_SCALE
	ray.hitDistance /= scale;
	ray.hitDistanceMin = hitDistanceMin;
	ray.hitDistanceMax = hitDistanceMax;
#endif
#endif

	// compensate for our rotations
	assert(ray.hitTriangle>=0 && ray.hitTriangle<triangles);
	Triangle* hitTriangle=&triangle[ray.hitTriangle];
	switch(hitTriangle->rotations)
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
	assert(hitTriangle->u2.y==0);

	return hitTriangle;
}

#include <memory.h>
DbgRay rrEngine::dbgRay[MAX_DBGRAYS];
unsigned rrEngine::dbgRays=0;
#define LOG_RAY(aeye,adir,adist) { memcpy(dbgRay[dbgRays].eye,&aeye,sizeof(Vec3)); memcpy(dbgRay[dbgRays].dir,&adir,sizeof(Vec3)); dbgRay[dbgRays].dist=adist; ++dbgRays%=MAX_DBGRAYS; }

// return first intersection with "scene minus *skip minus dynamic objects"
Triangle* Scene::intersectionStatic(RRRay& ray, const Point3& eye, const Vec3& direction, Triangle* skip)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv


	Triangle* hitTriangle = NULL;
	for(unsigned o=0;o<staticObjects;o++)
		if(object[o]->bound.intersect(eye,direction,ray.hitDistanceMax)) // replaced by pretests in RRIntersect
		{
#ifdef _MSC_VER
			ray.skipTriangle = ((unsigned)((U64)skip-(U64)object[o]->triangle))/sizeof(Triangle);
#else
			ray.skipTriangle = ((unsigned)((U32)skip-(U32)object[o]->triangle))/sizeof(Triangle);
#endif
			real tmpMin = ray.hitDistanceMin;
			real tmpMax = ray.hitDistanceMax;
			Triangle* tmp = object[o]->intersection(ray,eye,direction);
			ray.hitDistanceMin = tmpMin;
			if(tmp) 
			{
				hitTriangle = tmp;
				ray.hitDistanceMax = ray.hitDistance;
			} else {
				ray.hitDistanceMax = tmpMax;
			}
		}
	LOG_RAY(eye,direction,hitTriangle?ray.hitDistanceMax:-1);

	return hitTriangle;
}

#ifdef SUPPORT_DYNAMIC

// return first intersection with scene
// but only when that intersection is with dynobj
// sideeffect: inserts hit to triangle and triangle to hitTriangles

Triangle* Scene::intersectionDynobj(RRRay& ray, Point3& eye, Vec3& direction, Object *dynobj, Triangle* skip)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv

	if(!dynobj->intersection(ray,eye,direction,hitTriangle,hitPoint2d,hitOuterSide,hitDistance))
		return false;
	for(unsigned o=0;o<objects;o++)
	{
		if(object[o]!=dynobj && object[o]->bound.intersect(eye,direction,*hitDistance))
		{
			Triangle *hitTriangle;
			Hit hitPoint2d;
			bool hitOuterSide;
			real hitDistTmp = *hitDistance;
			if(object[o]->intersection(ray,eye,direction,&hitTriangle,&hitPoint2d,&hitOuterSide,&hitDistTmp))
				return false;
		}
	}

	// inserts hit triangle to hitTriangles
	if(!(*hitTriangle)->hits.hits) hitTriangles.insert(*hitTriangle);
	return true;
}

#endif

} // namespace
