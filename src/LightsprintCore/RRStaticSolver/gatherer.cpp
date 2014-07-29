// --------------------------------------------------------------------------
// Final gather support.
// Copyright (c) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "gatherer.h"
#include "rrcore.h"

namespace rr
{

extern RRVec3 refract(const RRVec3& I, const RRVec3& N, const RRMaterial* m);

Gatherer::Gatherer(const RRObject* _multiObject, const RRStaticSolver* _staticSolver, const RRBuffer* _environment, const RRScaler* _scaler, RRReal _gatherDirectEmitors, RRReal _gatherIndirectLight, bool _staticSceneContainsLods, unsigned _quality)
	: collisionHandlerGatherHemisphere(_scaler,_quality,_staticSceneContainsLods)
{
	stopAtDepth = 20; // without stopAtDepth limit, Lightmaps sample with refractive sphere runs forever, single ray bounces inside sphere. it hits only pixels with r=1, so it does not fade away. material clamping does not help here, point materials are used for quality>=18. in this case, stopAtDepth 20 is not visibly slower than 2
	stopAtVisibility = 0.001f;
	collisionHandlerGatherHemisphere.setHemisphere(_staticSolver);
	ray.collisionHandler = &collisionHandlerGatherHemisphere;
	ray.rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_PLANE|RRRay::FILL_POINT2D|RRRay::FILL_TRIANGLE;
	environment = _environment;
	scaler = _scaler;
	gatherDirectEmitors = _gatherDirectEmitors?true:false;
	gatherDirectEmitorsMultiplier = _gatherDirectEmitors;
	gatherIndirectLight = _gatherIndirectLight?true:false;
	gatherIndirectLightMultiplier = _gatherIndirectLight;
	multiObject = _multiObject;
	collider = multiObject->getCollider();
	triangle = (_staticSolver && _staticSolver->scene && _staticSolver->scene->object) ? _staticSolver->scene->object->triangle : NULL;
	triangles = triangle ? _staticSolver->scene->object->triangles : 0;

	// final gather in lightmap does this per-pixel, rather than per-thread
	//  so at very low quality, lightmaps are biased smooth rather than unbiased noisy
	//  bias is related to 1/quality
	russianRoulette.reset();
}

// material, ray.hitObject, ray.hitTriangle, ray.hitPoint2d -> normal
// Lightsprint RRVec3(hitPlane) is normalized, goes from front side. point normal also goes from front side
static RRVec3 getPointNormal(const RRRay& ray, const RRMaterial* material)
{
	RR_ASSERT(ray.hitObject);
	RR_ASSERT(material);

	// calculate interpolated objectspace normal
	const RRMesh* mesh = ray.hitObject->getCollider()->getMesh();
	RRMesh::TriangleNormals tn;
	mesh->getTriangleNormals(ray.hitTriangle,tn);
	RRMesh::TangentBasis pointBasis;
	pointBasis.normal = tn.vertex[0].normal + (tn.vertex[1].normal-tn.vertex[0].normal)*ray.hitPoint2d[0] + (tn.vertex[2].normal-tn.vertex[0].normal)*ray.hitPoint2d[1];
	RRVec3 objectNormal = pointBasis.normal.normalized();

	// if material has bumpmap, read it
	RRMesh::TriangleMapping tm;
	if (material->bumpMap.texture && mesh->getTriangleMapping(ray.hitTriangle,tm,material->bumpMap.texcoord))
	{
		// read localspace normal from bumpmap
		RRVec2 uvInTextureSpace = tm.uv[0] + (tm.uv[1]-tm.uv[0])*ray.hitPoint2d[0] + (tm.uv[2]-tm.uv[0])*ray.hitPoint2d[1];
		RRVec3 bumpElement = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0],uvInTextureSpace[1],0));
		RRVec3 localNormal;
		if (material->bumpMapTypeHeight)
		{
			float height = bumpElement.x;
			float hx = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0]+1.f/material->bumpMap.texture->getWidth(),uvInTextureSpace[1],0)).x;
			float hy = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0],uvInTextureSpace[1]+1.f/material->bumpMap.texture->getHeight(),0)).x;
			localNormal = RRVec3(height-hx,height-hy,0.1f);
		}
		else
		{
			localNormal = bumpElement*2-RRVec3(1);
		}
		localNormal.z /= material->bumpMap.color.x;
		localNormal.normalize();

		// convert localspace normal to objectspace normal
		pointBasis.tangent = tn.vertex[0].tangent + (tn.vertex[1].tangent-tn.vertex[0].tangent)*ray.hitPoint2d[0] + (tn.vertex[2].tangent-tn.vertex[0].tangent)*ray.hitPoint2d[1];
		pointBasis.bitangent = tn.vertex[0].bitangent + (tn.vertex[1].bitangent-tn.vertex[0].bitangent)*ray.hitPoint2d[0] + (tn.vertex[2].bitangent-tn.vertex[0].bitangent)*ray.hitPoint2d[1];
		objectNormal = (pointBasis.tangent*localNormal.x+pointBasis.bitangent*localNormal.y+pointBasis.normal*localNormal.z).normalized();
	}

	// convert objectspace normal to worldspace normal
	const RRMatrix3x4* iwm = ray.hitObject->getInverseWorldMatrix();
	return iwm ? iwm->getTransformedNormal(objectNormal).normalized() : objectNormal;
}

