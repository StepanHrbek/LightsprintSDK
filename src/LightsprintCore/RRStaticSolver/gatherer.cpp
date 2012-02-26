// --------------------------------------------------------------------------
// Final gather support.
// Copyright (c) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "gatherer.h"
#include "rrcore.h"

namespace rr
{

extern RRVec3 refract(RRVec3 N,RRVec3 I,real r);

Gatherer::Gatherer(const RRObject* _multiObject, const RRStaticSolver* _staticSolver, const RRBuffer* _environment, const RRScaler* _scaler, bool _gatherDirectEmitors, bool _gatherIndirectLight, bool _staticSceneContainsLods, unsigned _quality)
	: collisionHandlerGatherHemisphere(_multiObject,_staticSolver,_quality,_staticSceneContainsLods)
{
	ray.collisionHandler = &collisionHandlerGatherHemisphere;
	ray.rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_PLANE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE;
	environment = _environment;
	scaler = _scaler;
	gatherDirectEmitors = _gatherDirectEmitors;
	gatherIndirectLight = _gatherIndirectLight;
	Object* _object = _staticSolver->scene->object;
	object = _object->importer;
	collider = object->getCollider();
	triangle = _object->triangle;
	triangles = _object->triangles;
	ray.hitObject = object;

	// final gather in lightmap does this per-pixel, rather than per-thread
	//  so at very low quality, lightmaps are biased smooth rather than unbiased noisy
	//  bias is related to 1/quality
	russianRoulette.reset();
}

RRVec3 Gatherer::gatherPhysicalExitance(const RRVec3& eye, const RRVec3& direction, unsigned skipTriangleIndex, RRVec3 visibility, unsigned numBounces)
{
	RR_ASSERT(IS_VEC3(eye));
	RR_ASSERT(IS_VEC3(direction));
	RR_ASSERT(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir
	if (!triangles)
	{
		// although we may dislike it, somebody may feed objects with no faces which confuses intersect_bsp
		RR_ASSERT(0);
		return Channels(0);
	}
	ray.rayOrigin = eye;
	ray.rayDir = direction;
	//ray.hitObject = already set in ctor
	collisionHandlerGatherHemisphere.setShooterTriangle(skipTriangleIndex);
	if (!collider->intersect(&ray))
	{
		// ray left scene
		if (environment)
		{
			RRVec3 irrad = environment->getElementAtDirection(direction);
			if (scaler && environment->getScaled()) scaler->getPhysicalScale(irrad);
			return visibility * irrad;
		}
		return Channels(0);
	}

	//LOG_RAY(eye,direction,hitTriangle?ray.hitDistance:0.2f,hitTriangle);
	RR_ASSERT(IS_NUMBER(ray.hitDistance));
	Triangle* hitTriangle = &triangle[ray.hitTriangle];
	const RRMaterial* material = collisionHandlerGatherHemisphere.getContactMaterial(); // could be point detail, unlike hitTriangle->surface 
	RRSideBits side=material->sideBits[ray.hitFrontSide?0:1];
	Channels exitance = Channels(0);
	if (side.legal && (side.catchFrom || side.emitTo))
	{
		// work with ray+material before we recurse and overwrite them
		bool   specularReflect = false;
		RRVec3 specularReflectPower;
		RRReal specularReflectMax;
		RRVec3 specularReflectDir;
		bool   specularTransmit = false;
		RRVec3 specularTransmitPower;
		RRReal specularTransmitMax;
		RRVec3 specularTransmitDir;
		if (side.reflect)
		{
			specularReflectPower = visibility*material->specularReflectance.color;
			specularReflectMax = specularReflectPower.maxi();
			specularReflect = russianRoulette.survived(specularReflectMax);
			if (specularReflect)
			{
				// calculate new direction after ideal mirror reflection
				specularReflectDir = RRVec3(ray.hitPlane)*(-2*dot(direction,RRVec3(ray.hitPlane))/size2(RRVec3(ray.hitPlane)))+direction;
			}
		}
		if (side.transmitFrom)
		{
			specularTransmitPower = visibility*material->specularTransmittance.color;
			specularTransmitMax = specularTransmitPower.maxi();
			specularTransmit = russianRoulette.survived(specularTransmitMax);
			if (specularTransmit)
			{
				// calculate new direction after refraction
				specularTransmitDir = -refract(ray.hitPlane,direction,material->refractionIndex);
			}
		}
		RRVec3 hitPoint3d=eye+direction*ray.hitDistance;

		// copy remaining ray+material data to local stack
		unsigned rayHitTriangle = ray.hitTriangle;

		// diffuse reflection + emission
		if (side.emitTo)
		{
			// we emit everything to both sides of 2sided face, thus doubling energy
			// this may be changed later
			//float splitToTwoSides = material->sideBits[ray.hitFrontSide?1:0].emitTo ? 0.5f : 1;

			// diffuse emission
			if (gatherDirectEmitors)
			{
				// used in direct lighting final gather [per pixel emittance]
				exitance += visibility * material->diffuseEmittance.color;// * splitToTwoSides;
			}

			// diffuse reflection
			if (gatherIndirectLight)
			{
				// used in GI final gather
				{
					// point detail version
					exitance += visibility * hitTriangle->getTotalIrradiance() * material->diffuseReflectance.color;// * splitToTwoSides;
					// per triangle version (ignores point detail even if it's already available)
					//exitance += visibility * hitTriangle->totalExitingFlux / hitTriangle->area;
				}
			}
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}

		if (specularReflect)
		{
			// recursively call this function
			exitance += gatherPhysicalExitance(hitPoint3d,specularReflectDir,rayHitTriangle,specularReflectPower/specularReflectMax,numBounces-1);
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}

		if (specularTransmit)
		{
			// recursively call this function
			exitance += gatherPhysicalExitance(hitPoint3d,specularTransmitDir,rayHitTriangle,specularTransmitPower/specularTransmitMax,numBounces-1);
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}
	}
	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	return exitance;
}

}; // namespace

