// --------------------------------------------------------------------------
// Pathtracer.
// Copyright (c) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "pathtracer.h"
#include "rrcore.h"
#include "../RRSolver/private.h"

namespace rr
{

extern RRVec3 refract(const RRVec3& I, const RRVec3& N, const RRMaterial* m);

/////////////////////////////////////////////////////////////////////////////
//
// PathtracerJob
//

PathtracerJob::PathtracerJob(const RRSolver* _solver, bool _dynamic)
{
	solver = _solver;
	scaler = solver ? solver->getScaler() : NULL;
	RRReal angleRad0 = 0;
	RRReal angleRad1 = 0;
	RRReal blendFactor = solver ? solver->getEnvironmentBlendFactor() : 0;
	RRBuffer* environment0 = solver ? solver->getEnvironment(0,&angleRad0) : NULL;
	RRBuffer* environment1 = solver ? solver->getEnvironment(1,&angleRad1) : NULL;
	environment = RRBuffer::createEnvironmentBlend(environment0,environment1,angleRad0,angleRad1,blendFactor);
	collider = solver ? ( _dynamic ? solver->getCollider() : solver->getMultiObject()->getCollider() ) : NULL;

#ifdef MATERIAL_BACKGROUND_HACK
	environmentAveragePhysical = RRVec3(0);
	if (environment)
	{
		for (int i=-1;i<=1;i++)
		for (int j=-1;j<=1;j++)
		for (int k=-1;k<=1;k++)
			if (i || j || k)
				environmentAveragePhysical += environment->getElementAtDirection(RRVec3((float)i,(float)j,(float)k),scaler);
		environmentAveragePhysical /= 26;
	}
#endif
}

PathtracerJob::~PathtracerJob()
{
	delete environment;
}


/////////////////////////////////////////////////////////////////////////////
//
// PathtracerWorker
//

PathtracerWorker::PathtracerWorker(const PathtracerJob& _ptj, const RRSolver::PathTracingParameters& _parameters, bool _staticSceneContainsLods, unsigned _qualityForPointMaterials, unsigned _qualityForInterpolation)
	: ptj(_ptj), collisionHandlerGatherHemisphere(_ptj.scaler,_qualityForPointMaterials,_qualityForInterpolation,_staticSceneContainsLods),
	  collisionHandlerGatherLights(_ptj.scaler,_qualityForPointMaterials,_qualityForInterpolation,_staticSceneContainsLods),
	  parameters(_parameters)
{
	// don't use 'parameters' in ctor, GatheredIrradianceHemisphere sends us reference to empty params and fills them later
	collisionHandlerGatherHemisphere.setHemisphere(ptj.solver->priv->scene);
	ray.collisionHandler = &collisionHandlerGatherHemisphere;
	ray.rayFlags = RRRay::FILL_DISTANCE|RRRay::FILL_SIDE|RRRay::FILL_PLANE|RRRay::FILL_POINT2D|RRRay::FILL_POINT3D|RRRay::FILL_TRIANGLE; // 3D is only for shadowrays
	lights = &ptj.solver->getLights();
	multiObject = ptj.solver->getMultiObject();
	packedSolver = ptj.solver->priv->packedSolver;
	triangle = (ptj.solver->priv->scene && ptj.solver->priv->scene->scene && ptj.solver->priv->scene->scene->object) ? ptj.solver->priv->scene->scene->object->triangle : NULL;

#ifdef MATERIAL_BACKGROUND_HACK
	bouncedOffInvisiblePlane = false;
#endif
}

// material, ray.hitObject, ray.hitTriangle, ray.hitPoint2d -> normal
// Lightsprint RRVec3(hitPlane) is normalized, goes from front side. point normal also goes from front side
static RRVec3 getPointNormal(const RRRay& ray, const RRMaterial* material, bool interpolated)
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
		RRVec3 bumpElement = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0],uvInTextureSpace[1],0),NULL,interpolated);
		RRVec3 localNormal;
		if (material->bumpMapTypeHeight)
		{
			float height = bumpElement.x;
			float hx = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0]+1.f/material->bumpMap.texture->getWidth(),uvInTextureSpace[1],0),NULL,interpolated).x;
			float hy = material->bumpMap.texture->getElementAtPosition(RRVec3(uvInTextureSpace[0],uvInTextureSpace[1]+1.f/material->bumpMap.texture->getHeight(),0),NULL,interpolated).x;
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
	const RRMatrix3x4Ex* wm = ray.hitObject->getWorldMatrix();
	// either we work with world-space mesh that was already transformed in RRTransformedMeshFilter
	// or it's localspace and we transform normal here using the same code as in RRTransformedMeshFilter
	return wm ? wm->getTransformedNormal(objectNormal).normalized() : objectNormal;
}