RRVec3 Gatherer::gatherPhysicalExitance(const RRVec3& eye, const RRVec3& direction, const RRObject* shooterObject, unsigned shooterTriangle, RRVec3 visibility, unsigned numBounces)
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

	// AO [#22]: GatheredIrradianceHemisphere calculates AO from ray.hitDistance,
	// so it needs us to return it unchanged if initial ray does not hit scene (or if handler refused all intersections)
	// or return initial ray's hit distance otherwise
	// it doesn't want any secondary/reflected ray data
	RRReal hitDistanceBackup = ray.hitDistance;

	ray.rayOrigin = eye;
	ray.rayDir = direction;
	ray.hitObject = multiObject; // non-RRMultiCollider does not fill ray.hitObject, we prefill it here, collisionHandler needs it filled
	RR_ASSERT(ray.collisionHandler==&collisionHandlerGatherHemisphere);
	collisionHandlerGatherHemisphere.setShooterTriangle(shooterObject,shooterTriangle);
	if (!collider || !collider->intersect(&ray))
	{
		// AO [#22]: possible hits were refused by handler, restore original hitDistance
		ray.hitDistance = hitDistanceBackup;

		// ray left scene, add environment lighting
		if (environment)
		{
			RRVec3 irrad = environment->getElementAtDirection(direction);
			if (scaler && environment->getScaled()) scaler->getPhysicalScale(irrad);
			RR_ASSERT(IS_VEC3(irrad));
			return visibility * irrad;
		}
		return Channels(0);
	}
	// AO [#22]: scene was hit, remember distance
	hitDistanceBackup = ray.hitDistance;

	//LOG_RAY(eye,direction,hitTriangle?ray.hitDistance:0.2f,hitTriangle);
	RR_ASSERT(IS_NUMBER(ray.hitDistance));
	Triangle* hitTriangle = &triangle[ray.hitTriangle];
	RR_ASSERT(hitTriangle->surface && hitTriangle->area); // triangles rejected by setGeometry() have surface=area=0, collisionHandler should reject them too
	const RRMaterial* material = collisionHandlerGatherHemisphere.getContactMaterial(); // could be point detail, unlike hitTriangle->surface 
	RRSideBits side=material->sideBits[ray.hitFrontSide?0:1];
	Channels exitance = Channels(0);
	RR_ASSERT(side.renderFrom); // guaranteed by RRCollisionHandlerFinalGathering
	if (side.legal)
	{
		// normals initially go from front side
		RRVec3 faceNormal = ray.hitPlane;
		RRVec3 pixelNormal = faceNormal;

		// normals go from hit side now
		if (!ray.hitFrontSide)
		{
			faceNormal = -faceNormal;
			pixelNormal = -pixelNormal;
		}

		if (side.catchFrom || side.emitTo)
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
				specularReflectPower = visibility*material->specularReflectance.colorPhysical;
				specularReflectMax = specularReflectPower.maxi();
				specularReflect = russianRoulette.survived(specularReflectMax);
				if (specularReflect)
				{
					// calculate new direction after ideal mirror reflection
					specularReflectDir = pixelNormal*(-2*dot(direction,pixelNormal)/size2(pixelNormal))+direction;
				}
			}
			if (side.transmitFrom)
			{
				specularTransmitPower = visibility*material->specularTransmittance.colorPhysical;
				specularTransmitMax = specularTransmitPower.maxi();
				specularTransmit = russianRoulette.survived(specularTransmitMax);
				if (specularTransmit)
				{
					// calculate new direction after refraction
					specularTransmitDir = refract(direction,pixelNormal,material);
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
					exitance += visibility * material->diffuseEmittance.colorPhysical * gatherDirectEmitorsMultiplier;// * splitToTwoSides;
				}

				// diffuse reflection
				if (gatherIndirectLight)
				{
					// used in GI final gather
					{
						// point detail version
						// zero area would create #INF in getTotalIrradiance() 
						// that's why triangles with zero area are rejected in setGeometry (they get surface=NULL), and later rejected by collisionHandler (based on surface=NULL), they should not get here
						RR_ASSERT(hitTriangle->area);
						exitance += visibility * hitTriangle->getTotalIrradiance() * material->diffuseReflectance.colorPhysical * gatherIndirectLightMultiplier;// * splitToTwoSides;
						RR_ASSERT(IS_VEC3(exitance));
						// per triangle version (ignores point detail even if it's already available)
						//exitance += visibility * hitTriangle->totalExitingFlux / hitTriangle->area;
					}
				}
				//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
				RR_ASSERT(IS_VEC3(exitance));
			}

			if (numBounces<stopAtDepth)
			{
				if (specularReflect)
				{
					// recursively call this function
					exitance += gatherPhysicalExitance(hitPoint3d,specularReflectDir,ray.hitObject,rayHitTriangle,specularReflectPower/specularReflectMax,numBounces+1);
					//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
					RR_ASSERT(IS_VEC3(exitance));
				}

				if (specularTransmit)
				{
					// recursively call this function
					exitance += gatherPhysicalExitance(hitPoint3d,specularTransmitDir,ray.hitObject,rayHitTriangle,specularTransmitPower/specularTransmitMax,numBounces+1);
					//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
					RR_ASSERT(IS_VEC3(exitance));
				}
			}
		}
	}

	// AO [#22]: restore hitDistance from first hit, overwrite any later hits from reflected rays
	ray.hitDistance = hitDistanceBackup;

	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	RR_ASSERT(IS_VEC3(exitance));
	return exitance;
}

}; // namespace

