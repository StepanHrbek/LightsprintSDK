// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Pathtracer.
// --------------------------------------------------------------------------


#ifndef PATHTRACER_H
#define PATHTRACER_H

#include "../RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRStaticSolver/rrcore.h" // optional direct access to materials in rrcore

#define MATERIAL_BACKGROUND_HACK // pathtracer renders material with name "background" with background color
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
	RRCollisionHandlerFinalGathering(const RRColorSpace* _colorSpace, unsigned _qualityForPointMaterials, unsigned _qualityForInterpolation, bool _staticSceneContainsLods)
	{
		colorSpace = _colorSpace;
		qualityForPointMaterials = _qualityForPointMaterials;
		qualityForInterpolation = _qualityForInterpolation;
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
	#define TEST_BIT(material,bit) ((material)->sideBits[ray->hitFrontSide?0:1].bit || (light && (material)->sideBits[ray->hitFrontSide?1:0].bit)) // [#45] shadowRays collide with both sides

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

		const RRObject* hitObject = ray->hitObject;
		RR_ASSERT(hitObject);

		// don't collide with disabled objects
		if (!hitObject->enabled)
			return false;

		// don't collide with shooter
		if (ray->hitTriangle==shooterTriangleIndex && hitObject==shooterObject)
		{
			COLLISION_LOG(log<<"collides()=false1\n");
			return false;
		}
		// don't collide with other triangles at the same location
		if (shooterObject && shooterTriangleIndex!=UINT_MAX
			//&& triangle[shooterTriangleIndex].area==triangle[ray->hitTriangle].area // optimization, but too dangerous, areas of identical triangles might differ because of different vertex order
			&& ray->hitDistance<1000*ray->rayLengthMin) // [#38] optimization, perform these tests only for hits in small distance (ideally zero, but there is floating point error). we expect our caller to set rayLengthMin to minimalSafeDistance; float error in scene drezy is 1000x bigger
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
				if (hitVertex!=shooterVertex[0] && hitVertex!=shooterVertex[1] && hitVertex!=shooterVertex[2]) //!!! for dynamic objects, we probably compare localspace position, i.e. we could skip valid hit if instances of the same mesh are very close (e.g. overlap with 1mm shift)
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

		const RRMaterial* triangleMaterial = (triangle && !hitObject->isDynamic) // we can access solver's material only for static objects
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
			if (qualityForPointMaterials>triangleMaterial->minimalQualityForPointMaterials)
			{
				unsigned pmi = (firstContactMaterial==pointMaterial)?1:0; // index into pointMaterial[], one that is not occupied by firstContactMaterial
				hitObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,colorSpace,qualityForInterpolation>triangleMaterial->minimalQualityForPointMaterials,pointMaterial[pmi]);
				if (TEST_BIT(&pointMaterial[pmi],renderFrom))
				{
					// gathering hemisphere
					if (!light)
					{
						// this could speed up rays through holes, but it needs testing
						//if (pointMaterial[pmi].specularTransmittance.texture && visibility==RRVec3(1))
						//	return false;
						COLLISION_LOG(log<<"collides()=true1\n");
						firstContactMaterial = &pointMaterial[pmi];
						return true;
					}
					// gathering light
					//  this collison handler cares only about transmittance, but getPointMaterial reads all textures
					//  it would be nice to send some flag to getPointMaterial above, to read only transmittance, but we don't have such flag yet
					legal = pointMaterial[pmi].sideBits[ray->hitFrontSide?0:1].legal;
					visibility *= pointMaterial[pmi].specularTransmittance.colorLinear * RRReal( pointMaterial[pmi].sideBits[ray->hitFrontSide?0:1].transmitFrom * legal );
					RR_ASSERT(IS_VEC3(pointMaterial[pmi].specularTransmittance.colorLinear));
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
				visibility *= triangleMaterial->specularTransmittance.colorLinear * RRReal( triangleMaterial->sideBits[ray->hitFrontSide?0:1].transmitFrom * legal );
				RR_ASSERT(IS_VEC3(triangleMaterial->specularTransmittance.colorLinear));
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
	const RRColorSpace* colorSpace;
	unsigned qualityForPointMaterials; // 0 to forbid point details, more = use point details more often, UINT_MAX = always
	unsigned qualityForInterpolation;
	bool staticSceneContainsLods;

	// detector of triangles identical to shooter
	const RRObject* shooterObject;
	unsigned shooterTriangleIndex;
	bool shooterVertexLoaded;
	RRVec3 shooterVertex[3];

	// gathering hemisphere, pathtracing
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
// PathtracerJob
//
// constant data shared by all workers/threads

class PathtracerJob
{
public:
	PathtracerJob(const RRSolver* solver, bool dynamic);
	~PathtracerJob();
	
	const RRSolver* solver;
	const RRColorSpace* colorSpace;
	const RRBuffer* environment; // blend of two rotated solver environments
	const RRCollider* collider;

#ifdef MATERIAL_BACKGROUND_HACK
	RRVec3 environmentAveragePhysical;
#endif
};


//////////////////////////////////////////////////////////////////////////////
//
// PathtracerWorker
//

// Casts 1 ray with possible reflections/refractions/shadowrays, returns color visible in given direction.

// It is used only by RRSolver, but for higher speed,
// it can read data directly from RRStaticSolver or RRPackedSolver internals, so it is here.

// For final gathering of many rays, use one worker per thread or per pixel, whatever is easier.
// Can be used 1000x for 1 final gathered texel, 640x480x for 1 raytraced image...
class PathtracerWorker
{
public:
	// Initializes helper structures for gather().
	//! \param gatherDirectEmitors
	//!  Gather direct exitance from emitors (stored in material).
	//! \param gatherIndirectLight
	//!  Gather indirect exitance (stored in static solver). May include indirect light computed from direct realtime lights, direct emitors, rrlights, env.
	//! \param qualityForPointMaterials
	//!  Desired illumination quality, used to enable/disable point materials.
	//! \param qualityForInterpolation
	//!  Desired illumination quality, used to enable/disable point material interpolation.
	PathtracerWorker(const PathtracerJob& ptj, const RRSolver::PathTracingParameters& parameters, bool staticSceneContainsLods, unsigned qualityForPointMaterials, unsigned qualityForInterpolation);

	//! Returns color visible in given direction, in physical scale.
	//
	//! May reflect/refract internally until visibility is sufficiently low.
	//! Individual calls to gather() are independent.
	//! \param visibility
	//!  Importance for final exitance, result in physical scale is multiplied by visibility.
	//! \param numBounces
	//!  Current recursion depth.
	RRVec3 getIncidentRadiance(const RRVec3& eye, const RRVec3& direction, const RRObject* shooterObject, unsigned shooterTriangle, RRVec3 visibility = RRVec3(1), unsigned numBounces = 0);

	RRRay ray; // aligned, better keep it first
protected:
	const PathtracerJob& ptj;
	const RRSolver::PathTracingParameters& parameters;
	RRCollisionHandlerFinalGathering collisionHandlerGatherHemisphere;
	RRCollisionHandlerFinalGathering collisionHandlerGatherLights;
	const RRObject* multiObject;
	const RRLights* lights;
	const RRPackedSolver* packedSolver; // shortcut for accessing indirect in Fireball solver
	class Triangle* triangle; // shortcut for accessing indirect in Architect solver

#ifdef MATERIAL_BACKGROUND_HACK
	bool bouncedOffInvisiblePlane;
#endif
};

}; // namespace

#endif

