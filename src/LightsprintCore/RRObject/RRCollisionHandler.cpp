// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>

#include "RRCollisionHandler.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerFirstVisible

class RRCollisionHandlerFirstVisible : public RRCollisionHandler
{
public:
	RRCollisionHandlerFirstVisible(const RRObject* aobject)
	{
		object = aobject;
	}
	virtual void init()
	{
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		const RRMaterial* material = object->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
		if(material)
		{
			// per-pixel materials
			if(material->sideBits[ray->hitFrontSide?0:1].pointDetails)
			{
				// optional ray->hitPoint2d must be filled
				// this is satisfied by existing colliders, they fill hitPoint2d even when not requested by user
				RRMaterial pointMaterial;
				object->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
				if(pointMaterial.sideBits[ray->hitFrontSide?0:1].renderFrom)
				{
					return result = true;
				}
			}
			else
			// per-triangle materials
			{
				if(material->sideBits[ray->hitFrontSide?0:1].renderFrom)
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

RRCollisionHandler* RRObject::createCollisionHandlerFirstVisible()
{
	return new RRCollisionHandlerFirstVisible(this);
}

} // namespace
