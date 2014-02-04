// --------------------------------------------------------------------------
// Scene viewer - ray log for debugging.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVRayLog.h"

namespace rr_ed
{

	SVRayLog::Ray SVRayLog::log[MAX_RAYS];
	unsigned SVRayLog::size = 0;

	void SVRayLog::push_back(const rr::RRRay* ray, bool hit)
	{
		if (size<MAX_RAYS)
		{
			log[size].begin = ray->rayOrigin;
			log[size].end = hit ? ray->hitPoint3d : ray->rayOrigin+ray->rayDir*ray->rayLengthMax;
			log[size].infinite = !hit;
			log[size].unreliable = false;
			size++;
		}
	}

}; // namespace
