#ifndef COLLIDER_GEOMETRY_H
#define COLLIDER_GEOMETRY_H

#include "RRCollider.h"
#include "../RRMath/RRMathPrivate.h"
#include "config.h"

#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

namespace rr
{
	typedef RRReal real;

	//////////////////////////////////////////////////////////////////////////////
	//
	// 3d vector

	typedef RRVec3 Vec3;

	//////////////////////////////////////////////////////////////////////////////
	//
	// plane in 3d

	typedef RRVec4 Plane;

	real normalValueIn(const Plane& n,const Vec3& a);

	//////////////////////////////////////////////////////////////////////////////
	//
	// sphere in 3d

#ifdef USE_SPHERE
	struct Sphere
	{
		Vec3    center;
		real    radius;
		real    radius2;
		void    detect(const Vec3 *vertex,unsigned vertices);
		bool    intersect(RRRay* ray) const;
	};
#endif

	//////////////////////////////////////////////////////////////////////////////
	//
	// box in 3d

	struct Box
	{
		Vec3    min;
		real    pad;
		Vec3    max;
		void    detect(const Vec3 *vertex,unsigned vertices);
		bool    intersect(RRRay* ray) const;
		bool    intersectFast(RRRay* ray) const;
	};
}

#endif
