#ifndef COLLIDER_GEOMETRY_H
#define COLLIDER_GEOMETRY_H

#include "Lightsprint/RRCollider.h"
#include "../RRMathPrivate.h"
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

	typedef RRVec3p Plane;

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
