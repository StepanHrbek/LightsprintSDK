// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - linear.
// Copyright (c) 2000-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_INTERSECTLINEAR_H
#define COLLIDER_INTERSECTLINEAR_H

#include "geometry.h"
#include "Lightsprint/RRCollider.h"

#ifdef TRAVERSAL_INPUT_DIR_INVDIR
	#define DIVIDE_BY_RAYDIR *ray->rayDirInv
#else
	#define DIVIDE_BY_RAYDIR /ray->rayDir
#endif

namespace rr
{



	PRIVATE bool update_rayDir(RRRay* ray);
	PRIVATE void update_hitPoint3d(RRRay* ray, real distance);
	PRIVATE void update_hitPlane(RRRay* ray, const RRMesh* importer);
	PRIVATE bool intersect_triangle(RRRay* ray, const RRMesh::TriangleBody* t);

	class IntersectLinear : public RRCollider // RRCollider is RRAligned because this class requested alignment (does it still need it?)
	{
	public:
		static IntersectLinear* create(const RRMesh* aimporter) {return new IntersectLinear(aimporter);}
		virtual ~IntersectLinear();
		virtual bool      intersect(RRRay* ray) const;
		virtual bool      isValidTriangle(unsigned i) const;
		virtual const RRMesh* getMesh() const {return importer;}
		virtual IntersectTechnique getTechnique() const {return IT_LINEAR;}
		virtual size_t    getMemoryOccupied() const;
	protected:
		IntersectLinear(const RRMesh* aimporter);
		real              DELTA_BSP; // tolerance to numeric errors (absolute distance in scenespace)
		unsigned          triangles;
#if defined(_M_X64) || defined(_LP64)
		Box               box; // aligned + vtable(8) + DELTA_BSP(4) + triangles(4) = aligned. for kd/bsp, not used by linear
		const RRMesh*     importer;
#else
		const RRMesh*     importer;
		Box               box; // aligned + vtable(4) + importer(4) + DELTA_BSP(4) + triangles(4) = aligned. for kd/bsp, not used by linear
#endif
		unsigned          numIntersects; // statistics only, we warn if user uses this slow collider too often
	};


	////////////////////////////////////////////////////////////////////////////
	//
	// RayHitBackup

	// RayHitBackup saves minimal set of information about ray hit and restores full hit information on request.
	// saved: hitTriangle,hitObject,hitDistance,hitPoint2d,hitFrontSide
	// autogenerated: hitPoint3d, hitPlane
	// Used by IntersectBspCompact, but Linear and Fast* can use it in future too.
	class RayHitBackup
	{
	public:
		// creates backup of ray hit
		// (it's not necessary to have valid hitPoint3d and hitPlane, they are not saved anyway)
		void createBackupOf(const RRRay* ray);

		// restores ray hit from backup
		// (restores also hitPoint3d and hitPlane that are autogenerated)
		void restoreBackupTo(RRRay* ray, const RRMesh* mesh);

		// helper when sorting ray hits by distance
		static int compareBackupDistance(const void* elem1, const void* elem2);

		RRReal getHitDistance()
		{
			return hitDistance;
		}
		RRReal hitDistance; // public only for getHitOrdered()
	private:
		unsigned hitTriangle;
		const RRObject* hitObject;
		#ifdef FILL_HITPOINT2D
			RRVec2 hitPoint2d;
		#endif
		#ifdef FILL_HITSIDE
			bool hitFrontSide;
		#endif
	};


	////////////////////////////////////////////////////////////////////////////
	//
	// RayHits

	// Helper for intersecting bunch of unordered triangles in proper order (by hit distance).
	class RayHits
	{
	public:
		RayHits();
		// Call each time when ray collides with triangle. Order of inserts doesn't matter.
		// It should be still the same ray, only hit may differ.
		void insertHitUnordered(const RRRay* ray);
		// Call once at the end, stored hits are processed.
		// Hits are passed to collisionHandler ordered by distance.
		// True = nearest accepted hit is saved to ray.
		bool getHitOrdered(RRRay* ray, const RRMesh* mesh);
		~RayHits();
	private:
		enum {STATIC_SLOTS=16};
		RayHitBackup backupSlotStatic[STATIC_SLOTS]; // we use several static slots to make typical case fast
		RayHitBackup* backupSlotDynamic; // dynamic slots are allocated only in case of need (=rarely)
		RayHitBackup* getSlots(); // return used slots, static or dynamic
		unsigned backupSlotsUsed;
		unsigned backupSlotsAllocated;
		unsigned theBestSlot;
	};

} // namespace

#endif
