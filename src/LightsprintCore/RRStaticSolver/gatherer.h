// --------------------------------------------------------------------------
// Final gather support.
// Copyright (c) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef GATHERER_H
#define GATHERER_H

#include "../RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRStaticSolver/rrcore.h" // optional direct access to materials in rrcore

//#define COLLISION_LOG(x) x
#define COLLISION_LOG(x)
COLLISION_LOG(#include <sstream>)

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
	COLLISION_LOG(std::stringstream log);
	RRCollisionHandlerFinalGathering(const RRScaler* _scaler, unsigned _quality, bool _staticSceneContainsLods)
	{
		scaler = _scaler;
		quality = _quality;
		staticSceneContainsLods = _staticSceneContainsLods;
		shooterObject = NULL;
		shooterTriangleIndex = UINT_MAX; // set manually before intersect

		// gathering hemisphere
		triangle = NULL;

		// gathering light
		light = NULL;
		singleObjectReceiver = NULL;
	}

	void setShooterTriangle(const RRObject* _object, unsigned _triangle)
	{
		COLLISION_LOG(log<<"setShooterTriangle("<<t<<")\n");
		if (shooterTriangleIndex!=_triangle || shooterObject!=_object)
		{
			shooterObject = _object;
			shooterTriangleIndex = _triangle;
			if (shooterObject)
				shooterObject->getTriangleLod(shooterTriangleIndex,shooterLod);

			// initialize detector of identical triangles
			shooterVertexLoaded = false;
		}
	}

	//! Configures handler for gathering illumination from hemisphere (collides when hitSide has renderFrom).
	//
	//! When _staticSolver is set, handler finds closest receiver for given emitor.
	//! Supports optional point details (e.g. alpha keying) provided by RRObject::getPointMaterial().
	//! Finds closest surface with RRMaterial::sideBits::renderFrom && triangleIndex!=emitorTriangleIndex.
	void setHemisphere(const RRStaticSolver* _staticSolver)
	{
		COLLISION_LOG(log<<"setHemisphere()\n");
		triangle = _staticSolver ? _staticSolver->scene->object->triangle : NULL;
	}

	//! Configures handler for gathering illumination from light (collides when any side has renderFrom).
	//
	//! When _singleObjectReceiver and _light is set,
	//! handler calculates visibility (0..1) between begin and end of ray.
	//! getVisibility() returns visibility computed from transparency of penetrated materials.
	//! If illegal side is encountered, ray is terminated(=collision found) and isLegal() returns false.
	//! It is used to test direct visibility from light to receiver, with ray shot from receiver to light (for higher precision).
	void setLight(const RRLight* _light, const RRObject* _singleObjectReceiver)
	{
		RR_ASSERT(_light);
		//RR_ASSERT(_singleObjectReceiver); not necessary, only for checking light-object shadow casting flags
		COLLISION_LOG(log<<"setLight()\n");
		light = _light;
		singleObjectReceiver = _singleObjectReceiver;
	}

	// Tests one side after setHemisphere(), two sides after setLight().
	#define TEST_BIT(material,bit) ((material)->sideBits[ray->hitFrontSide?0:1].bit || (light && (material)->sideBits[ray->hitFrontSide?1:0].bit))

	virtual void init(RRRay* ray)
	{
		COLLISION_LOG(log<<"init()\n");
		ray->rayFlags |= RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_TRIANGLE|RRRay::FILL_POINT2D;

		// gathering hemisphere
		firstContactMaterial = NULL;

		// gathering light
		visibility = RRVec3(1);
	}

	virtual bool collides(const RRRay* ray)
	{
		COLLISION_LOG(log<<"origin="<<ray->rayOrigin.x<<" "<<ray->rayOrigin.y<<" "<<ray->rayOrigin.z<<" dir="<<ray->rayDir.x<<" "<<ray->rayDir.y<<" "<<ray->rayDir.z<<" hitDistance="<<ray->hitDistance<<" hitTriangle="<<ray->hitTriangle<<"  ");
		RR_ASSERT(ray->rayFlags&RRRay::FILL_DISTANCE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_SIDE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_TRIANGLE);
		RR_ASSERT(ray->rayFlags&RRRay::FILL_POINT2D);

		// don't collide with shooter
		if (ray->hitTriangle==shooterTriangleIndex && ray->hitObject==shooterObject)
		{
			COLLISION_LOG(log<<"collides()=false1\n");
			return false;
		}
		const RRObject* hitObject = ray->hitObject;
		// don't collide with other triangles at the same location
		if (shooterObject && shooterTriangleIndex!=UINT_MAX
			//&& triangle[shooterTriangleIndex].area==triangle[ray->hitTriangle].area // optimization, but too dangerous, areas of identical triangles might differ because of different vertex order
			&& ray->hitDistance<1000*ray->rayLengthMin) // optimization, perform these tests only for hits in small distance (ideally zero, but there is floating point error). we expect our caller to set rayLengthMin to minimalSafeDistance; float error in scene drezy is 1000x bigger
		{
			if (!shooterVertexLoaded)
			{
				const RRMesh* shooterMesh = shooterObject->getCollider()->getMesh();
				RRMesh::Triangle t;
				shooterMesh->getTriangle(shooterTriangleIndex,t);
				shooterMesh->getVertex(t[0],shooterVertex[0]);
				shooterMesh->getVertex(t[1],shooterVertex[1]);
				shooterMesh->getVertex(t[2],shooterVertex[2]);
				shooterVertexLoaded = true;
			}
			const RRMesh* hitMesh = hitObject->getCollider()->getMesh();
			RRMesh::Triangle t;
			hitMesh->getTriangle(ray->hitTriangle,t);
			for (unsigned i=0;i<3;i++)
			{
				RRVec3 hitVertex;
				hitMesh->getVertex(t[i],hitVertex);
				if (hitVertex!=shooterVertex[0] && hitVertex!=shooterVertex[1] && hitVertex!=shooterVertex[2])
					goto not_identical;
			}
			COLLISION_LOG(log<<"collides()=false2\n");
			return false;
			not_identical:;
		}

		// don't collide with wrong LODs
		if (staticSceneContainsLods)
		{
			RRObject::LodInfo shadowCasterLod;
			hitObject->getTriangleLod(ray->hitTriangle,shadowCasterLod);
			if ((shadowCasterLod.base==shooterLod.base && shadowCasterLod.level!=shooterLod.level) // non-shooting LOD of shooter
				|| (shadowCasterLod.base!=shooterLod.base && shadowCasterLod.level)) // non-base LOD of non-shooter
			{
				COLLISION_LOG(log<<"collides()=false3\n");
				return false;
			}
		}

		const RRMaterial* triangleMaterial = (triangle && hitObject && !hitObject->isDynamic) // we can access solver's material only for static objects
			// gathering hemisphere: don't collide when triangle has surface=NULL (triangle was rejected by setGeometry, everything should work as if it does not exist)
			? triangle[ray->hitTriangle].surface // read from rrcore, faster than hitObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL)
			// gathering light: don't collide when object has shadow casting disabled
			: hitObject->getTriangleMaterial(ray->hitTriangle,light,singleObjectReceiver);
		if (!triangleMaterial)
		{
			COLLISION_LOG(log<<"collides()=false4\n");
			return false;
		}
		if (TEST_BIT(triangleMaterial,renderFrom))
		{
			// per-pixel materials
			if (quality>=triangleMaterial->minimalQualityForPointMaterials)
			{
				unsigned pmi = (firstContactMaterial==pointMaterial)?1:0; // index into pointMaterial[], one that is not occupied by firstContactMaterial
				hitObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial[pmi],scaler);
				if (TEST_BIT(&pointMaterial[pmi],renderFrom))
				{
					// gathering hemisphere
					if (!light)
					{
						COLLISION_LOG(log<<"collides()=true1\n");
						firstContactMaterial = &pointMaterial[pmi];
						return true;
					}
					// gathering light
					legal = pointMaterial[pmi].sideBits[ray->hitFrontSide?0:1].legal;
					visibility *= pointMaterial[pmi].specularTransmittance.colorPhysical * RRReal( pointMaterial[pmi].sideBits[ray->hitFrontSide?0:1].transmitFrom * legal );
					RR_ASSERT(IS_VEC3(pointMaterial[pmi].specularTransmittance.colorPhysical));
					RR_ASSERT(IS_VEC3(visibility));
					COLLISION_LOG(log<<"collides()=?1\n");
					return visibility==RRVec3(0);
				}
			}
			else
			// per-triangle materials
			{
				// gathering hemisphere
				if (!light)
				{
					COLLISION_LOG(log<<"collides()=true2\n");
					firstContactMaterial = triangleMaterial;
					return true;
				}
				// gathering light
				legal = triangleMaterial->sideBits[ray->hitFrontSide?0:1].legal;
				visibility *= triangleMaterial->specularTransmittance.colorPhysical * RRReal( triangleMaterial->sideBits[ray->hitFrontSide?0:1].transmitFrom * legal );
				RR_ASSERT(IS_VEC3(triangleMaterial->specularTransmittance.colorPhysical));
				RR_ASSERT(IS_VEC3(visibility));
				COLLISION_LOG(log<<"collides()=?2\n");
				return visibility==RRVec3(0);
			}
		}
		COLLISION_LOG(log<<"collides()=false\n");
		return false;
	}

	virtual bool done()
	{
		COLLISION_LOG(log<<"done()="<<(!light?firstContactMaterial!=NULL:visibility==RRVec3(0))<<"\n\n");
		return !light
			// gathering hemisphere
			? firstContactMaterial!=NULL
			// gathering light
			: visibility==RRVec3(0);
	}

	// gathering hemisphere: returns contact material from previous collision
	const RRMaterial* getContactMaterial()
	{
		return firstContactMaterial;
	}

	// gathering light: returns visibility between ends of last ray
	RRVec3 getVisibility() const
	{
		return visibility;
	}

	// gathering light: returns false when illegal side was contacted 
	bool isLegal() const
	{
		return legal!=0;
	}

