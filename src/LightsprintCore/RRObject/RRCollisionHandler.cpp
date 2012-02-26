// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright (c) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerFirstVisible

class RRCollisionHandlerFirstVisible : public RRCollisionHandler
{
public:
	RRCollisionHandlerFirstVisible(const RRObject* _object)
	{
		object = _object;
	}
	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_POINT2D;
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&RRRay::FILL_POINT2D);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_SIDE);

		const RRObject* objectToReadMaterialFrom = ray->hitObject?ray->hitObject:object;
		if (!objectToReadMaterialFrom)
			return true;

		RRPointMaterial pointMaterial;
		objectToReadMaterialFrom->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
		return result = pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom
			// This makes selecting in sceneviewer see through transparent pixels, they don't have renderFrom cleared.
			&& pointMaterial.specularTransmittance.color!=RRVec3(1);
	}
	virtual bool done()
	{
		return result;
	}
private:
	bool result;
	const RRObject* object;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRCollisionHandler* RRObject::createCollisionHandlerFirstVisible() const
{
	return new RRCollisionHandlerFirstVisible(this);
}


////////////////////////////////////////////////////////////////////////////
//
// RRCollider

static void addRay(const RRCollider* collider, RRRay& ray, RRVec3 dir, RRVec2& distanceMinMax)
{
	float dirLength = dir.length();
	dir.normalize();
	ray.rayDir = dir;
	if (dir.finite() && collider->intersect(&ray))
	{
		// calculation of distanceOfPotentialNearPlane depends on dir length
		float distanceOfPotentialNearPlane = ray.hitDistance/dirLength;
		distanceMinMax[0] = RR_MIN(distanceMinMax[0],distanceOfPotentialNearPlane);
		distanceMinMax[1] = RR_MAX(distanceMinMax[1],distanceOfPotentialNearPlane);
	}
}

void RRCollider::getDistancesFromPoint(const RRVec3& point, const RRObject* object, RRVec2& distanceMinMax)
{
	RRRay ray;
	RRCollisionHandlerFirstVisible collisionHandler(object);
	ray.rayOrigin = point;
	ray.rayLengthMax = 1e12f;
	ray.rayFlags = RRRay::FILL_DISTANCE;
	ray.hitObject = object;
	ray.collisionHandler = &collisionHandler;
	enum {RAYS=3}; // total num rays is (2*RAYS+1)^2 * 6 = 294
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			float u = i/(RAYS+0.5f);
			float v = j/(RAYS+0.5f);
			addRay(this,ray,RRVec3(u,v,+1),distanceMinMax);
			addRay(this,ray,RRVec3(u,v,-1),distanceMinMax);
			addRay(this,ray,RRVec3(u,+1,v),distanceMinMax);
			addRay(this,ray,RRVec3(u,-1,v),distanceMinMax);
			addRay(this,ray,RRVec3(+1,u,v),distanceMinMax);
			addRay(this,ray,RRVec3(-1,u,v),distanceMinMax);
		}
	}
}

void RRCollider::getDistancesFromCamera(const RRCamera& camera, const RRObject* object, RRVec2& distanceMinMax)
{
	RRRay ray;
	RRCollisionHandlerFirstVisible collisionHandler(object);
	ray.rayLengthMax = 1e12f;
	ray.rayFlags = RRRay::FILL_DISTANCE;
	ray.hitObject = object;
	ray.collisionHandler = &collisionHandler;
	enum {RAYS=4}; // #rays is actually (2*RAYS+1)^2 = 81
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			RRVec2 posInWindow(i/float(RAYS),j/float(RAYS));
			ray.rayOrigin = camera.getRayOrigin(posInWindow);
			addRay(this,ray,camera.getRayDirection(posInWindow),distanceMinMax);
		}
	}
}

} // namespace
