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
#ifdef _M_X64
		Box               box; // aligned + vtable(8) + DELTA_BSP(4) + triangles(4) = aligned
		RRMesh*           importer;
#else
		const RRMesh*     importer;
		Box               box; // aligned + vtable(4) + importer(4) + DELTA_BSP(4) + triangles(4) = aligned
#endif
	};

}

#endif
