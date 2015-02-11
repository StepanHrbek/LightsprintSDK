//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Response to ray-mesh collision.
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
	RRCollisionHandlerFirstVisible(const RRObject* _object, bool _shadowRays)
	{
		object = _object;
		shadowRays = _shadowRays;
	}
	virtual void init(RRRay& ray)
	{
		ray.rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_POINT2D;
		result = false;
	}
	virtual bool collides(const RRRay& ray)
	{
		RR_ASSERT(ray.rayFlags&RRRay::FILL_POINT2D);
		RR_ASSERT(ray.rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray.rayFlags&RRRay::FILL_SIDE);

		const RRObject* objectToReadMaterialFrom = ray.hitObject?ray.hitObject:object;
		if (!objectToReadMaterialFrom)
			return true;

		RRPointMaterial pointMaterial;
		objectToReadMaterialFrom->getPointMaterial(ray.hitTriangle,ray.hitPoint2d,NULL,false,pointMaterial); // custom is sufficient, no colorSpace needed
		if ( (pointMaterial.sideBits[ray.hitFrontSide?0:1].renderFrom
			 || (shadowRays && pointMaterial.sideBits[ray.hitFrontSide?1:0].renderFrom) // [#45] shadowRays collide with both sides
			)
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
	bool shadowRays;
};


} // namespace