RRVec3 PathtracerWorker::getIncidentRadiance(const RRVec3& eye, const RRVec3& direction, const RRObject* shooterObject, unsigned shooterTriangle, RRVec3 visibility, unsigned numBounces)
{
	RR_ASSERT(IS_VEC3(eye));
	RR_ASSERT(IS_VEC3(direction));
	RR_ASSERT(fabs(size2(direction)-1)<0.001);//ocekava normalizovanej dir

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
	if (!ptj.collider || !ptj.collider->intersect(&ray))
	{
		// AO [#22]: possible hits were refused by handler, restore original hitDistance
		ray.hitDistance = hitDistanceBackup;

		// ray left scene, add environment lighting
		if (ptj.environment)
		{
#ifdef MATERIAL_BACKGROUND_HACK
			if (bouncedOffInvisiblePlane)
				return ptj.environmentAveragePhysical * parameters.environmentMultiplier;
#endif

			RRVec3 irrad = ptj.environment->getElementAtDirection(direction,ptj.scaler);
			RR_ASSERT(IS_VEC3(irrad));
			return irrad * parameters.environmentMultiplier;
		}
		return Channels(0);
	}
	// AO [#22]: scene was hit, remember distance
	hitDistanceBackup = ray.hitDistance;

	//LOG_RAY(eye,direction,hitTriangle?ray.hitDistance:0.2f,hitTriangle);
	RR_ASSERT(IS_NUMBER(ray.hitDistance));
	const RRMaterial* material = collisionHandlerGatherHemisphere.getContactMaterial(); // could be point detail, unlike hitTriangle->surface 
	RRSideBits side=material->sideBits[ray.hitFrontSide?0:1];
	Channels exitance = Channels(0);
	RR_ASSERT(side.renderFrom); // guaranteed by RRCollisionHandlerFinalGathering
	if (side.legal)
	{
		// normals initially go from front side
		RRVec3 faceNormal = ray.hitPlane;
		RRVec3 pixelNormal = faceNormal;
		if (numBounces<parameters.useFlatNormalsSinceDepth)
			pixelNormal = getPointNormal(ray,material,true);

		// normals go from hit side now
		if (!ray.hitFrontSide)
		{
			faceNormal = -faceNormal;
			pixelNormal = -pixelNormal;
		}

		RRMaterial::Response response;
		response.dirNormal = pixelNormal;
		response.dirOut = -direction;

		float lightMultiplier = numBounces ? parameters.currentSolutionMultiplier : parameters.lightMultiplier;

#ifdef MATERIAL_BACKGROUND_HACK
		RRPointMaterial invisiblePlaneMaterial;
		bool oldBouncedOffInvisiblePlane = bouncedOffInvisiblePlane;
		if (!bouncedOffInvisiblePlane && material && material->name=="background" && ptj.environment)
		{
			invisiblePlaneMaterial = *material;
			bouncedOffInvisiblePlane = true;
			RRVec3 environmentAndSunsPhysical = ptj.environmentAveragePhysical * parameters.environmentMultiplier;
			for (unsigned i=0;i<lights->size();i++)
				if ((*lights)[i]->enabled && (*lights)[i]->type==RRLight::DIRECTIONAL)
				{
					response.dirIn = (*lights)[i]->direction;
					invisiblePlaneMaterial.getResponse(response,parameters.brdfTypes);
					environmentAndSunsPhysical += (*lights)[i]->color * response.colorOut * lightMultiplier;
				}
			RRVec3 floor = ptj.environment->getElementAtDirection(direction,ptj.scaler);
			floor /= environmentAndSunsPhysical+RRVec3(1e-10f);
			invisiblePlaneMaterial.diffuseReflectance.colorPhysical *= floor;
			invisiblePlaneMaterial.specularReflectance.colorPhysical *= floor;
			material = &invisiblePlaneMaterial;
		}
#endif

		// add direct lighting
		if (lightMultiplier)
		if (numBounces>=parameters.useSolverDirectSinceDepth
			&& ray.hitObject && !ray.hitObject->isDynamic // only available if we hit static object
			&& (packedSolver || triangle))
		{
			// fast: read direct light from solver
			// (not interpolated, interpolation is slow and does not improve accuracy)
			if (packedSolver)
			{
				// fireball
				exitance += packedSolver->getTriangleIrradianceDirect(ray.hitTriangle) * material->diffuseReflectance.colorPhysical * lightMultiplier;
				RR_ASSERT(IS_VEC3(exitance));
			}
			else
			{
				// architect
				Triangle* hitTriangle = triangle ? &triangle[ray.hitTriangle] : NULL;
				RR_ASSERT(!triangle || (hitTriangle->surface && hitTriangle->area)); // triangles rejected by setGeometry() have surface=area=0, collisionHandler should reject them too
				// zero area would create #INF in getIndirectIrradiance()
				// that's why triangles with zero area are rejected in setGeometry (they get surface=NULL), and later rejected by collisionHandler (based on surface=NULL), they should not get here
				RR_ASSERT(hitTriangle->area);
				exitance += hitTriangle->getDirectIrradiance() * material->diffuseReflectance.colorPhysical * lightMultiplier;
				RR_ASSERT(IS_VEC3(exitance));
			}
		}
		else
		{
			// accurate: shoot shadow rays to lights
			RRRay shadowRay;
			shadowRay.rayOrigin = ray.hitPoint3d;
			shadowRay.rayLengthMin = ray.rayLengthMin;
			shadowRay.collisionHandler = &collisionHandlerGatherLights;
			shadowRay.rayFlags = 0;
			collisionHandlerGatherLights.setShooterTriangle(ray.hitObject,ray.hitTriangle);
			unsigned numLights = lights ? lights->size() : 0;
			for (unsigned i=0;i<numLights;i++)
			{
				RRLight* light = (*lights)[i];
				if (light->enabled)
				{
					if (light->type==RRLight::DIRECTIONAL)
					{
						shadowRay.rayDir = -light->direction;
						shadowRay.rayLengthMax = 1e10f;//!!!
					}
					else
					{
						shadowRay.rayDir = (light->position-shadowRay.rayOrigin).normalized();
						shadowRay.rayLengthMax = (light->position-shadowRay.rayOrigin).length();
					}
					if (pixelNormal.dot(shadowRay.rayDir)>0 && faceNormal.dot(shadowRay.rayDir)>0) // bad normalmap -> some rays are terminated here
					{
						collisionHandlerGatherLights.setLight(light,NULL);
						RRVec3 unobstructedLight = light->getIrradiance(shadowRay.rayOrigin,ptj.scaler);
						response.dirIn = -shadowRay.rayDir;
						material->getResponse(response,parameters.brdfTypes);
						RRVec3 totalContribution = unobstructedLight * response.colorOut;
						shadowRay.hitObject = multiObject; // non-RRMultiCollider does not fill ray.hitObject, we prefill it here, collisionHandler needs it filled
						if (totalContribution!=RRVec3(0) && !ptj.collider->intersect(&shadowRay))
						{
							// dokud neudelam bidir, shadow raye musej prolitat bez refrakce, s pocitanim pruhlednosti, jinak nevzniknou rgb stiny
							exitance += totalContribution * collisionHandlerGatherLights.getVisibility() * lightMultiplier;
							RR_ASSERT(IS_VEC3(exitance));
						}
					}
				}
			}
			ray.rayLengthMax = 1e10f;
		}

		// add material's own emission
		if (side.emitTo)
		{
			// we emit everything to both sides of 2sided face, thus doubling energy
			// this may be changed later
			//float splitToTwoSides = material->sideBits[ray.hitFrontSide?1:0].emitTo ? 0.5f : 1;

			// used in direct lighting final gather [per pixel emittance]
			exitance += material->diffuseEmittance.colorPhysical * parameters.materialEmittanceMultiplier;// * splitToTwoSides;
			RR_ASSERT(IS_VEC3(exitance));
		}

		// probabilities that we reflect using given BRDF
		float probabilityDiff = (parameters.brdfTypes&RRMaterial::BRDF_DIFFUSE) ? RR_MAX(0,material->diffuseReflectance.colorPhysical.avg()) : 0;
		float probabilitySpec = (parameters.brdfTypes&RRMaterial::BRDF_SPECULAR) ? RR_MAX(0,material->specularReflectance.colorPhysical.avg()) : 0;
		float probabilityTran = (parameters.brdfTypes&RRMaterial::BRDF_TRANSMIT) ? RR_MAX(0,material->specularTransmittance.colorPhysical.avg()) : 0;
		float probabilityStop = RR_MAX(0,1-probabilityDiff-probabilitySpec-probabilityTran);

		// add material's diffuse reflection using shortcut (reading irradiance from solver) 
		// if shortcut is requested, use it always, to reduce noise
		if (numBounces>=parameters.useSolverIndirectSinceDepth
			&& probabilityDiff
			&& ray.hitObject && !ray.hitObject->isDynamic) // only available if we hit static object
		{
			if (packedSolver)
			{
				// fireball:
				exitance += packedSolver->getPointIrradianceIndirect(ray.hitTriangle,ray.hitPoint2d) * material->diffuseReflectance.colorPhysical;// * splitToTwoSides;
				RR_ASSERT(IS_VEC3(exitance));
				probabilityDiff = 0; // exclude diffuse from next path
			}
			else
			if (triangle)
			{
				// if we have triangle, it means that we can access indirect irradiance stored in Architect solver (and we hit static object)
				Triangle* hitTriangle = triangle ? &triangle[ray.hitTriangle] : NULL;
				RR_ASSERT(!triangle || (hitTriangle->surface && hitTriangle->area)); // triangles rejected by setGeometry() have surface=area=0, collisionHandler should reject them too
				// zero area would create #INF in getIndirectIrradiance()
				// that's why triangles with zero area are rejected in setGeometry (they get surface=NULL), and later rejected by collisionHandler (based on surface=NULL), they should not get here
				RR_ASSERT(hitTriangle->area);
				exitance += hitTriangle->getPointMeasure(RM_IRRADIANCE_PHYSICAL_INDIRECT,ray.hitPoint2d) * material->diffuseReflectance.colorPhysical;// * splitToTwoSides;
				RR_ASSERT(IS_VEC3(exitance));
				probabilityDiff = 0; // exclude diffuse from next path
			}
		}

		// continue path
		float r = rand()/(float)RAND_MAX;
		if (// terminate by russian roulette?
			r<probabilityDiff+probabilitySpec+probabilityTran
			// terminate by max depth?
			&& numBounces<parameters.stopAtDepth)
		{
			// select BRDF type of reflected ray
			RRMaterial::BrdfType brdfType = (r<probabilityDiff) ? RRMaterial::BRDF_DIFFUSE : ( (r<probabilityDiff+probabilitySpec) ? RRMaterial::BRDF_SPECULAR : RRMaterial::BRDF_TRANSMIT );

			// increase intensity to compensate for not shooting rays of other types
			float intensity = (probabilityDiff+probabilitySpec+probabilityTran+probabilityStop) / ( (r<probabilityDiff) ? probabilityDiff : ( (r<probabilityDiff+probabilitySpec) ? probabilitySpec : probabilityTran ) );

			// select ray
			material->sampleResponse(response,RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX)),brdfType);

			// if it is good
			if (response.pdf>0 // not invalid
				&& (response.brdfType!=RRMaterial::BRDF_TRANSMIT) == (response.dirIn.dot(faceNormal)<0)) // does not go through wall because of bad normals/normalmap (fixes test/bump-propousti-svetlo.rr3)
			{
				// and strong enough
				RRVec3 responseStrength = response.colorOut * (intensity/response.pdf);
				if (responseStrength.avg()>parameters.stopAtVisibility)
				{
					// shoot it
					exitance += getIncidentRadiance(eye+direction*ray.hitDistance,-response.dirIn,ray.hitObject,ray.hitTriangle,visibility*responseStrength,numBounces+1) * responseStrength;
					RR_ASSERT(IS_VEC3(exitance));
				}
			}
		}

#ifdef MATERIAL_BACKGROUND_HACK
		bouncedOffInvisiblePlane = oldBouncedOffInvisiblePlane;
#endif
	}

	// AO [#22]: restore hitDistance from first hit, overwrite any later hits from reflected rays
	ray.hitDistance = hitDistanceBackup;

	//RR_ASSERT(exitance[0]>=0 && exitance[1]>=0 && exitance[2]>=0); may be negative by rounding error
	RR_ASSERT(IS_VEC3(exitance));
	return exitance;
}

}; // namespace

