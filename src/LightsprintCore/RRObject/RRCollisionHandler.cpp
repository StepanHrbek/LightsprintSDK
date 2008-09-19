// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>

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

		const RRMaterial* material = object->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
		if (material)
		{
			// per-pixel materials
			if (material->sideBits[ray->hitFrontSide?0:1].pointDetails)
			{
				RRMaterial pointMaterial;
				object->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
				if (pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom)
				{
					return result = true;
				}
			}
			else
			// per-triangle materials
			{
				if (material->sideBits[ray->hitFrontSide?0:1].renderFrom)
				{
					return result = true;
				}
			}
		}
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


//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRCollisionHandler* RRObject::createCollisionHandlerFirstVisible() const
{
	return new RRCollisionHandlerFirstVisible(this);
}

} // namespace
