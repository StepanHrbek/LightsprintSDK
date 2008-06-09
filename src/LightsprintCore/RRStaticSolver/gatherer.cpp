// --------------------------------------------------------------------------
// Final gather support.
// Copyright 2007-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "gatherer.h"
#include "rrcore.h"

namespace rr
{

extern RRVec3 refract(RRVec3 N,RRVec3 I,real r);

// inputs:
//  - ray->rayLengthMin
//  - ray->rayLengthMax
Gatherer::Gatherer(RRRay* _ray, const RRObject* _multiObject, const RRStaticSolver* _staticSolver, const RRBuffer* _environment, const RRScaler* _scaler, bool _gatherDirectEmitors, bool _gatherIndirectLight, bool _staticSceneContainsLods)
	: collisionHandlerGatherHemisphere(_multiObject,_staticSolver,true,_staticSceneContainsLods)
{
	ray = _ray;
	ray->collisionHandler = &collisionHandlerGatherHemisphere;
	ray->rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE;
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
	if(!triangles)
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
	if(!collider->intersect(ray))
	{
		// ray left scene
		if(environment)
		{
			RRVec3 irrad = environment->getElement(direction);
			if(scaler) scaler->getPhysicalScale(irrad);
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
	if(side.legal && (side.catchFrom || side.emitTo))
	{
		// per-pixel material
		RRMaterial pointMaterial;
		if(side.pointDetails)
		{
			material = &pointMaterial;
			object->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
			side = pointMaterial.sideBits[ray->hitFrontSide?0:1];
		}

		// diffuse reflection
		if(side.emitTo)
		{
			if(gatherIndirectLight)
			{
				// used in GI final gather [per triangle]
				exitance += visibility * hitTriangle->totalExitingFlux / hitTriangle->area;
			}
			else
			if(gatherDirectEmitors)
			{
				// used in direct lighting final gather [per pixel emittance]
				exitance += visibility * material->diffuseEmittance.color;
			}
			else
			{
				// used in GI first gather
			}
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
			//!!! /2 kdyz emituje do obou stran
		}

		// specular reflection
		if(side.reflect)
		if(sum(abs(visibility*material->specularReflectance))>0.1)
		{
			// calculate hitpoint
			Point3 hitPoint3d=eye+direction*ray->hitDistance;
			// calculate new direction after ideal mirror reflection
			RRVec3 newDirection=ray->hitPlane*(-2*dot(direction,ray->hitPlane)/size2(ray->hitPlane))+direction;
			// recursively call this function
			exitance += gatherPhysicalExitance(hitPoint3d,newDirection,ray->hitTriangle,visibility*material->specularReflectance);
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}

		// specular transmittance
		if(side.transmitFrom)
		if(sum(abs(visibility*material->specularTransmittance.color))>0.1)
		{
			// calculate hitpoint
			Point3 hitPoint3d=eye+direction*ray->hitDistance;
			// calculate new direction after refraction
			RRVec3 newDirection=-refract(ray->hitPlane,direction,material->refractionIndex);
			// recursively call this function
			exitance += gatherPhysicalExitance(hitPoint3d,newDirection,ray->hitTriangle,visibility*material->specularTransmittance.color);
			RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0);
		}
	}
	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	return exitance;
}

}; // namespace

