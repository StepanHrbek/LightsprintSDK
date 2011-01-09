// --------------------------------------------------------------------------
// Helper AABB code for ray-mesh intersections.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

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
	// box in 3d

	struct Box
	{
		Vec3    min;
		real    pad;
		Vec3    max;
		void    init(RRVec3 _min, RRVec3 _max);
		bool    intersect(RRRay* ray) const;
		bool    intersectUnaligned(RRRay* ray) const;
	};
}

#endif
