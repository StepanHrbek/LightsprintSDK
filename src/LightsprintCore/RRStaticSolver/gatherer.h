// --------------------------------------------------------------------------
// Final gather support.
// Copyright 2007-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
// RRCollisionHandlerGatherHemisphere
//
//! Creates and returns collision handler, that finds closest receiver for given emitor.
//
//! Supports optional point details (e.g. alpha keying) provided by RRObject::getPointMaterial().
//! \n Finds closest surface with RRMaterial::sideBits::catchFrom && triangleIndex!=emitorTriangleIndex.
//!
//! Thread safe: this function yes, but created collision handler no.
//!  Typical use case is: for n threads, use 1 collider, n rays and n handlers.
//! \param allowPointMaterials
//!  True to use point materials when available, false to always use only per-triangle materials.

class RRCollisionHandlerGatherHemisphere : public RRCollisionHandler
{
public:
	RRCollisionHandlerGatherHemisphere(const RRObject* _multiObject, const RRStaticSolver* _staticSolver, bool _allowPointMaterials, bool _staticSceneContainsLods)
	{
		multiObject = _multiObject; // Physical
		triangle = _staticSolver ? _staticSolver->scene->object->triangle : NULL;
		allowPointMaterials = _allowPointMaterials;
		staticSceneContainsLods = _staticSceneContainsLods;
		shooterTriangleIndex = UINT_MAX; // set manually before intersect
	}

	void setShooterTriangle(unsigned t)
	{
		if (shooterTriangleIndex!=t)
		{
			shooterTriangleIndex = t;
			multiObject->getTriangleLod(t,shooterLod);
		}
	}

	virtual void init(RRRay* ray)
	{
		ray->rayFlags |= RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_POINT2D;
		result = pointMaterialValid = false;
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

		// don't collide when object has NULL material (illegal input)
		triangleMaterial = triangle
			? triangle[ray->hitTriangle].surface // read from rrcore, faster
			: multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL); // read from multiobject, slower
		if (!triangleMaterial)
			return false;

		// per-pixel materials
		if (allowPointMaterials && triangleMaterial->sideBits[ray->hitFrontSide?0:1].pointDetails)
		{
			multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
			if (pointMaterial.sideBits[ray->hitFrontSide?0:1].catchFrom)
			{
				return result = pointMaterialValid = true;
			}
		}
		else
		// per-triangle materials
		{
			if (triangleMaterial->sideBits[ray->hitFrontSide?0:1].catchFrom)
			{
				return result = true;
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
		if (!result) return NULL;
		if (pointMaterialValid) return &pointMaterial;
		return triangleMaterial;
	}

private:
	unsigned shooterTriangleIndex;
	RRObject::LodInfo shooterLod;
	bool result;
	const RRObject* multiObject;
	Triangle* triangle; // shortcut, direct access to materials in rrcore
	bool allowPointMaterials;
	bool staticSceneContainsLods;

	// when collision is found, contact material is stored here:
	const RRMaterial* triangleMaterial;
	RRMaterial pointMaterial;
	bool pointMaterialValid;
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
	//! \param ray
	//!  rayLengthMin and rayLengthMax are inputs, other attributes are changed.
	//! \param gatherDirectEmitors
	//!  Gather direct exitance from emitors (stored in material).
	//! \param gatherIndirectLight
	//!  Gather indirect exitance (stored in static solver). May include indirect light computed from direct realtime lights, direct emitors, rrlights, env.
	Gatherer(RRRay* ray, const RRObject* multiObject, const RRStaticSolver* staticSolver, const RRBuffer* environment, const RRScaler* scaler, bool gatherDirectEmitors, bool gatherIndirectLight, bool staticSceneContainsLods);

	//! Returns color visible in given direction, in physical scale, multiplied by visibility.
	//
	//! May reflect/refract internally until visibility is sufficiently low.
	//! Individual calls to gather() are independent.
	//! \param visibility
	//!  Importance for final exitance, result in physical scale is multiplied by visibility.
	RRVec3 gatherPhysicalExitance(RRVec3 eye, RRVec3 direction, unsigned skipTriangleNumber, RRVec3 visibility);

protected:
	// helper structures
	RRRay* ray;
	RRCollisionHandlerGatherHemisphere collisionHandlerGatherHemisphere;
	RRObject* object;
	const RRCollider* collider;
	const RRBuffer* environment;
	const RRScaler* scaler;
	bool gatherDirectEmitors;
	bool gatherIndirectLight;
	class Triangle* triangle;
	unsigned triangles;
};

}; // namespace

#endif