private:
	RRObject::LodInfo shooterLod;
	const RRScaler* scaler;
	unsigned quality; // 0 to forbid point details, more = use point details more often, UINT_MAX = always
	bool staticSceneContainsLods;

	// detector of triangles identical to shooter
	const RRObject* shooterObject;
	unsigned shooterTriangleIndex;
	bool shooterVertexLoaded;
	RRVec3 shooterVertex[3];

	// gathering hemisphere
	Triangle* triangle; // shortcut, direct access to materials in rrcore
	const RRMaterial* firstContactMaterial; // when collision is found, contact material is stored here:
	RRPointMaterial pointMaterial[2]; // helper for storing contact material. one slot for old accepted contact, one slot for new not-yet-accepted contact

	// gathering light
	const RRObject* singleObjectReceiver;
	const RRLight* light;
	RRVec3 visibility;
	unsigned legal;
};


//////////////////////////////////////////////////////////////////////////////
//
// Gatherer
//

// Casts 1 ray with possible reflections/refractions, returns color visible in given direction.

// It is used only by RRSolver, but for higher speed,
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
	Gatherer(const RRObject* multiObject, const RRStaticSolver* staticSolver, const RRBuffer* environment, const RRScaler* scaler, RRReal gatherDirectEmitors, RRReal gatherIndirectLight, bool staticSceneContainsLods, unsigned quality);

	//! Returns color visible in given direction, in physical scale, multiplied by visibility.
	//
	//! May reflect/refract internally until visibility is sufficiently low.
	//! Individual calls to gather() are independent.
	//! \param visibility
	//!  Importance for final exitance, result in physical scale is multiplied by visibility.
	//! \param numBounces
	//!  Unused.
	RRVec3 gatherPhysicalExitance(const RRVec3& eye, const RRVec3& direction, const RRObject* shooterObject, unsigned shooterTriangle, RRVec3 visibility, unsigned numBounces);

	// helper structures
	RRRay ray; // aligned, better keep it first
protected:
	RRCollisionHandlerFinalGathering collisionHandlerGatherHemisphere;
	const RRObject* multiObject;
	const RRCollider* collider;
	const RRBuffer* environment;
	const RRScaler* scaler;
	bool gatherDirectEmitors;
	bool gatherIndirectLight;
	RRReal gatherDirectEmitorsMultiplier;
	RRReal gatherIndirectLightMultiplier;
	class Triangle* triangle;
	unsigned triangles;
	RussianRoulette russianRoulette;
};

}; // namespace

#endif

