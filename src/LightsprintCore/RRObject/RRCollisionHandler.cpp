// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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

		RRPointMaterial pointMaterial;
		object->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
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

} // namespace
