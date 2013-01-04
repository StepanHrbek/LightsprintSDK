// --------------------------------------------------------------------------
// Final gather support.
// Copyright (c) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef GATHERER_H
#define GATHERER_H

#include "../RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRStaticSolver/rrcore.h" // optional direct access to materials in rrcore

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// RRCollisionHandlerFinalGathering
//
//! Collision handler for final gathering.
//!
//! It has two modes of operation, setHemisphere() or setLight() must be called later.
//!
//! Thread safe: no, use one handler per thread. So for n threads use 1 collider, n rays and n handlers.

class RRCollisionHandlerFinalGathering : public RRCollisionHandler
{
public:
	RRCollisionHandlerFinalGathering(const RRObject* _multiObject, unsigned _quality, bool _staticSceneContainsLods)
	{
		multiObject = _multiObject; // Physical
		quality = _quality;
		staticSceneContainsLods = _staticSceneContainsLods;
		shooterTriangleIndex = UINT_MAX; // set manually before intersect

		// gathering hemisphere
		triangle = NULL;

		// gathering light
		light = NULL;
		singleObjectReceiver = NULL;
	}

	void setShooterTriangle(unsigned t)
	{
		if (shooterTriangleIndex!=t)
		{
			shooterTriangleIndex = t;
			multiObject->getTriangleLod(t,shooterLod);
		}
	}

	//! Configures handler for gathering illumination from hemisphere.
	//
	//! When _staticSolver is set, handler finds closest receiver for given emitor.
	//! Supports optional point details (e.g. alpha keying) provided by RRObject::getPointMaterial().
	//! Finds closest surface with RRMaterial::sideBits::catchFrom && triangleIndex!=emitorTriangleIndex.
	void setHemisphere(const RRStaticSolver* _staticSolver)
	{
		triangle = _staticSolver ? _staticSolver->scene->object->triangle : NULL;
	}

	//! Configures handler for gathering illumination from light.
	//
	//! When _singleObjectReceiver and _light is set,
	//! handler calculates visibility (0..1) between begin and end of ray.
	//! getVisibility() returns visibility computed from transparency of penetrated materials.
	//! If illegal side is encountered, ray is terminated(=collision found) and isLegal() returns false.
	//! It is used to test direct visibility from light to receiver, with ray shot from receiver to light (for higher precision).
	void setLight(const RRLight* _light, const RRObject* _singleObjectReceiver)
	{
		light = _light;
		singleObjectReceiver = _singleObjectReceiver;
	}

	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_POINT2D;

		// gathering hemisphere
		result = pointMaterialValid = false;

		// gathering light
		visibility = 1;
	}

	virtual bool collides(const RRRay* ray)
	{
		RR_ASSERT(ray->rayFlags&RRRay::FILL_SIDE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_POINT2D);

		// don't collide with shooter
		if (ray->hitTriangle==shooterTriangleIndex)
			return false;

		// don't collide with wrong LODs
		if (staticSceneContainsLods)
		{
			RRObject::LodInfo shadowCasterLod;
			multiObject->getTriangleLod(ray->hitTriangle,shadowCasterLod);
			if ((shadowCasterLod.base==shooterLod.base && shadowCasterLod.level!=shooterLod.level) // non-shooting LOD of shooter
				|| (shadowCasterLod.base!=shooterLod.base && shadowCasterLod.level)) // non-base LOD of non-shooter
				return false;
		}

		triangleMaterial = triangle
			// gathering hemisphere: don't collide when object has NULL material (illegal)
			? triangle[ray->hitTriangle].surface // read from rrcore, faster than multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL)
			// gathering light: don't collide when object has shadow casting disabled
			: multiObject->getTriangleMaterial(ray->hitTriangle,light,singleObjectReceiver);
		if (!triangleMaterial)
			return false;

		if (triangleMaterial->sideBits[ray->hitFrontSide?0:1].catchFrom)
		{
			// per-pixel materials
			if (quality>=triangleMaterial->minimalQualityForPointMaterials)
			{

				multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
				if (pointMaterial.sideBits[ray->hitFrontSide?0:1].catchFrom)
				{
					// gathering hemisphere
					if (triangle)
						return result = pointMaterialValid = true;

					// gathering light
					legal = pointMaterial.sideBits[ray->hitFrontSide?0:1].legal;
					visibility *= pointMaterial.specularTransmittance.color.avg() * pointMaterial.sideBits[ray->hitFrontSide?0:1].transmitFrom * legal;
					RR_ASSERT(_finite(pointMaterial.specularTransmittance.color.avg()));
					RR_ASSERT(_finite(visibility));
					return !visibility;
				}
			}
			else
			// per-triangle materials
			{
				// gathering hemisphere
				if (triangle)
					return result = true;

				// gathering light
				legal = triangleMaterial->sideBits[ray->hitFrontSide?0:1].legal;
				visibility *= triangleMaterial->specularTransmittance.color.avg() * triangleMaterial->sideBits[ray->hitFrontSide?0:1].transmitFrom * legal;
				RR_ASSERT(_finite(triangleMaterial->specularTransmittance.color.avg()));
				RR_ASSERT(_finite(visibility));
				return !visibility;
			}
		}
		return false;
	}

	virtual bool done()
	{
		return triangle
			// gathering hemisphere
			? result
			// gathering light
			: visibility==0;
	}

	// gathering hemisphere: returns contact material from previous collision
	const RRMaterial* getContactMaterial()
	{
		if (!result) return NULL;
		if (pointMaterialValid) return &pointMaterial;
		return triangleMaterial;
	}

	// gathering light: returns visibility between ends of last ray
	RRReal getVisibility() const
	{
		return visibility;
	}

	// gathering light: returns false when illegal side was contacted 
	bool isLegal() const
	{
		return legal!=0;
	}

