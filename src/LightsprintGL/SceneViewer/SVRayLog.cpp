// --------------------------------------------------------------------------
// Scene viewer - ray log for debugging.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVRayLog.h"

#ifdef SUPPORT_SCENEVIEWER

namespace rr_gl
{

	SVRayLog::Ray SVRayLog::log[MAX_RAYS];
	unsigned SVRayLog::size = 0;

	void SVRayLog::push_back(const rr::RRRay* ray, bool hit)
	{
		if (size<MAX_RAYS)
		{
			log[size].begin = ray->rayOrigin;
			log[size].end = hit ? ray->hitPoint3d : ray->rayOrigin+rr::RRVec3(1/ray->rayDirInv[0],1/ray->rayDirInv[1],1/ray->rayDirInv[2])*ray->rayLengthMax;
			log[size].infinite = !hit;
			log[size].unreliable = false;
			size++;
		}
	}

}; // namespace

#endif // SUPPORT_SCENEVIEWER
