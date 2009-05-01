// --------------------------------------------------------------------------
// Final gather support.
// Copyright (c) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "gatherer.h"
#include "rrcore.h"

namespace rr
{

extern RRVec3 refract(RRVec3 N,RRVec3 I,real r);

// survivalProbability is in 0..1 range
static bool survivedRussianRoulette(float survivalProbability)
{
	return rand()<(int)(RAND_MAX*survivalProbability);
}

Gatherer::Gatherer(RRRay* _ray, const RRObject* _multiObject, const RRStaticSolver* _staticSolver, const RRBuffer* _environment, const RRScaler* _scaler, bool _gatherDirectEmitors, bool _gatherIndirectLight, bool _staticSceneContainsLods, unsigned _quality)
	: collisionHandlerGatherHemisphere(_multiObject,_staticSolver,_quality,_staticSceneContainsLods)
{
	ray = _ray;
	ray->collisionHandler = &collisionHandlerGatherHemisphere;
	ray->rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_PLANE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE;
	environment = _environment;
	scaler = _scaler;
	gatherDirectEmitors = _gatherDirectEmitors;
	gatherIndirectLight = _gatherIndirectLight;
	Object* _object = _staticSolver->scene->object;
	object = _object->importer;
	collider = object->getCollider();
	triangle = _object->triangle;
	triangles = _object->triangles;
}

RRVec3 Gatherer::gatherPhysicalExitance(RRVec3 eye, RRVec3 direction, unsigned skipTriangleIndex, RRVec3 visibility)
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
	ray->rayOrigin = eye;
	ray->rayDirInv[0] = 1/direction[0];
	ray->rayDirInv[1] = 1/direction[1];
	ray->rayDirInv[2] = 1/direction[2];
	collisionHandlerGatherHemisphere.setShooterTriangle(skipTriangleIndex);
	if (!collider->intersect(ray))
	{
		// ray left scene
		if (environment)
		{
			RRVec3 irrad = environment->getElement(direction);
			if (scaler && environment->getScaled()) scaler->getPhysicalScale(irrad);
			return visibility * irrad;
		}
		return Channels(0);
	}
	//LOG_RAY(eye,direction,hitTriangle?ray.hitDistance:0.2f,hitTriangle);
	RR_ASSERT(IS_NUMBER(ray->hitDistance));
	Triangle* hitTriangle = &triangle[ray->hitTriangle];
	const RRMaterial* material = collisionHandlerGatherHemisphere.getContactMaterial(); // could be point detail, unlike hitTriangle->surface 
	RRSideBits side=material->sideBits[ray->hitFrontSide?0:1];
	Channels exitance = Channels(0);
	if (side.legal && (side.catchFrom || side.emitTo))
	{
		// diffuse reflection + emission
		if (side.emitTo)
		{
			// we admit we emit everything to both sides of 2sided face, thus doubling energy
			// this behaviour will be probably changed later
			//float splitToTwoSides = material->sideBits[ray->hitFrontSide?1:0].emitTo ? 0.5f : 1;

			// diffuse reflection
			if (gatherIndirectLight)
			{
				// used in GI final gather
				// point detail version
				exitance += visibility * hitTriangle->getTotalIrradiance() * material->diffuseReflectance.color;// * splitToTwoSides;
				// per triangle version (ignores point detail even if it's already available)
				//exitance += visibility * hitTriangle->totalExitingFlux / hitTriangle->area;
			}
			// diffuse emission
			if (gatherDirectEmitors)
			{
				// used in direct lighting final gather [per pixel emittance]
				exitance += visibility * material->diffuseEmittance.color;// * splitToTwoSides;
			}
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}

		RRVec3 specularReflectPower;
		RRVec3 specularTransmitPower;
		RRReal specularReflectMax;
		RRReal specularTransmitMax;
		bool specularReflect = false;
		bool specularTransmit = false;
		if (side.reflect)
		{
			specularReflectPower = visibility*material->specularReflectance.color;
			specularReflectMax = specularReflectPower.max();
			specularReflect = survivedRussianRoulette(specularReflectMax);
		}
		if (side.transmitFrom)
		{
			specularTransmitPower = visibility*material->specularTransmittance.color;
			specularTransmitMax = specularTransmitPower.max();
			specularTransmit = survivedRussianRoulette(specularTransmitMax);
		}

		if (specularReflect || specularTransmit)
		{
			// calculate hitpoint
			RRVec3 hitPoint3d=eye+direction*ray->hitDistance;

			// parameters of transmission must be computed in advance, recursive reflection destroys ray and material content
			RRVec3 newDirectionTransmit;
			unsigned hitTriangleTransmit = ray->hitTriangle;
			if (specularTransmit)
			{
				// calculate new direction after refraction
				newDirectionTransmit = -refract(ray->hitPlane,direction,material->refractionIndex);
			}

			if (specularReflect)
			{
				// calculate new direction after ideal mirror reflection
				RRVec3 newDirectionReflect = RRVec3(ray->hitPlane)*(-2*dot(direction,RRVec3(ray->hitPlane))/size2(RRVec3(ray->hitPlane)))+direction;
				// recursively call this function
				exitance += gatherPhysicalExitance(hitPoint3d,newDirectionReflect,ray->hitTriangle,specularReflectPower/specularReflectMax);
				//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
			}

			if (specularTransmit)
			{
				// recursively call this function
				exitance += gatherPhysicalExitance(hitPoint3d,newDirectionTransmit,hitTriangleTransmit,specularTransmitPower/specularTransmitMax);
				//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
			}

		}
	}
	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	return exitance;
}

}; // namespace

