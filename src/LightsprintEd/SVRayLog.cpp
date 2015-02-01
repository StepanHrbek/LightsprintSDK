// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - ray log for debugging.
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