private:
	unsigned shooterTriangleIndex;
	RRObject::LodInfo shooterLod;
	const RRObject* multiObject;
	unsigned quality; // 0 to forbid point details, more = use point details more often
	bool staticSceneContainsLods;

	// gathering hemisphere
	Triangle* triangle; // shortcut, direct access to materials in rrcore
	bool result;
	// when collision is found, contact material is stored here:
	const RRMaterial* triangleMaterial;
	RRPointMaterial pointMaterial;
	bool pointMaterialValid;

	// gathering light
	const RRObject* singleObjectReceiver;
	const RRLight* light;
	RRReal visibility;
	unsigned legal;
};


//////////////////////////////////////////////////////////////////////////////
//
// Gatherer
//

// Casts 1 ray with possible reflections/refractions, returns color visible in given direction.

// It is used only by RRDynamicSolver, but for higher speed,
// it reads data directly from RRStaticSolver internals, so it is here.

// For final gathering of many rays, use one gatherer per thread.
// May be used 1000x for 1 final gathered texel, 640x480x for 1 raytraced image...
class Gatherer
{
public:
	// Initializes helper structures for gather().
	//! \param gatherDirectEmitors
	//!  Gather direct exitance from emitors (stored in material).
	//! \param gatherIndirectLight
	//!  Gather indirect exitance (stored in static solver). May include indirect light computed from direct realtime lights, direct emitors, rrlights, env.
	//! \param quality
	//!  Desired illumination quality, used to enable/disable point materials.
	Gatherer(const RRObject* multiObject, const RRStaticSolver* staticSolver, const RRBuffer* environment, const RRScaler* scaler, bool gatherDirectEmitors, bool gatherIndirectLight, bool staticSceneContainsLods, unsigned quality);

	//! Returns color visible in given direction, in physical scale, multiplied by visibility.
	//
	//! May reflect/refract internally until visibility is sufficiently low.
	//! Individual calls to gather() are independent.
	//! \param visibility
	//!  Importance for final exitance, result in physical scale is multiplied by visibility.
	//! \param numBounces
	//!  Unused.
	RRVec3 gatherPhysicalExitance(const RRVec3& eye, const RRVec3& direction, unsigned skipTriangleNumber, RRVec3 visibility, int numBounces);

	// helper structures
	RRRay ray; // aligned, better keep it first
protected:
	RRCollisionHandlerFinalGathering collisionHandlerGatherHemisphere;
	RRObject* object;
	const RRCollider* collider;
	const RRBuffer* environment;
	const RRScaler* scaler;
	bool gatherDirectEmitors;
	bool gatherIndirectLight;
	class Triangle* triangle;
	unsigned triangles;
	RussianRoulette russianRoulette;
};

}; // namespace

#endif

