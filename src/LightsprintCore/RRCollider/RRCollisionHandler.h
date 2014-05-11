// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright (c) 2005-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

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
		if (pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom
			// This makes selecting in sceneviewer see through transparent pixels, they don't have renderFrom cleared.
			&& pointMaterial.specularTransmittance.color!=RRVec3(1))
			return result = true;
		return false;
	}
	virtual bool done()
	{
		return result;
	}
private:
	bool result;
	const RRObject* object;
};


} // namespace
