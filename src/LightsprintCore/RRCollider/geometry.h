//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Helper AABB code for ray-mesh intersections.
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
		void    init(const RRVec3& min, const RRVec3& max);
		bool    intersect(const RRRay& ray) const;
		bool    intersectUnaligned(const RRRay& ray) const;
	};
}

#endif
