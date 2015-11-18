//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Ray-mesh intersection traversal - linear.
// --------------------------------------------------------------------------

#include "IntersectLinear.h"
#include "Lightsprint/RRObject.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib> // exit, qsort

using namespace std; // necessary for isnan() on Mac

#define DBG(a) //a

namespace rr
{

PRIVATE bool update_rayDir(RRRay& ray)
{
#ifdef COLLIDER_INPUT_INVDIR
	ray.rayDir[0] = 1/ray.rayDirInv[0];
	ray.rayDir[1] = 1/ray.rayDirInv[1];
	ray.rayDir[2] = 1/ray.rayDirInv[2];
#else
#ifdef TRAVERSAL_INPUT_DIR_INVDIR
	ray.rayDirInv[0] = 1/ray.rayDir[0];
	ray.rayDirInv[1] = 1/ray.rayDir[1];
	ray.rayDirInv[2] = 1/ray.rayDir[2];
#endif
#endif
	return true;
}

PRIVATE void update_hitPoint3d(RRRay& ray, real distance)
{
	ray.hitPoint3d[0] = ray.rayOrigin[0] + ray.rayDir[0]*distance;
	ray.hitPoint3d[1] = ray.rayOrigin[1] + ray.rayDir[1]*distance;
	ray.hitPoint3d[2] = ray.rayOrigin[2] + ray.rayDir[2]*distance;
}

PRIVATE void update_hitPlane(RRRay& ray, const RRMesh* importer)
{
	RRMesh::TriangleBody t2;
	importer->getTriangleBody(ray.hitTriangle,t2);
	Vec3 n;
	n.x = t2.side1[1] * t2.side2[2] - t2.side1[2] * t2.side2[1];
	n.y = t2.side1[2] * t2.side2[0] - t2.side1[0] * t2.side2[2];
	n.z = t2.side1[0] * t2.side2[1] - t2.side1[1] * t2.side2[0];
	real siz = size(n);
	ray.hitPlane[0] = n.x/siz;
	ray.hitPlane[1] = n.y/siz;
	ray.hitPlane[2] = n.z/siz;
	ray.hitPlane[3] = -(t2.vertex0[0] * ray.hitPlane[0] + t2.vertex0[1] * ray.hitPlane[1] + t2.vertex0[2] * ray.hitPlane[2]);
}

PRIVATE bool intersect_triangle(RRRay& ray, const RRMesh::TriangleBody* t)
// input:                ray, t
// returns:              true if ray hits t
// modifies when hit:    hitDistance, hitPoint2D, hitFrontSide
// modifies when no hit: <nothing is changed>
{
	FILL_STATISTIC(intersectStats.intersect_triangle++);
	RR_ASSERT(t);

	// calculate determinant - also used to calculate U parameter
	Vec3 pvec = orthogonalTo(ray.rayDir,t->side2);
	real det = dot(t->side1,pvec);

	// cull test
	bool hitFrontSide = det>0;

	// if determinant is near zero, ray lies in plane of triangle
	if (det==0) return false;
	//#define EPSILON 1e-10 // 1e-6 good for all except bunny, 1e-10 good for bunny
	//if (det>-EPSILON && det<EPSILON) return false;

	// calculate distance from vert0 to ray origin
	Vec3 tvec = ray.rayOrigin-t->vertex0;

	// calculate U parameter and test bounds
	real u = dot(tvec,pvec)/det;
	if (u<0 || u>1) return false;

	// prepare to test V parameter
	Vec3 qvec = orthogonalTo(tvec,t->side1);

	// calculate V parameter and test bounds
	real v = dot(ray.rayDir,qvec)/det;
	if (v<0 || u+v>1) return false;

	// calculate distance where ray intersects triangle
	real dist = dot(t->side2,qvec)/det;
	if (dist<ray.hitDistanceMin || dist>ray.hitDistanceMax) return false;

	ray.hitDistance = dist;
#ifdef FILL_HITPOINT2D
	ray.hitPoint2d[0] = u;
	ray.hitPoint2d[1] = v;
#endif
#ifdef FILL_HITSIDE
	ray.hitFrontSide = hitFrontSide;
#endif
	return true;
}

IntersectLinear::IntersectLinear(const RRMesh* aimporter)
{
#ifdef USE_SSE
	if ((long long)(&box)%16) 
	{
		RRReporter::report(ERRO,"You created unaligned structure. Try static or heap if it's on stack now.\n");
		RR_ASSERT(!((long long)(&box)%16));
		exit(0);
	}
#endif
	importer = aimporter;

	// only for kd/bsp, not used by linear
	triangles = importer->getNumTriangles();
	importer->getAABB(&box.min,&box.max,nullptr);
	box.init(box.min,box.max);
	// lower number = danger of numeric errors
	// higher number = slower intersection
	// (0.01 is good, artifacts from numeric errors not seen yet, 1 is 3% slower)
	//Vec3 tmp = box.max-box.min;
	//DELTA_BSP = (tmp.x+tmp.y+tmp.z)/4*1e-5f;
	RRReal tmpx = RR_MAX(fabs(box.max.x),fabs(box.min.x));
	RRReal tmpy = RR_MAX(fabs(box.max.y),fabs(box.min.y));
	RRReal tmpz = RR_MAX(fabs(box.max.z),fabs(box.min.z));
	RRReal maxCoord = RR_MAX3(tmpx,tmpy,tmpz);
	RR_ASSERT(IS_NUMBER(maxCoord));
	if (maxCoord==0 || _isnan(maxCoord)) maxCoord = 1;
	DELTA_BSP = maxCoord*1e-5f;
	numIntersects = 0;
}

size_t IntersectLinear::getMemoryOccupied() const
{
	return sizeof(IntersectLinear);
}

bool IntersectLinear::isValidTriangle(unsigned i) const
{
	// filters out all degenerates
	// this makes collider safe agains INF/NaN vertices
	RRMesh::TriangleBody tb;
	importer->getTriangleBody(i,tb);
	return tb.isNotDegenerated();
}

// return first intersection with object
// but not with *skip and not more far than *hitDistance
//bool Object::intersection(RRVec3 eye,Vec3 direction,Triankle *skip,
//  Triangle **hitTriangle,Hit *hitPoint2d,bool *hitFrontSide,real *hitDistance)
bool IntersectLinear::intersect(RRRay& ray) const
{
	DBG(printf("\n"));
	FILL_STATISTIC(intersectStats.intersect_mesh++);
	bool hit = false;

	// linear is slow, warn if used too often
	if (const_cast<IntersectLinear*>(this)->numIntersects++>1000) // cast is unsafe, multiple threads may write to numIntersects, but we don't care, it's used only for warning
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Heavy use of IT_LINEAR collider detected, consider using faster IT_BSP_XXX techniques.\n"));

#ifdef COLLISION_HANDLER
	char backup[sizeof(RRRay)];
	if (ray.collisionHandler)
		ray.collisionHandler->init(ray);
#endif

	// collisionHandler->init/done must be called _always_, users depend on it,
	// having ray rejected early by box.intersect test is no excuse
	// (!importer) shouldn't happen but linear is so slow that we can test it
	// (!triangles) is valid case, we probably generate collider even for mesh without triangles
	if (importer)
	{
		// don't use this->triangles, mesh may be dynamic
		unsigned numTriangles = importer->getNumTriangles();

		if (numTriangles)
		{
			// don't use this->box, mesh may be dynamic
			Box boxUnaligned;
			importer->getAABB(&boxUnaligned.min,&boxUnaligned.max,nullptr);
			boxUnaligned.init(boxUnaligned.min,boxUnaligned.max);

#ifdef USE_SSE
			if (boxUnaligned.intersectUnaligned(ray))
#else
			if (boxUnaligned.intersect(ray))
#endif
			{
				ray.hitDistance = ray.hitDistanceMax;
				update_rayDir(ray);
				RR_ASSERT(fabs(size2(ray.rayDir)-1)<0.001);//ocekava normalizovanej dir
				FILL_STATISTIC(intersectStats.intersect_linear++);
				for (unsigned t=0;t<triangles;t++)
				{
					RRMesh::TriangleBody t2;
					importer->getTriangleBody(t,t2);
					if (intersect_triangle(ray,&t2))
					{
						ray.hitTriangle = t;
			#ifdef COLLISION_HANDLER
						if (ray.collisionHandler) 
						{
			#ifdef FILL_HITPOINT3D
							if (ray.rayFlags&RRRay::FILL_POINT3D)
							{
								update_hitPoint3d(ray,ray.hitDistance);
							}
			#endif
			#ifdef FILL_HITPLANE
							if (ray.rayFlags&RRRay::FILL_PLANE)
							{
								update_hitPlane(ray,importer);
							}
			#endif
							// hits are reported in random order
							if (ray.collisionHandler->collides(ray)) 
							{
								memcpy(backup,&ray,sizeof(ray)); // the best hit is stored, ray may be overwritten by other faces that seems better until they get refused by collides
								ray.hitDistanceMax = ray.hitDistance;
								hit = true;
							}
						}
						else
			#endif
						{
							ray.hitDistanceMax = ray.hitDistance;
							hit = true;
						}
					}
				}
			}
		}
	}

#ifdef COLLISION_HANDLER
	if (ray.collisionHandler)
		hit = ray.collisionHandler->done();
#endif

	if (hit) 
	{
#ifdef COLLISION_HANDLER
		if (ray.collisionHandler)
		{
			memcpy(&ray,backup,sizeof(ray)); // the best hit is restored
		}
#endif
#ifdef FILL_HITPOINT3D
		if (ray.rayFlags&RRRay::FILL_POINT3D)
		{
			update_hitPoint3d(ray,ray.hitDistance);
		}
#endif
#ifdef FILL_HITPLANE
		if (ray.rayFlags&RRRay::FILL_PLANE)
		{
			update_hitPlane(ray,importer);
		}
#endif
		FILL_STATISTIC(intersectStats.hit_mesh++);
	}
	return hit;
}

IntersectLinear::~IntersectLinear()
{
}


////////////////////////////////////////////////////////////////////////////
//
// RayHitBackup

void RayHitBackup::createBackupOf(const RRRay& ray)
{
	hitTriangle = ray.hitTriangle;
	hitObject = ray.hitObject;
	hitDistance = ray.hitDistance;
	#ifdef FILL_HITPOINT2D
		hitPoint2d = ray.hitPoint2d;
	#endif
	#ifdef FILL_HITSIDE
		hitFrontSide = ray.hitFrontSide;
	#endif
}

// if mesh is nullptr, ray.hitObject must be set
void RayHitBackup::restoreBackupTo(RRRay& ray, const RRMesh* mesh)
{
	// some data are copied
	ray.hitTriangle = hitTriangle;
	ray.hitObject = hitObject;
	ray.hitDistance = hitDistance;
	#ifdef FILL_HITPOINT2D
		ray.hitPoint2d = hitPoint2d;
	#endif
	#ifdef FILL_HITSIDE
		ray.hitFrontSide = hitFrontSide;
		if (hitObject)
		{
			// corrects hitFrontSide for objects with negative scale
			// this code is used only for dynamic objects
			// static objects somehow end up good, probably thanks to merging into multiobject
			RRVec3 scale = hitObject->getWorldMatrixRef().getScale();
			if (scale.x*scale.y*scale.z<0) 
				ray.hitFrontSide = !hitFrontSide;
		}
	#endif
	// some data are autogenerated
	#ifdef FILL_HITPOINT3D
		if (ray.rayFlags&RRRay::FILL_POINT3D)
		{
			RR_ASSERT(ray.rayFlags&RRRay::FILL_DISTANCE); //!!! this is not ensured
			update_hitPoint3d(ray,ray.hitDistance);
		}
	#endif
	#ifdef FILL_HITPLANE
		if (ray.rayFlags&RRRay::FILL_PLANE)
		{
			// sets local space hitPlane
			if (!mesh && ray.hitObject)
				mesh = ray.hitObject->getCollider()->getMesh();
			RR_ASSERT(mesh);
			if (mesh)
				update_hitPlane(ray,mesh);
			// converts it to world space
			if (ray.hitObject && ray.hitObject->getWorldMatrix())
			{
				ray.hitObject->getWorldMatrix()->transformPlane(ray.hitPlane);
				// normalize
				RRReal length = ray.hitPlane.RRVec3::length();
				ray.hitPlane /= length;
			}
		}
	#endif
}

int RayHitBackup::compareBackupDistance(const void* elem1, const void* elem2)
{
	const RayHitBackup* ray1 = (RayHitBackup*)elem1;
	const RayHitBackup* ray2 = (RayHitBackup*)elem2;
	return (ray1->hitDistance<ray2->hitDistance) ? ((ray1->hitDistance==ray2->hitDistance) ? 0 : -1) : 1;
}


////////////////////////////////////////////////////////////////////////////
//
// RayHits

RayHits::RayHits()
{
	backupSlotDynamic = nullptr;
	backupSlotsUsed = 0;
	backupSlotsAllocated = 0;
	theBestSlot = 0;
}

RayHitBackup* RayHits::getSlots()
{
	return backupSlotDynamic ? backupSlotDynamic : backupSlotStatic;
}

void RayHits::insertHitUnordered(const RRRay& ray)
{
	if (backupSlotsUsed==STATIC_SLOTS)
	{
		backupSlotDynamic = new RayHitBackup[backupSlotsAllocated=4*STATIC_SLOTS];
		memcpy(backupSlotDynamic,backupSlotStatic,sizeof(backupSlotStatic));
	}
	else
	if (backupSlotsAllocated && backupSlotsUsed==backupSlotsAllocated)
	{
		RayHitBackup* tmp = backupSlotDynamic;
		backupSlotDynamic = new RayHitBackup[backupSlotsAllocated*=2];
		memcpy(backupSlotDynamic,tmp,backupSlotsUsed*sizeof(RayHitBackup));
		delete[] tmp;
	}
	getSlots()[backupSlotsUsed++].createBackupOf(ray);
	if (ray.hitDistance<getSlots()[theBestSlot].getHitDistance()) theBestSlot = backupSlotsUsed-1;
}

// if mesh is nullptr, ray.hitObject must be set
bool RayHits::getHitOrdered(RRRay& ray, const RRMesh* mesh)
{
	bool hit = false;
	if (backupSlotsUsed)
	{
		// restore the most close hit
		getSlots()[theBestSlot].restoreBackupTo(ray,mesh);

#ifdef COLLISION_HANDLER
		if (ray.collisionHandler)
		{
			// optimization: try theBestSlot first, before qsort
			if (ray.collisionHandler->collides(ray)) 
			{
				hit = true;
			}
			else
			{
				// when there are multiple slots with the same distance, make sure that theBestSlot gets sorted first
				getSlots()[theBestSlot].hitDistance -= 1;
				// theBestSlot was rejected, sort hits by distance
				qsort(getSlots(),backupSlotsUsed,sizeof(RayHitBackup),RayHitBackup::compareBackupDistance);

				// run collisionHandler on sorted hits
				// skip 0th element, it was theBestSlot before sort
				for (unsigned i=1;i<backupSlotsUsed;i++)
				{
					getSlots()[i].restoreBackupTo(ray,mesh);
					if (ray.collisionHandler->collides(ray)) 
					{
						hit = true;
						break;
					}
				}
			}
		}
		else
#endif
		{
			// simply keep the most close hit
			hit = true;
		}
	}
	return hit;
}

RayHits::~RayHits()
{
	delete[] backupSlotDynamic;
}

} // namespace
