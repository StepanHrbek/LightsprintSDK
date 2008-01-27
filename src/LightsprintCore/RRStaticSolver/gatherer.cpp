// --------------------------------------------------------------------------
// Final gather support.
// Copyright 2007-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "gatherer.h"
#include "rrcore.h"

namespace rr
{

extern Vec3 refract(Vec3 N,Vec3 I,real r);

// inputs:
//  - ray->rayLengthMin
//  - ray->rayLengthMax
Gatherer::Gatherer(RRRay* _ray, const RRStaticSolver* _staticSolver, const RRBuffer* _environment, const RRScaler* _scaler, bool _gatherDirectEmitors, bool _gatherIndirectLight)
{
	ray = _ray;
	ray->collisionHandler = &skipTriangle;
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

RRVec3 Gatherer::gather(RRVec3 eye, RRVec3 direction, unsigned skipTriangleIndex, RRVec3 visibility)
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
	skipTriangle.skipTriangleIndex = skipTriangleIndex;
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
	Triangle* hitTriangle = &triangle[ray->hitTriangle];
	if(!hitTriangle->surface)
	{
		// error (bsp se generuje z meshe a surfacu(null=zahodit face), bsp hash se generuje jen z meshe. -> po zmene materialu nacte stary bsp a zasahne triangl ktery mel surface ok ale nyni ma NULL)
		// sponza-stezka.dae sem leze i kdyz jsem ho nacet poprve, nevim proc
		//RR_ASSERT(0);
		return Channels(0);
	}
	RR_ASSERT(hitTriangle->u2.y==0);
	RR_ASSERT(IS_NUMBER(ray->hitDistance));

	RRSideBits side=hitTriangle->surface->sideBits[ray->hitFrontSide?0:1];
	Channels exitance = Channels(0);
	if(side.legal && (side.catchFrom || side.emitTo))
	{
		// per-pixel material
		const RRMaterial* material = hitTriangle->surface;
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
				exitance += visibility * material->diffuseEmittance;
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
			Vec3 newDirection=hitTriangle->getN3()*(-2*dot(direction,hitTriangle->getN3())/size2(hitTriangle->getN3()))+direction;
			// recursively call this function
			exitance += gather(hitPoint3d,newDirection,ray->hitTriangle,visibility*material->specularReflectance);
			//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
		}

		// specular transmittance
		if(side.transmitFrom)
		if(sum(abs(visibility*material->specularTransmittance))>0.1)
		{
			// calculate hitpoint
			Point3 hitPoint3d=eye+direction*ray->hitDistance;
			// calculate new direction after refraction
			Vec3 newDirection=-refract(hitTriangle->getN3(),direction,material->refractionIndex);
			// recursively call this function
			exitance += gather(hitPoint3d,newDirection,ray->hitTriangle,visibility*material->specularTransmittance);
			RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0);
		}
	}
	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	return exitance;
}

}; // namespace

