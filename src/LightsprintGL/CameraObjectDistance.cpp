// --------------------------------------------------------------------------
// Camera-object distance measuring helper.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "CameraObjectDistance.h"

namespace rr_gl
{

////////////////////////////////////////////////////////////////////////
//
// CameraObjectDistance

CameraObjectDistance::CameraObjectDistance(const rr::RRObject* _object)
{
	object = _object;
	distMin = 1e10f;
	distMax = 0;
	ray = rr::RRRay::create();
	ray->rayLengthMin = 0;
	ray->rayLengthMax = 1e12f;
	ray->rayFlags = rr::RRRay::FILL_DISTANCE;
	ray->collisionHandler = object ? object->createCollisionHandlerFirstVisible() : NULL;
}

CameraObjectDistance::~CameraObjectDistance()
{
	delete ray->collisionHandler;
	delete ray;
}

void CameraObjectDistance::addRay(const rr::RRVec3& pos, rr::RRVec3 dir)
{
	float dirLength = dir.length();
	dir.normalize();
	ray->rayOrigin = pos;
	ray->rayDirInv[0] = 1/dir[0];
	ray->rayDirInv[1] = 1/dir[1];
	ray->rayDirInv[2] = 1/dir[2];
	if (pos.finite() && dir.finite() && object && object->getCollider()->intersect(ray))
	{
		// calculation of distanceOfPotentialNearPlane depends on dir length
		float distanceOfPotentialNearPlane = ray->hitDistance/dirLength;
		distMin = RR_MIN(distMin,distanceOfPotentialNearPlane);
		distMax = RR_MAX(distMax,distanceOfPotentialNearPlane);
	}
}

void CameraObjectDistance::addPoint(const rr::RRVec3& pos)
{
	enum {RAYS=4}; // total num rays is (2*RAYS+1)^2 * 6 = 486
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			float u = i/(RAYS+0.5f);
			float v = j/(RAYS+0.5f);
			addRay(pos,rr::RRVec3(u,v,+1));
			addRay(pos,rr::RRVec3(u,v,-1));
			addRay(pos,rr::RRVec3(u,+1,v));
			addRay(pos,rr::RRVec3(u,-1,v));
			addRay(pos,rr::RRVec3(+1,u,v));
			addRay(pos,rr::RRVec3(-1,u,v));
		}
	}
}

void CameraObjectDistance::addCamera(Camera* camera)
{
	if (!object)
		return;
	if (!camera)
		return;
	if (camera->orthogonal)
	{
		RR_ASSERT(!camera->orthogonal);
		return;
	}
	enum {RAYS=4}; // #rays is actually (2*RAYS+1)^2
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			addRay(camera->pos,camera->getDirection(rr::RRVec2(i/float(RAYS),j/float(RAYS))));
		}
	}
}


}; // namespace
