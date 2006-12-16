#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "RRVision.h" // dbgray
#include "rrcore.h"
#include "intersections.h"

namespace rr
{

#define DBG(a) //a

//////////////////////////////////////////////////////////////////////////////
//
// intersections
//
unsigned __shot=0;

// return first intersection with object
//  const inputs in ray: rayLengthMin, rayLengthMax, rayFlags, collisionHandler
//  var inputs in ray:
//  outputs in ray: all defined by rayFlags

Triangle* Object::intersection(RRRay& ray, const Point3& eye, const Vec3& direction)
{
	DBG(printf("\n"));
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
	__shot++;
	assert(fabs(size2(direction)-1)<0.001); // normalized dir expected

	// transform from scenespace to objectspace
	Vec3 rayDir;
	real scale = 1;
#ifdef SUPPORT_TRANSFORMS
	real oldRayLengthMin;
	real oldRayLengthMax;
	// translation+rotation allowed, no scaling, so direction stays normalized
	if(inverseMatrix)
	{
		ray.rayOrigin = inverseMatrix->transformedPosition(eye);
		rayDir = inverseMatrix->transformedDirection(direction);
#ifdef SUPPORT_SCALE
		// translation+rotation+scale allowed
		scale = size(rayDir); // kolikrat je mesh ve worldu zmenseny
		oldRayLengthMin = ray.rayLengthMin;
		oldRayLengthMax = ray.rayLengthMax;
		ray.rayLengthMin *= scale;
		ray.rayLengthMax *= scale;
#endif
	}
	else
#endif
	{
		// no transformation
		ray.rayOrigin = eye;
		rayDir = direction;
	}
	ray.rayDirInv[0] = scale/rayDir[0];
	ray.rayDirInv[1] = scale/rayDir[1];
	ray.rayDirInv[2] = scale/rayDir[2];

	if(!importer->getCollider()->intersect(&ray))
		return NULL;

	// compensate scale
#ifdef SUPPORT_TRANSFORMS
#ifdef SUPPORT_SCALE
	if(inverseMatrix)
	{
		ray.hitDistance /= scale;
		ray.rayLengthMin = oldRayLengthMin;
		ray.rayLengthMax = oldRayLengthMax;
	}
#endif
#endif

	assert(ray.hitTriangle>=0 && ray.hitTriangle<triangles);
	Triangle* hitTriangle=&triangle[ray.hitTriangle];

	// compensate for our rotations
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

class SkipTriangle : public RRCollisionHandler
{
public:
	SkipTriangle(unsigned askip) : skip(askip) {}
	virtual void init()
	{
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		result = result || (ray->hitTriangle!=skip);
		return ray->hitTriangle!=skip;
	}
	virtual bool done()
	{
		return result;
	}
	unsigned skip;
private:
	bool result;
};

#include <memory.h>
#define LOG_RAY(aeye,adir,adist,hit) { \
	STATISTIC( \
	RRScene::getSceneStatistics()->lineSegments[RRScene::getSceneStatistics()->numLineSegments].point[0]=aeye; \
	RRScene::getSceneStatistics()->lineSegments[RRScene::getSceneStatistics()->numLineSegments].point[1]=(aeye)+(adir)*(adist); \
	RRScene::getSceneStatistics()->lineSegments[RRScene::getSceneStatistics()->numLineSegments].infinite=!hit; \
	++RRScene::getSceneStatistics()->numLineSegments%=RRScene::getSceneStatistics()->MAX_LINES; ) }

// return first intersection with "scene minus *skip"
//  const inputs in ray: rayLengthMin, rayLengthMax, rayFlags
//  var inputs in ray:
//  outputs in ray: all defined by rayFlags
Triangle* Scene::intersectionStatic(RRRay& ray, const Point3& eye, const Vec3& direction, Triangle* skip)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv
	Triangle* hitTriangle = NULL;
	static SkipTriangle skipTriangle(INT_MAX); //!!! neni thread safe
	ray.collisionHandler = &skipTriangle;

	skipTriangle.skip = (unsigned)(skip-object->triangle);
	hitTriangle = object->intersection(ray,eye,direction); // no intersection -> outputs get undefined 

	LOG_RAY(eye,direction,hitTriangle?ray.rayLengthMax:0.2f,hitTriangle);
	return hitTriangle;
}

} // namespace
