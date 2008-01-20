// --------------------------------------------------------------------------
// Response to ray-mesh collision.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// SkipTriangle

class SkipTriangle : public RRCollisionHandler
{
public:
	virtual void init()
	{
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		result = result || (ray->hitTriangle!=skip);
		return ray->hitTriangle!=skip;
	}
	virtual bool done()
	{
		return result;
	}
	unsigned skip;
private:
	bool result;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerFirstReceiver
//
//! Creates and returns collision handler, that finds closest receiver for given emitor.
//
//! Supports optional point details (e.g. alpha keying) provided by RRObject::getPointMaterial().
//! \n Finds closest surface with RRMaterial::sideBits::catchFrom && triangleNumber!=emitorTriangleNumber.
//!
//! Thread safe: this function yes, but created collision handler no.
//!  Typical use case is: for n threads, use 1 collider, n rays and n handlers.
//! \param emitorTriangleNumber
//!  Number of triangle that never collides, it is skipped. It is usually used for emitor where ray begins.
//! \param allowPointMaterials
//!  True to use point materials when available, false to always use only per-triangle materials.

class RRCollisionHandlerFirstReceiver : public RRCollisionHandler
{
public:
	RRCollisionHandlerFirstReceiver(const RRObject* _multiObject,unsigned _emitorTriangleNumber,bool _allowPointMaterials)
	{
		multiObject = _multiObject;
		emitorTriangleNumber = _emitorTriangleNumber;
		allowPointMaterials = _allowPointMaterials;
	}
	virtual void init()
	{
		result = pointMaterialValid = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		if(ray->hitTriangle!=emitorTriangleNumber)
		{
			triangleMaterial = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
			if(triangleMaterial)
			{
				// per-pixel materials
				if(allowPointMaterials && triangleMaterial->sideBits[ray->hitFrontSide?0:1].pointDetails)
				{
					// optional ray->hitPoint2d must be filled
					// this is satisfied on 2 external places:
					//   - existing users request 2d to be filled
					//   - existing colliders fill hitPoint2d even when not requested by user
					multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
					if(pointMaterial.sideBits[ray->hitFrontSide?0:1].catchFrom)
					{
						return result = pointMaterialValid = true;
					}
				}
				else
				// per-triangle materials
				{
					if(triangleMaterial->sideBits[ray->hitFrontSide?0:1].catchFrom)
					{
						return result = true;
					}
				}
			}
		}
		return false;
	}
	virtual bool done()
	{
		return result;
	}

	// returns contact material from previous collision
	const RRMaterial* getContactMaterial()
	{
		if(!result) return NULL;
		if(pointMaterialValid) return &pointMaterial;
		return triangleMaterial;
	}

private:
	bool result;
	const RRObject* multiObject;
	bool allowPointMaterials;
	unsigned emitorTriangleNumber;

	// when collision is found, contact material is stored here:
	const RRMaterial* triangleMaterial;
	RRMaterial pointMaterial;
	bool pointMaterialValid;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerVisibility
//
//! Calculates visibility (0..1) between begin and end of ray.
//
//! getVisibility() returns visibility computed from transparency of penetrated materials.
//! If illegal side is encountered, ray is terminated(=collision found) and isLegal() returns false.
//!
//! Used to test direct visibility from light to receiver, with ray shot from receiver to light (for higher precision).

class RRCollisionHandlerVisibility : public RRCollisionHandler
{
public:
	RRCollisionHandlerVisibility(const RRObject* _multiObject,const RRObject* _singleObjectReceiver,const RRLight* _light,unsigned _emitorTriangleNumber,bool _allowPointMaterials)
	{
		multiObject = _multiObject;
		singleObjectReceiver = _singleObjectReceiver;
		light = _light;
		emitorTriangleNumber = _emitorTriangleNumber;
		allowPointMaterials = _allowPointMaterials;
	}
	virtual void init()
	{
		visibility = 1;
	}
	virtual bool collides(const RRRay* ray)
	{
		if(ray->hitTriangle!=emitorTriangleNumber)
		{
			const RRMaterial* triangleMaterial = multiObject->getTriangleMaterial(ray->hitTriangle,light,singleObjectReceiver);
			if(triangleMaterial)
			{
				// per-pixel materials
				if(allowPointMaterials && triangleMaterial->sideBits[ray->hitFrontSide?0:1].pointDetails)
				{
					// optional ray->hitPoint2d must be filled
					// this is satisfied on 2 external places:
					//   - existing users request 2d to be filled
					//   - existing colliders fill hitPoint2d even when not requested by user
					RRMaterial pointMaterial;
					multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
					if(pointMaterial.sideBits[ray->hitFrontSide?0:1].catchFrom)
					{
						legal = pointMaterial.sideBits[ray->hitFrontSide?0:1].legal;
						visibility *= pointMaterial.specularTransmittance.avg() * pointMaterial.sideBits[ray->hitFrontSide?0:1].transmitFrom * legal;
						return !visibility;
					}
				}
				else
				// per-triangle materials
				{
					if(triangleMaterial->sideBits[ray->hitFrontSide?0:1].catchFrom)
					{
						legal = triangleMaterial->sideBits[ray->hitFrontSide?0:1].legal;
						visibility *= triangleMaterial->specularTransmittance.avg() * triangleMaterial->sideBits[ray->hitFrontSide?0:1].transmitFrom * legal;
						return !visibility;
					}
				}
			}
		}
		return false;
	}
	virtual bool done()
	{
		return visibility==0;
	}

	// returns visibility between ends of last ray
	RRReal getVisibility() const
	{
		return visibility;
	}

	// returns false when illegal side was contacted 
	bool isLegal() const
	{
		return legal!=0;
	}


	const RRLight* light;
private:
	const RRObject* multiObject;
	const RRObject* singleObjectReceiver;
	bool allowPointMaterials;
	unsigned emitorTriangleNumber;

	RRReal visibility;
	unsigned legal;
};

} // namespace
