#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "RRVision.h" // dbgray
#include "rrcore.h"
#include "intersections.h"

using namespace rr;
namespace rr
{

#define DBG(a) //a

//////////////////////////////////////////////////////////////////////////////
//
// intersections
//
unsigned __shot=0;

// return first intersection with object
//  const inputs in ray: rayLengthMin, rayLengthMax, rayFlags, surfaceImporter
//  var inputs in ray:
//  outputs in ray: all defined by rayFlags

Triangle* Object::intersection(RRRay& ray, const Point3& eye, const Vec3& direction)
{
	DBG(printf("\n"));
	__shot++;
	if(!triangles) return false; // although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
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

class SkipTriangle : public rr::RRAcceptHit
{
public:
	SkipTriangle(unsigned askip) : skip(askip) {}
	virtual bool acceptHit(const rr::RRRay* ray)
	{
		return ray->hitTriangle!=skip;
	}
	unsigned skip;
};

#include <memory.h>
#define LOG_RAY(aeye,adir,adist) { \
	STATISTIC( \
	RRScene::getSceneStatistics()->lineSegments[RRScene::getSceneStatistics()->numLineSegments].point[0]=aeye; \
	RRScene::getSceneStatistics()->lineSegments[RRScene::getSceneStatistics()->numLineSegments].point[1]=(aeye)+(adir)*(adist); \
	++RRScene::getSceneStatistics()->numLineSegments%=RRScene::getSceneStatistics()->MAX_LINES; ) }

// return first intersection with "scene minus *skip minus dynamic objects"
//  const inputs in ray: rayLengthMin, rayLengthMax, rayFlags
//  var inputs in ray:
//  outputs in ray: all defined by rayFlags
Triangle* Scene::intersectionStatic(RRRay& ray, const Point3& eye, const Vec3& direction, Triangle* skip)
{
	assert(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	// pri velkem poctu objektu by pomohlo sesortovat je podle
	//  vzdalenosti od oka a blizsi testovat driv
	Triangle* hitTriangle = NULL;
	static SkipTriangle skipTriangle(INT_MAX);
	ray.surfaceImporter = &skipTriangle;

	if(multiCollider)
	{
		RRMesh::MultiMeshPreImportNumber preImportSkip;
		preImportSkip.object = skip->object->id; // we expect that object id is object index in scene
		assert(preImportSkip.object<objects);
		// get single-postimport skip
		unsigned postImportSkip = (unsigned)(skip-object[preImportSkip.object]->triangle);
		assert(postImportSkip<object[preImportSkip.object]->triangles);
		// get single-preimport skip
		unsigned tmp = object[preImportSkip.object]->importer->getCollider()->getImporter()->getPreImportTriangle(postImportSkip);
		assert(tmp!=RRMesh::UNDEFINED);
		preImportSkip.index = tmp;
		// get multi-postimport skip
		skipTriangle.skip = multiCollider->getImporter()->getPostImportTriangle(preImportSkip);

		ray.rayOrigin[0] = eye.x;
		ray.rayOrigin[1] = eye.y;
		ray.rayOrigin[2] = eye.z;
		ray.rayDirInv[0] = 1/direction[0];
		ray.rayDirInv[1] = 1/direction[1];
		ray.rayDirInv[2] = 1/direction[2];
		// get multi-postimport ray.hitTriangle
		if(!multiCollider->intersect(&ray)) return NULL;
		// convert to single-preimport preImportNumber.index
		RRMesh::MultiMeshPreImportNumber preImportNumber = multiCollider->getImporter()->getPreImportTriangle(ray.hitTriangle);
		assert(preImportNumber!=RRMesh::UNDEFINED);
		unsigned obj = preImportNumber.object;
		// convert to single-postimport tri
		assert(obj<staticObjects);
		assert(object[obj]);
		assert(object[obj]->importer);
		assert(object[obj]->importer->getCollider());
		assert(object[obj]->importer->getCollider()->getImporter());
		unsigned tri = object[obj]->importer->getCollider()->getImporter()->getPostImportTriangle(preImportNumber.index);
		assert(tri<object[obj]->triangles);
		hitTriangle = &object[obj]->triangle[tri];
		assert(hitTriangle!=skip);

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

		// jen kvuli logovani
		ray.rayLengthMax = ray.hitDistance;
	}
	else

	/* this could bring microscopic speedup, not worth it
	if(staticObjects==1)
	{
		skipTriangle.skip = (unsigned)(skip-object[0]->triangle);
		hitTriangle = object[0]->intersection(ray,eye,direction); // no intersection -> outputs get undefined 
	}	
	else*/
	{       
		U8 backup[10*sizeof(RRReal)+sizeof(unsigned)+sizeof(bool)]; //!!! may change with changes in RRRay
		for(unsigned o=0;o<staticObjects;o++)
			if(object[o]->bound.intersect(eye,direction,ray.rayLengthMax)) // replaced by pretests in RRCollider
			{
				skipTriangle.skip = (unsigned)(skip-object[o]->triangle);
				real tmpMin = ray.rayLengthMin;
				real tmpMax = ray.rayLengthMax;
				Triangle* tmp = object[o]->intersection(ray,eye,direction); // no intersection -> outputs get undefined 
				ray.rayLengthMin = tmpMin;
				if(tmp) 
				{
					hitTriangle = tmp;
					ray.rayLengthMax = ray.hitDistance;
					if(staticObjects>1) memcpy(backup,&ray.hitDistance,sizeof(backup)); // backup valid outputs
				} else {
					ray.rayLengthMax = tmpMax;
				}
			}
		if(staticObjects>1) memcpy(&ray.hitDistance,backup,sizeof(backup)); // restore valid outputs
	}

	LOG_RAY(eye,direction,hitTriangle?ray.rayLengthMax:-1);
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

	if(!dynobj->intersection(ray,eye,direction,hitTriangle,hitPoint2d,hitFrontSide,hitDistance))
		return false;
	for(unsigned o=0;o<objects;o++)
	{
		if(object[o]!=dynobj && object[o]->bound.intersect(eye,direction,*hitDistance))
		{
			Triangle *hitTriangle;
			Hit hitPoint2d;
			bool hitFrontSide;
			real hitDistTmp = *hitDistance;
			if(object[o]->intersection(ray,eye,direction,&hitTriangle,&hitPoint2d,&hitFrontSide,&hitDistTmp))
				return false;
		}
	}

	// inserts hit triangle to hitTriangles
	if(!(*hitTriangle)->hits.hits) hitTriangles.insert(*hitTriangle);
	return true;
}

#endif

} // namespace
