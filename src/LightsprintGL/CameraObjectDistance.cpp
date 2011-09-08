// --------------------------------------------------------------------------
// Camera-object distance measuring helper.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "CameraObjectDistance.h"

namespace rr_gl
{

////////////////////////////////////////////////////////////////////////
//
// CameraObjectDistance

CameraObjectDistance::CameraObjectDistance(const rr::RRObject* _object, bool _water, float _waterLevel)
{
	water = _water;
	waterLevel = _waterLevel;
	distMin = 1e10f;
	distMax = 0;
	ray = rr::RRRay::create();
	ray->rayLengthMin = 0;
	ray->rayLengthMax = 1e12f;
	ray->rayFlags = rr::RRRay::FILL_DISTANCE;
	ray->hitObject = _object;
	ray->collisionHandler = NULL;
	if (_object)
		ray->collisionHandler = _object->createCollisionHandlerFirstVisible();
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
	ray->rayDir = dir;
	if (pos.finite() && dir.finite() && ray->hitObject && ray->hitObject->getCollider()->intersect(ray))
	{
		// calculation of distanceOfPotentialNearPlane depends on dir length
		float distanceOfPotentialNearPlane = ray->hitDistance/dirLength;
		distMin = RR_MIN(distMin,distanceOfPotentialNearPlane);
		distMax = RR_MAX(distMax,distanceOfPotentialNearPlane);
	}
}

void CameraObjectDistance::addPoint(const rr::RRVec3& pos)
{
	enum {RAYS=3}; // total num rays is (2*RAYS+1)^2 * 6 = 294
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
	// measure distance to water level
	if (water)
	{
		float distanceOfPotentialNearPlane = fabs(pos.y-waterLevel);
		distMin = RR_MIN(distMin,distanceOfPotentialNearPlane);
		distMax = RR_MAX(distMax,distanceOfPotentialNearPlane);
	}
}

void CameraObjectDistance::addCamera(Camera* camera)
{
	if (!ray->hitObject)
		return;
	if (!camera)
		return;
	enum {RAYS=4}; // #rays is actually (2*RAYS+1)^2 = 81
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			rr::RRVec2 posInWindow(i/float(RAYS),j/float(RAYS));
			addRay(camera->getRayOrigin(posInWindow),camera->getRayDirection(posInWindow));
		}
	}
	// measure distance to water level
	// simplification: works as if camera always points to water
	if (water)
	{
		float distanceOfPotentialNearPlane = fabs(camera->pos.y-waterLevel);
		distMin = RR_MIN(distMin,distanceOfPotentialNearPlane);
		distMax = RR_MAX(distMax,distanceOfPotentialNearPlane);
	}
}

}; // namespace

