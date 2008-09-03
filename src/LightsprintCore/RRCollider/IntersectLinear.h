// --------------------------------------------------------------------------
// Ray-mesh intersection traversal - linear.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
		virtual unsigned  getMemoryOccupied() const;
	protected:
		IntersectLinear(const RRMesh* aimporter);
		real              DELTA_BSP; // tolerance to numeric errors (absolute distance in scenespace)
		unsigned          triangles;
#if defined(_M_X64) || defined(_LP64)
		Box               box; // aligned + vtable(8) + DELTA_BSP(4) + triangles(4) = aligned
		const RRMesh*     importer;
#else
		const RRMesh*     importer;
		Box               box; // aligned + vtable(4) + importer(4) + DELTA_BSP(4) + triangles(4) = aligned
#endif
	};


	////////////////////////////////////////////////////////////////////////////
	//
	// RayHits

	// Helper for intersecting bunch of unordered triangles in proper order (by hit distance).
	class RayHits
	{
	public:
		RayHits(unsigned maxNumHits);
		// Call each time when ray collides with triangle. Order of inserts doesn't matter.
		// It should be still the same ray, only hit may differ.
		void insertHitUnordered(RRRay* ray);
		// Call once at the end, stored hits are processed.
		// Hits are passed to collisionHandler ordered by distance.
		// True = nearest accepted hit is saved to ray.
		bool getHitOrdered(RRRay* ray, const RRMesh* mesh);
		~RayHits();
	private:
		//std::vector<RayHitBackup> backupSlot;
		class RayHitBackup* backupSlot;
		unsigned backupSlotsUsed;
		unsigned theBestSlot;
	};

} // namespace

#endif
