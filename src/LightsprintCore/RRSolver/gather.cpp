// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// First and final gathering.
// --------------------------------------------------------------------------


#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/RRSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"
#include "gather.h"
#include "../RRStaticSolver/pathtracer.h" //!!! vola neverejny interface static solveru

#define HOMOGENOUS_FILL // enables homogenous rather than random(noisy) shooting, improves baking quality as long as randomnes is provided via [#15]
//#define BLUR 4 // enables full lightmap blur, higher=stronger
//#define UNWRAP_DIAGNOSTIC // kazdy triangl dostane vlastni nahodnou barvu, tam kde bude videt prechod je spatny unwrap nebo rasterizace

namespace rr
{

RRString getIndirectParamsAsString(const RRSolver::UpdateParameters& params)
{
	return RRString(0,L"%hs%hs%hs%hs%d",
		params.indirect.lightMultiplier?"lights ":"",
		params.indirect.environmentMultiplier?"env ":"",
		params.indirect.materialEmittanceMultiplier?"emi ":"",
		params.useCurrentSolution?"cur ":"",
		params.quality
		);
}

//////////////////////////////////////////////////////////////////////////////
//
// directional lightmaps, compatible with Unreal Engine 3

static const RRVec3 g_lightmapDirections[NUM_LIGHTMAPS] = // indexed by LightmapSemantic
{
	RRVec3(0,0,1),
	RRVec3(0,sqrtf(6.0f)/3,1/sqrtf(3.0f)),
	RRVec3(-1/sqrtf(2.0f),-1/sqrtf(6.0f),1/sqrtf(3.0f)),
	RRVec3(1/sqrtf(2.0f),-1/sqrtf(6.0f),1/sqrtf(3.0f))
};


//////////////////////////////////////////////////////////////////////////////
//
// homogenous filling:
//   generates points that nearly homogenously (low density fluctuations) fill 2d area

class HomogenousFiller2
{
	unsigned num;
public:
	// resets random seed
	// smaller seed is faster
	void Reset(unsigned progress) {num=progress;}

	// triangle: center=0,0 vertices=1.732,-1 -1.732,-1 0,2
	void GetTrianglePoint(RRReal *a,RRReal *b)
	{
		unsigned n=num++;
		static const RRReal dir[4][3]={{0,0,-0.5f},{0,1,0.5f},{0.86602540378444f,-0.5f,0.5f},{-0.86602540378444f,-0.5f,0.5f}};
		RRReal x=0;
		RRReal y=0;
		RRReal dist=1;
		while (n)
		{
			x+=dist*dir[n&3][0];
			y+=dist*dir[n&3][1];
			dist*=dir[n&3][2];
			n>>=2;
		}
		*a=x;
		*b=y;
	}

	// circle: center=0,0 radius=1
	RRReal GetCirclePoint(RRReal *a,RRReal *b)
	{
		RRReal dist;
		do GetTrianglePoint(a,b); while ((dist=*a**a+*b**b)>=1);
		return dist;
	}

	// square: -1,-1 .. 1,1
	void GetSquare11Point(RRReal *a,RRReal *b)
	{
		RRReal siz = 0.732050807568876f; // lezi na hrane trianglu, tj. x=2-2*0.86602540378444*x, x=1/(0.5+0.86602540378444)=0.73205080756887656833031125171467
		RRReal sizInv = 1.366025403784441f;
		do GetTrianglePoint(a,b); while (*a<-siz || *a>siz || *b<-siz || *b>siz);
		*a *= sizInv;
		*b *= sizInv;
	}

	// square: 0,0 .. 1,1
	void GetSquare01Point(RRReal *a,RRReal *b)
	{
		RRReal siz = 0.732050807568876f; // lezi na hrane trianglu, tj. x=2-2*0.86602540378444*x, x=1/(0.5+0.86602540378444)=0.73205080756887656833031125171467
		RRReal sizInv = 1.366025403784441f/2;
		do GetTrianglePoint(a,b); while (*a<-siz || *a>siz || *b<-siz || *b>siz);
		*a = *a*sizInv+0.5f;
		*b = *b*sizInv+0.5f;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// random exiting ray

// returns random direction of light entering diffuse surface (= uses cosine distribution)
// result is normalized only if base is orthonormal
// ignores sidebits, is hardcoded for 1sided surface
static RRVec3 getRandomEnterDirNormalized(HomogenousFiller2& filler, const RRMesh::TangentBasis& basisOrthonormal)
{
#ifdef HOMOGENOUS_FILL
	RRReal x,y;
	RRReal cosa=sqrt(1-filler.GetCirclePoint(&x,&y));
	return basisOrthonormal.normal*cosa + basisOrthonormal.tangent*x + basisOrthonormal.bitangent*y;
#else
	// select random vector from srcPoint3 to one halfspace
	RRReal tmp=rand()*(1.f/RAND_MAX);
	RRReal cosa=sqrt(1-tmp);
	RRReal sina=sqrt( tmp );                  // a = rotation angle from normal to side, sin(a) = distance from center of circle
	RRReal b=rand()*(2*RR_PI/RAND_MAX);       // b = rotation angle around normal
	return basisOrthonormal.normal*cosa + basisOrthonormal.tangent*(sina*cos(b)) + basisOrthonormal.bitangent*(sina*sin(b));
#endif
}


/////////////////////////////////////////////////////////////////////////////
//
// baking: helper objects
// created in processTexel(), for each texel/triangle separately

class GatheringTools
{
public:
	GatheringTools(const ProcessTexelParams& pti)
	{
		collider = pti.context.solver->getMultiObject()->getCollider();
		environment = pti.context.params.indirect.environmentMultiplier ? pti.context.solver->getEnvironment() : NULL;
		fillerPos.Reset(pti.resetFiller);
	}

	const RRCollider* collider;
	const RRBuffer* environment;
	HomogenousFiller2 fillerPos;
};


/////////////////////////////////////////////////////////////////////////////
//
// baking: first or final gather of irradiance from hemisphere
// created in processTexel(), for each texel/triangle separately

class GatheredIrradianceHemisphere
{
public:
	// once before shooting (quick init that computes 'rays')
	// inputs:
	//  - pti.context.params
	//  - pti.ray[0]
	//  - pti.tri.triangleIndex
	GatheredIrradianceHemisphere(const GatheringTools& _tools, const ProcessTexelParams& _pti) :
		tools(_tools),
		pti(_pti),
		pathtracerWorker(
			_pti.context,
			pathTracingParameters,
			_pti.context.staticSceneContainsLods,
			_pti.context.params.quality,
			0) // 0 turns off interpolation in lightmap baking (for all but shadow rays), UINT_MAX would enforce it
	{
		pathTracingParameters.direct = _pti.context.params.indirect; // both direct+indirect multipliers taken from single indirect source
		pathTracingParameters.indirect = _pti.context.params.indirect; // it's ok, because this code runs at least 1 bounce from eye, all is indirect
		pathTracingParameters.brdfTypes = RRMaterial::BRDF_ALL;
		// bake with the fastest and the least noisy options (also the least realistic, corners have to be artificially darkened with aoIntensity/aoSize)
		// keeping UINT_MAX should naturally darken corners, but noise makes it useless
		pathTracingParameters.useFlatNormalsSinceDepth = 0;
		pathTracingParameters.useSolverDirectSinceDepth = 0;
		pathTracingParameters.useSolverIndirectSinceDepth = 0;

		RR_ASSERT(_pti.subTexels && _pti.subTexels->size());
		// used by processTexel even when not shooting to hemisphere
		for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
			irradiancePhysicalHemisphere[i] = RRVec3(0);
		bentNormalHemisphere = RRVec3(0);
		reliabilityHemisphere = 0;
		rays = (tools.environment || pti.context.params.indirect.materialEmittanceMultiplier!=0 || pti.context.params.useCurrentSolution) ? RR_MAX(1,pti.context.params.quality) : 0;
		pathtracerWorker.ray.rayLengthMin = pti.rayLengthMin;
	}

	// once before shooting (full init)
	// inputs:
	//  - pti.tri.normal
	void init()
	{
		if (!rays) return;
		// prepare homogenous filler
		fillerDir.Reset(pti.resetFiller);
		// init counters
		hitsReliable = 0;
		hitsUnreliable = 0;
		hitsDistant = 0;
		hitsInside = 0;
		hitsRug = 0;
		hitsSky = 0;
		hitsScene = 0;
		// init watchdogs
		maxSingleRayContribution = 0; // max sum of all irradiance components in physical scale
		// init ray
		//pti.rays[0].rayLengthMin = 0; // Our caller already set rayLengthMin to nice small value, keep it. Small bias is important for UE3 terrain segments that overlap. 0 would be better only if triangles don't overlap.
		pathtracerWorker.ray.rayLengthMax = pti.context.params.locality;
	}

	// 1 ray
	// inputs:
	//  - pti.rays[0].rayOrigin
	// _basisSkewed is derived from RRMesh basis, not orthogonal, not normalized
	void shotRay(const RRVec3& _rayOrigin, const RRMesh::TangentBasis& _basisSkewedNormalized, const RRMesh::TangentBasis& _basisOrthonormal, unsigned _skipTriangleIndex)
	{
		pathtracerWorker.ray.rayOrigin = _rayOrigin;

		// random exit dir
		// use orthonormal basis (so that probabilities of exit directions are correct)
		// don't use _basisSkewedNormalized, because it's not orthonormal, it's made for compatibility with UE3
		RRVec3 dir = getRandomEnterDirNormalized(fillerDir,_basisOrthonormal);

		// AO [#22]: prepare for approximate measurement of hit distance (if it is not changed = no hit)
		// getIncidentRadiance() won't change it if there is no accepted hit
		pathtracerWorker.ray.hitDistance = 1e10f;

		// gather 1 ray
		RRVec3 irrad = pathtracerWorker.getIncidentRadiance(pathtracerWorker.ray.rayOrigin,dir,pti.context.solver->getMultiObject(),_skipTriangleIndex);
		//RR_ASSERT(irrad[0]>=0 && irrad[1]>=0 && irrad[2]>=0); may be negative by rounding error
		if (!pti.context.gatherAllDirections)
		{
			// single irradiance is computed
			// no need to compute dot(dir,normal), it is already compensated by picking dirs close to normal more often
			irradiancePhysicalHemisphere[LS_LIGHTMAP] += irrad;
			RR_ASSERT(IS_VEC3(irradiancePhysicalHemisphere[LS_LIGHTMAP]));
		}
		else
		{
			// 4 irradiance values are computed for different normal directions
			// dot(dir,normal) must be compensated twice because dirs close to main normal are picked more often
			float normalIncidence1 = dot(dir,_basisSkewedNormalized.normal);
			// dir je spocteny z neorthogonalni baze, nejsou tedy zadne zaruky ze dot vyjde >0
			if (normalIncidence1>0)
			{
				float normalIncidence1Inv = 1/normalIncidence1;
				for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				{
					RRVec3 lightmapDirection = _basisSkewedNormalized.tangent*g_lightmapDirections[i][0] + _basisSkewedNormalized.bitangent*g_lightmapDirections[i][1] + _basisSkewedNormalized.normal*g_lightmapDirections[i][2];
					float normalIncidence2 = dot(dir,lightmapDirection.normalized());
					if (normalIncidence2>0)
					{
						irradiancePhysicalHemisphere[i] += irrad * (normalIncidence2*normalIncidence1Inv);
						RR_ASSERT(IS_VEC3(irradiancePhysicalHemisphere[i]));
					}
				}
			}
		}
		bentNormalHemisphere += dir * irrad.abs().avg();
		hitsScene++;
		hitsReliable++;
		if (pathtracerWorker.ray.hitDistance>pti.context.params.aoSize)
			hitsDistant++;
	}

	// once after shooting
	void done()
	{
		if (!rays) return;
		// compute irradiance and reliability
		if (hitsReliable==0)
		{
			// completely unreliable
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradiancePhysicalHemisphere[i] = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		if (hitsInside>(hitsReliable+hitsUnreliable)*pti.context.params.insideObjectsThreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradiancePhysicalHemisphere[i] = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		{
			// get average hit, hemisphere hits don't accumulate
			float factor = 1/(RRReal)hitsReliable;
			if (pti.context.params.aoIntensity>0 && pti.context.params.aoSize>0)
				factor *= pow(hitsDistant/(float)hitsReliable,pti.context.params.aoIntensity);
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
			{
				irradiancePhysicalHemisphere[i] *= factor;
				RR_ASSERT(IS_VEC3(irradiancePhysicalHemisphere[i]));
			}
			// compute reliability
			reliabilityHemisphere = hitsReliable/(RRReal)rays;
		}
		//RR_ASSERT(irradiancePhysicalHemisphere[0]>=0 && irradiancePhysicalHemisphere[1]>=0 && irradiancePhysicalHemisphere[2]>=0); may be negative by rounding error
	}

	RRVec3 irradiancePhysicalHemisphere[NUM_LIGHTMAPS];
	RRVec3 bentNormalHemisphere;
	RRReal reliabilityHemisphere;
	unsigned rays;
	unsigned hitsReliable;
	unsigned hitsUnreliable;
	unsigned hitsDistant;

protected:
	const GatheringTools& tools;
	const ProcessTexelParams& pti;
	PathtracerWorker pathtracerWorker;
	RRSolver::PathTracingParameters pathTracingParameters; // passed to pathtracerWorker
	// homogenous filler
	HomogenousFiller2 fillerDir;
	// counters
	unsigned hitsInside;
	unsigned hitsRug;
	unsigned hitsSky;
	unsigned hitsScene;
	// watchdogs
	RRReal maxSingleRayContribution; // max sum of all irradiance components in physical scale
};


/////////////////////////////////////////////////////////////////////////////
//
// baking: first or final gather of irradiance from lights
// created in processTexel(), for each texel/triangle separately
//
// handler computes direct visibility to light, taking transparency into account.
// light paths with refraction and reflection are silently skipped
//
// reads direct.lightMultiplier (so when used for first gather, caller has to direct.lightMultiplier=indirect)

class GatheredIrradianceLights
{
public:
	// once before shooting
	// inputs:
	//  - pti.rayLengthMin
	//  - ...
	GatheredIrradianceLights(const GatheringTools& _tools, const ProcessTexelParams& _pti)
		: tools(_tools),
		pti(_pti),
		collisionHandlerGatherLight(
			_pti.context.colorSpace,
			_pti.context.params.quality*2, // when gathering lights (possibly rendering direct shadows), make point details 2* more important
			_pti.context.params.quality/10, // but interpolation less important
			_pti.context.staticSceneContainsLods)
	{
		RR_ASSERT(_pti.subTexels && _pti.subTexels->size());
		// filter lights
		//  lights that don't illuminate subTexel[0] shall not illuminate other subtexels too
		//  (because other subtexels are from the same object and for now, users turn off lighting per-object, not per triangle)
		if (pti.relevantLightsFilled)
		{
			numRelevantLights = _pti.numRelevantLights;
		}
		else
		{
			numRelevantLights = 0;
			const RRLights& allLights = _pti.context.solver->getLights();
			const RRObject* multiObject = _pti.context.solver->getMultiObject();
			for (unsigned i=0;i<allLights.size();i++)
				if (allLights[i]->enabled && _pti.subTexels && multiObject->getTriangleMaterial(_pti.subTexels->begin()->multiObjPostImportTriIndex,allLights[i],NULL))
				{
					pti.relevantLights[numRelevantLights++] = allLights[i];
				}
		}
		// more init (depends on filtered lights)
		for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
			irradiancePhysicalLights[i] = RRVec3(0);
		bentNormalLights = RRVec3(0);
		reliabilityLights = 0;
		rounds = (pti.context.params.direct.lightMultiplier && numRelevantLights) ? pti.context.params.quality/10+1 : 0;
		rays = numRelevantLights*rounds;
		ray.hitObject = pti.context.solver->getMultiObject();
		ray.rayLengthMin = pti.rayLengthMin;
	}

	// before shooting
	void init()
	{
		if (!rays) return;
		hitsReliable = 0;
		hitsUnreliable = 0;
		hitsLight = 0;
		hitsInside = 0;
		hitsRug = 0;
		hitsScene = 0;
		shotRounds = 0;
	}

	// 1 ray
	// inputs:
	// - pti.rays[1].rayOrigin
	// _basisSkewed is derived from RRMesh basis, not orthogonal, not normalized
	void shotRay(const RRLight* _light, const RRMesh::TangentBasis& _basisSkewedNormalized)
	{
		if (!_light) return;
		// set dir to light
		RRVec3 dir = (_light->type==RRLight::DIRECTIONAL)?-_light->direction:(_light->position-ray.rayOrigin);
		RRReal dirsize = dir.length();
		dir /= dirsize;
		if (_light->type==RRLight::DIRECTIONAL) dirsize *= pti.context.params.locality;
		float normalIncidence1 = dot(dir,_basisSkewedNormalized.normal);
		if (normalIncidence1<=0 || !_finite(normalIncidence1))
		{
			// face is not oriented towards light (or wrong normal, extremely rare) -> reliable black (selfshadowed)
			hitsScene++;
			hitsReliable++;
			//if (!_finite(normalIncidence1)) // #IND normals in shortfuse/undead scene
			//	RR_LIMITED_TIMES(10,RRReporter::report(INF1,"lightdir=%f %f %f normal=%f %f %f\n",dir[0],dir[1],dir[2],_basisSkewed.normal[0],_basisSkewed.normal[1],_basisSkewed.normal[2]));
		}
		else
		{
			// intesect scene
			ray.rayDir = dir;
			ray.rayLengthMax = dirsize;
			ray.rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE|RRRay::FILL_POINT2D; // triangle+2d is only for point materials
			//ray.hitObject = already set in ctor
			ray.collisionHandler = &collisionHandlerGatherLight;
			collisionHandlerGatherLight.setLight(_light,pti.context.singleObjectReceiver);
			if (!_light->castShadows || !tools.collider->intersect(&ray))
			{
				// direct visibility found (at least partial), add irradiance from light
				// !_light->castShadows -> direct visibility guaranteed even without raycast
				RRVec3 irrad = _light->getIrradiance(ray.rayOrigin,pti.context.colorSpace) * pti.context.params.direct.lightMultiplier;
				RR_ASSERT(IS_VEC3(irrad)); // getIrradiance() must return finite number
				if (_light->castShadows)
				{
					irrad *= collisionHandlerGatherLight.getVisibility();
					RR_ASSERT(IS_VEC3(collisionHandlerGatherLight.getVisibility()));
					RR_ASSERT(IS_VEC3(irrad));
				}
				if (!pti.context.gatherAllDirections)
				{
					if (pti.context.colorSpace && _light->directLambertScaled)
						pti.context.colorSpace->toLinear(normalIncidence1);
					irradiancePhysicalLights[LS_LIGHTMAP] += irrad * normalIncidence1;
					RR_ASSERT(IS_VEC3(irrad));
					RR_ASSERT(_finite(normalIncidence1));
					RR_ASSERT(IS_VEC3(irradiancePhysicalLights[0]));
//RRReporter::report(INF1,"%d/%d +(%f*%f=%f) avg=%f\n",hitsReliable+1,shotRounds+1,irrad[0],normalIncidence1,irrad[0]*normalIncidence1,irradiancePhysicalLights[LS_LIGHTMAP][0]/(shotRounds+1));
				}
				else
				{
					for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
					{
						RRVec3 lightmapDirection = _basisSkewedNormalized.tangent*g_lightmapDirections[i][0] + _basisSkewedNormalized.bitangent*g_lightmapDirections[i][1] + _basisSkewedNormalized.normal*g_lightmapDirections[i][2];
						float normalIncidence2 = dot( dir, lightmapDirection.normalized() );
						if (normalIncidence2>0)
						{
							// darkens light perpendicular to pixel, to smooth one type of sharp edge
							// helps a bit on sphere, but it's unclear how much it would help in general case
							//normalIncidence2 = sqrt(normalIncidence1*normalIncidence2)*1.36f;

							if (pti.context.colorSpace && _light->directLambertScaled)
								pti.context.colorSpace->toLinear(normalIncidence2);
							irradiancePhysicalLights[i] += irrad * normalIncidence2;
							RR_ASSERT(IS_VEC3(irradiancePhysicalLights[0]));
						}
					}
				}
				bentNormalLights += dir * (irrad.abs().avg()*normalIncidence1);
				hitsLight++;
				hitsReliable++;
			}
			else
			if (!collisionHandlerGatherLight.isLegal())
			{
				// illegal side encountered, ray was lost inside object or other harmful situation -> unreliable
				hitsInside++;
				hitsUnreliable++;
			}
			else
			if (ray.hitDistance<pti.context.params.rugDistance)
			{
				// no visibility, ray hit rug, very close object -> unreliable
				hitsRug++;
				hitsUnreliable++;
			}
			else
			{
				// no visibility, ray hit scene -> reliable black (shadowed)
				hitsScene++;
				hitsReliable++;
			}
		}
	}

	// 1 ray per light
	// _basisSkewed is derived from RRMesh basis, not orthogonal, not normalized
	void shotRayPerLight(const RRVec3& _rayOrigin, const RRMesh::TangentBasis& _basisSkewedNormalized, unsigned _skipTriangleIndex)
	{
		ray.rayOrigin = _rayOrigin;
		collisionHandlerGatherLight.setShooterTriangle(pti.context.solver->getMultiObject(),_skipTriangleIndex);
		for (unsigned i=0;i<numRelevantLights;i++)
			shotRay(pti.relevantLights[i],_basisSkewedNormalized);
		shotRounds++;
	}

	void done()
	{
		if (!rays) return;
		// compute irradiance and reliability
		if (hitsReliable==0)
		{
			// completely unreliable
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradiancePhysicalLights[i] = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		if (hitsInside>rays*pti.context.params.insideObjectsThreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradiancePhysicalLights[i] = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		{
			// get average result from 1 round (lights accumulate inside 1 round, but multiple rounds must be averaged)
			for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradiancePhysicalLights[i] /= (RRReal)shotRounds;
			RR_ASSERT(shotRounds!=0);
			// compute reliability (lights have unknown intensities, so result is usually bad in partially reliable scene.
			//  however, scheme works well for most typical 100% and 0% reliable pixels)
			reliabilityLights = hitsReliable/(RRReal)rays;
		}
		RR_ASSERT(IS_VEC3(irradiancePhysicalLights[0]));
	}

	unsigned getNumMaterialAcceptedLights()
	{
		return numRelevantLights;
	}

	RRRay ray; // aligned, better keep it first in structure
	RRVec3 irradiancePhysicalLights[NUM_LIGHTMAPS];
	RRVec3 bentNormalLights;
	RRReal reliabilityLights;
	unsigned hitsReliable;
	unsigned hitsUnreliable;
	unsigned rounds; // requested number of rounds of shooting to all lights
	unsigned rays; // requested number of rays (rounds*lights)

protected:
	const GatheringTools& tools;
	const ProcessTexelParams& pti;
	unsigned numRelevantLights; // lights are in pti.relevantLights
	unsigned hitsLight;
	unsigned hitsInside;
	unsigned hitsRug;
	unsigned hitsScene;
	unsigned shotRounds;
	// collision handler
	RRCollisionHandlerFinalGathering collisionHandlerGatherLight;
};


/////////////////////////////////////////////////////////////////////////////
//
// baking: first or final gathering of irradiance from hemisphere and lights
// called for each texel/triangle separately

// thread safe: yes
// returns:
//   if tc->pixelBuffer is set, physical scale irradiance is rendered and returned
//   if tc->pixelBuffer is not set, physical scale irradiance is returned
ProcessTexelResult processTexel(const ProcessTexelParams& pti)
{
	if (!pti.context.solver || !pti.context.solver->getMultiObject() || !pti.context.solver->getMultiObject()->getCollider()->getMesh()->getNumTriangles())
	{
		RRReporter::report(WARN,"processTexel: Empty scene.\n");
		RR_ASSERT(0);
		return ProcessTexelResult();
	}
	RR_ASSERT(pti.subTexels->size());


	// init helper objects: collider, environment, fillerPos
	GatheringTools tools(pti);

	// prepare irradiance accumulators, set .rays properly (0 when shooting is disabled for any reason)
	GatheredIrradianceHemisphere hemisphere(tools,pti);
	GatheredIrradianceLights gilights(tools,pti);

	// bail out if no work here
	if (!hemisphere.rays && !gilights.rays)
	{
		// This is actual performance warning, we should detect this condition sooner and skip whole triangle or mesh.
		// It's commented out because
		// - copying this condition to distant place would make code unnecessarily complex
		//   (this simple condition depends on 2 other places that set rays)
		// - performance loss is very small
		//RR_LIMITED_TIMES(1,RRReporter::report(WARN,"processTexel: No lightsources (lights=%d, material.accepted.lights=%d, lightMultiplier=%f, env=%d, environmentMultiplier=%f).\n",pti.context.solver->getLights().size(),gilights.getNumMaterialAcceptedLights(),pti.context.params.lightDirectMultiplier,pti.context.solver->getEnvironment()?1:0,pti.context.params.environmentMultiplier));
		return ProcessTexelResult();
	}

	hemisphere.init();
	gilights.init();

	// cached data reused for all rays from one triangleIndex
	const SubTexel* cache_subTexelPtr = NULL;
	unsigned cache_triangleIndex = UINT_MAX;
	RRMesh::TriangleBody cache_tb;
	RRMesh::TriangleNormals cache_bases;
	RRMesh::TangentBasis cache_basis_skewed_normalized;
	RRMesh::TangentBasis cache_basis_orthonormal;


	// init subtexel selector
	TexelSubTexels::const_iterator subTexelIterator = pti.subTexels->begin();
	RRReal areaAccu = -pti.subTexels->begin()->areaInMapSpace;
	RRReal areaMax = 0;
	for (TexelSubTexels::const_iterator i=pti.subTexels->begin();i!=pti.subTexels->end();++i) areaMax += i->areaInMapSpace;

	// shoot
	extern void (*g_logRay)(const RRRay* ray,bool hit);
	g_logRay = pti.context.params.debugRay;
	while (1)
	{
		/////////////////////////////////////////////////////////////////
		//
		// break when no shooting is requested or too many failures were detected

		bool shootHemisphere = (hemisphere.hitsReliable+hemisphere.hitsUnreliable<hemisphere.rays || hemisphere.hitsReliable*10<hemisphere.rays) && hemisphere.hitsUnreliable<hemisphere.rays*100;
		bool shootLights = (gilights.hitsReliable+gilights.hitsUnreliable<gilights.rays || gilights.hitsReliable*10<gilights.rays) && gilights.hitsUnreliable<gilights.rays*100;
		if (!shootHemisphere && !shootLights)
			break;

		/////////////////////////////////////////////////////////////////
		//
		// shoot 1 series

		// update subtexel selector
		unsigned seriesNumShootersTotal = RR_MAX(shootHemisphere?hemisphere.rays:0,shootLights?gilights.rounds:0);
		RRReal areaStep = areaMax/(seriesNumShootersTotal+0.91f);

		unsigned seriesNumHemisphereShootersShot = 0;
		unsigned seriesNumGilightsShootersShot = 0;
		for (unsigned seriesShooterNum=0;seriesShooterNum<seriesNumShootersTotal;seriesShooterNum++)
		{
			/////////////////////////////////////////////////////////////////
			//
			// get random position in texel

			// select subtexel
			areaAccu += areaStep;
			while (areaAccu>0)
			{
				++subTexelIterator;
				if (subTexelIterator==pti.subTexels->end())
					subTexelIterator = pti.subTexels->begin();
				areaAccu -= subTexelIterator->areaInMapSpace;
			}
			const SubTexel* subTexel = *subTexelIterator;

			// update cached triangle data
			if (subTexel->multiObjPostImportTriIndex!=cache_triangleIndex)
			{
				cache_triangleIndex = subTexel->multiObjPostImportTriIndex;
				const RRMesh* multiMesh = pti.context.solver->getMultiObject()->getCollider()->getMesh();
				multiMesh->getTriangleBody(subTexel->multiObjPostImportTriIndex,cache_tb);
				multiMesh->getTriangleNormals(subTexel->multiObjPostImportTriIndex,cache_bases);
			}
			// update cached subtexel data
			// (simplification: average tangent base is used for all rays from subtexel)
			if (subTexel!=cache_subTexelPtr)
			{
				cache_subTexelPtr = subTexel;
				// tangent bases precomputed for center of texel are used by all rays from subtexel, saves 6% of time in lightmap build
				// 1. What is cache_basis_skewed_normalized?
				//    Basis used for lightmap calculation, must exactly match basis in shader.
				//    UE3 shader uses linear interpolation of vertex bases, i.e. we need cache_basis_skewed.
				//    GB (Gamebryo GI Package 2.1.1) shader uses normalized linear interpolation of vertex bases,
				//     i.e. we need cache_basis_skewed_normalized.
				RRVec2 uvInTriangleSpace = ( subTexel->uvInTriangleSpace[0] + subTexel->uvInTriangleSpace[1] + subTexel->uvInTriangleSpace[2] )*0.333333333f; // uv of center of subtexel
				RRReal wInTriangleSpace = 1-uvInTriangleSpace[0]-uvInTriangleSpace[1];
				cache_basis_skewed_normalized.normal = (cache_bases.vertex[0].normal*wInTriangleSpace + cache_bases.vertex[1].normal*uvInTriangleSpace[0] + cache_bases.vertex[2].normal*uvInTriangleSpace[1]).normalized();
				cache_basis_skewed_normalized.tangent = (cache_bases.vertex[0].tangent*wInTriangleSpace + cache_bases.vertex[1].tangent*uvInTriangleSpace[0] + cache_bases.vertex[2].tangent*uvInTriangleSpace[1]).normalized();
				cache_basis_skewed_normalized.bitangent = (cache_bases.vertex[0].bitangent*wInTriangleSpace + cache_bases.vertex[1].bitangent*uvInTriangleSpace[0] + cache_bases.vertex[2].bitangent*uvInTriangleSpace[1]).normalized();
				//if (!cache_basis_skewed.normal.finite()) // shows #IND normal in undead scene
				//	RR_LIMITED_TIMES(10,RRReporter::report(INF1,"cached_normal=%f %f %f uvInTriSpace=%f %f\n",cache_basis_skewed.normal[0],cache_basis_skewed.normal[1],cache_basis_skewed.normal[2],uvInTriangleSpace[0],uvInTriangleSpace[1]));
				// 2. What is cache_basis_orthonormal?
				//    Helper for homogenous shooting to hemisphere, must be orthonormal.
				cache_basis_orthonormal.normal = cache_basis_skewed_normalized.normal;
				cache_basis_orthonormal.buildBasisFromNormal();
			}

			// random 2d pos in subtexel
			unsigned u=rand();
			unsigned v=rand();
			if (u+v>RAND_MAX)
			{
				u=RAND_MAX-u;
				v=RAND_MAX-v;
			}
			RRVec2 uvInTriangleSpace = subTexel->uvInTriangleSpace[0] + (subTexel->uvInTriangleSpace[1]-subTexel->uvInTriangleSpace[0])*(u/(RRReal)RAND_MAX) + (subTexel->uvInTriangleSpace[2]-subTexel->uvInTriangleSpace[0])*(v/(RRReal)RAND_MAX);

			// shooter 3d pos, norm
			RRVec3 rayOrigin = cache_tb.vertex0 + cache_tb.side1*uvInTriangleSpace[0] + cache_tb.side2*uvInTriangleSpace[1];


			/////////////////////////////////////////////////////////////////
			//
			// shoot into hemisphere

			if (shootHemisphere)
			{
				if (!shootLights // shooting only hemisphere
					|| hemisphere.rays>=gilights.rounds // shooting both, always hemisphere
					|| seriesNumHemisphereShootersShot*(seriesNumShootersTotal-1)<hemisphere.rays*seriesShooterNum) // shooting both, sometimes hemisphere
				{
					seriesNumHemisphereShootersShot++;
					hemisphere.shotRay(rayOrigin,cache_basis_skewed_normalized,cache_basis_orthonormal,subTexel->multiObjPostImportTriIndex);
				}
			}
			

			/////////////////////////////////////////////////////////////////
			//
			// shoot into lights

			if (shootLights)
			{
				if (!shootHemisphere // shooting only into lights
					|| gilights.rounds>=hemisphere.rays // shoting both, always into lights
					|| seriesNumGilightsShootersShot*(seriesNumShootersTotal-1)<gilights.rounds*seriesShooterNum) // shooting both, sometimes into lights
				{
					seriesNumGilightsShootersShot++;
					gilights.shotRayPerLight(rayOrigin,cache_basis_skewed_normalized,subTexel->multiObjPostImportTriIndex);
				}
			}
		}
	}
	g_logRay = NULL;

	hemisphere.done();
	gilights.done();

	// sum and store irradiance
	// physical irradiances are stored to working float buffers here, copyElementsTo will copy and scale them to user's buffer later
	ProcessTexelResult result;
	for (unsigned i=0;i<NUM_LIGHTMAPS;i++)
	{
		result.irradiancePhysical[i] = gilights.irradiancePhysicalLights[i] + hemisphere.irradiancePhysicalHemisphere[i]; // [3] = 0
		RR_ASSERT(IS_VEC4(result.irradiancePhysical[i]));
		if (gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
		{
			result.irradiancePhysical[i][3] = TEXELFLAGS_TO_FLOAT(pti.subTexels->texelFlags); // only completely unreliable results stay at 0, others get 1 here
			RR_ASSERT(IS_VEC4(result.irradiancePhysical[i]));
		}
		// store irradiance (no scaling yet)
		if (pti.context.pixelBuffers[i])
			pti.context.pixelBuffers[i]->setElement(pti.uv[0]+pti.uv[1]*pti.context.pixelBuffers[i]->getWidth(),result.irradiancePhysical[i],NULL);
	}
//RRReporter::report(INF1,"texel=%f+%f=%f\n",gilights.irradiancePhysicalLights[LS_LIGHTMAP][0],hemisphere.irradiancePhysicalHemisphere[LS_LIGHTMAP][0],result.irradiance[LS_LIGHTMAP][0]);

	// sum bent normals
	result.bentNormal = gilights.bentNormalLights + hemisphere.bentNormalHemisphere; // [3] = 0
	if (gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
	{
		result.bentNormal[3] = TEXELFLAGS_TO_FLOAT(pti.subTexels->texelFlags); // only completely unreliable results stay at 0, others get 1 here
		RR_ASSERT(IS_VEC4(result.bentNormal));
	}
	if (result.bentNormal[0] || result.bentNormal[1] || result.bentNormal[2]) // avoid NaN
	{
		result.bentNormal.RRVec3::normalize();
		RR_ASSERT(IS_VEC4(result.bentNormal));
	}
	// store bent normal (no scaling now or later)
	if (pti.context.pixelBuffers[LS_BENT_NORMALS])
	{
		pti.context.pixelBuffers[LS_BENT_NORMALS]->setElement(pti.uv[0]+pti.uv[1]*pti.context.pixelBuffers[LS_BENT_NORMALS]->getWidth(),
			// instead of result.bentNormal
			// pass (x+1)/2 to prevent underflow when saving -1..1 in 8bit 0..1
			(result.bentNormal+RRVec4(1,1,1,0))*RRVec4(0.5f,0.5f,0.5f,1),
			NULL);
	}

	//RR_ASSERT(result.irradiance[0]>=0 && result.irradiance[1]>=0 && result.irradiance[2]>=0); small float error may generate negative value
	RR_ASSERT(IS_VEC3(result.irradiancePhysical[0]));

	return result;
}

/////////////////////////////////////////////////////////////////////////////
//
// baking: first or final gathering into triangles

// reads direct.lightMultiplier, indirect.env+emi
bool RRSolver::gatherPerTrianglePhysical(const UpdateParameters* _params, const GatheredPerTriangleData* resultsPhysical, unsigned numResultSlots)
{
	if (aborting)
		return false;
	if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0,&priv->previousCalculateParameters);
		if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
		{
			RRReporter::report(WARN,"RRSolver::gatherPerTrianglePhysical: Empty scene.\n");
			RR_ASSERT(0);
			return false;
		}
	}
	const RRObject* multiObject = getMultiObject();
	const RRMesh* multiMesh = multiObject->getCollider()->getMesh();
	unsigned numPostImportTriangles = multiMesh->getNumTriangles();

	// init params
	UpdateParameters params;
	if (_params) params = *_params;
	params.quality = RR_MAX(1,params.quality);
	optimizeMultipliers(params,false);

	RRReportInterval report(INF2,"Gathering(%ls) ...\n",getIndirectParamsAsString(params).w_str());
	LightmapperJob lmj(this,params);
	lmj.gatherAllDirections = resultsPhysical->data[LS_DIRECTION1]||resultsPhysical->data[LS_DIRECTION2]||resultsPhysical->data[LS_DIRECTION3];
	lmj.staticSceneContainsLods = priv->staticSceneContainsLods;
	RR_ASSERT(numResultSlots==numPostImportTriangles);

	// preallocates texels
#ifdef _OPENMP
	int numThreads = omp_get_max_threads();
#else
	int numThreads = 1;
#endif
	TexelSubTexels* subTexels = new TexelSubTexels[numThreads];
	SubTexel subTexel;
	subTexel.areaInMapSpace = 1; // absolute value of area is not important because subtexel is only one
	subTexel.uvInTriangleSpace[0] = RRVec2(0,0);
	subTexel.uvInTriangleSpace[1] = RRVec2(1,0);
	subTexel.uvInTriangleSpace[2] = RRVec2(0,1);
	TexelSubTexels::Allocator subTexelAllocator;
	for (int i=0;i<numThreads;i++)
		subTexels[i].push_back(subTexel,subTexelAllocator);

	// preallocate empty relevantLights
	//unsigned numAllLights = getLights().size();
	//const RRLight** emptyRelevantLights = new const RRLight*[numAllLights*numThreads];
	// preallocate filled per-object relevantLights
	unsigned numAllLights = getLights().size();
	unsigned numObjects = getStaticObjects().size();
	std::vector<const RRLight*>* relevantLightsPerObject = new std::vector<const RRLight*>[numObjects];
	for (unsigned objectNumber=0;objectNumber<numObjects;objectNumber++)
	{
		RRMesh::PreImportNumber preImportTriangleNumber;
		preImportTriangleNumber.object = objectNumber;
		preImportTriangleNumber.index = getStaticObjects()[objectNumber]->getCollider()->getMesh()->getPreImportTriangle(0).index; // we assume object's triangle 0 made it into multiobject
		unsigned postImportTriangleNumber = multiMesh->getPostImportTriangle(preImportTriangleNumber);
		for (unsigned lightNumber=0;lightNumber<numAllLights;lightNumber++)
		{
			const RRLight* light = getLights()[lightNumber];
			if (light && light->enabled)
				if (// make all lights relevant in rare case we picked invalid triangle
					// - UINT_MAX is returned if triangle 0 is not in multiobject, this happens when opening kalasatama.dae in MovingSun, koupelna3.3ds+1light in SceneViewer
					// - out of range number is returned if object has 0 triangles, this happens when building lmaps in koupelna3 with inserted light
					postImportTriangleNumber>=numPostImportTriangles

					|| multiObject->getTriangleMaterial(postImportTriangleNumber,light,0))
					relevantLightsPerObject[objectNumber].push_back(light);
		}
	}

#pragma omp parallel for schedule(dynamic)
	for (int t=0;t<(int)numPostImportTriangles;t++)
	{
		if ((t%10000)==0) RRReporter::report(INF3,"step %d/%d\n",t/10000,(numPostImportTriangles+10000-1)/10000);
		if ((params.debugTriangle==UINT_MAX || params.debugTriangle==t) && !aborting) // skip other triangles when debugging one
		{
#ifdef _OPENMP
			int threadNum = omp_get_thread_num();
#else
			int threadNum = 0;
#endif
			unsigned objectNumber = multiMesh->getPreImportTriangle(t).object;
			lmj.singleObjectReceiver = getStaticObjects()[objectNumber];
			ProcessTexelParams ptp(lmj);
			ptp.subTexels = subTexels+threadNum;
			ptp.subTexels->begin()->multiObjPostImportTriIndex = t;
			ptp.rayLengthMin = priv->minimalSafeDistance;

			// pass empty array (is filled by processTexel for each triangle separately)
			//ptp.relevantLights = emptyRelevantLights+numAllLights*threadNum;
			//ptp.numRelevantLights = 0;
			//ptp.relevantLightsFilled = false;
			// pass filled array (is common for all triangles in singleobject)
			ptp.numRelevantLights = (unsigned)relevantLightsPerObject[objectNumber].size();
			ptp.relevantLights = ptp.numRelevantLights ? &(relevantLightsPerObject[objectNumber][0]) : NULL; // dirty conversion, from std::vector to array
			ptp.relevantLightsFilled = true;

			resultsPhysical->store(t,processTexel(ptp));
		}
	}

	//delete[] emptyRelevantLights;
	delete[] relevantLightsPerObject;
	delete[] subTexels;
	return !aborting;
}

/////////////////////////////////////////////////////////////////////////////
//
// baking: updates direct illumination in solver

// reads direct.lightMultiplier, indirect.env+emi
bool RRSolver::updateSolverDirectIllumination(const UpdateParameters* _params)
{
	RRReportInterval report(INF2,"Updating solver direct ...\n");

	if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0,&priv->previousCalculateParameters);
		if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
		{
			RR_ASSERT(0);
			RRReporter::report(WARN,"Empty scene.\n");
			return false;
		}
	}

	// solution+lights+env -gather-> tmparray
	unsigned numPostImportTriangles = getMultiObject()->getCollider()->getMesh()->getNumTriangles();
	GatheredPerTriangleData* finalGather = GatheredPerTriangleData::create(numPostImportTriangles,1,0,0);
	if (!finalGather)
	{
		RRReporter::report(ERRO,"Not enough memory, illumination not updated.\n");
		return false;
	}
	UpdateParameters params = _params ? *_params : UpdateParameters();
#ifdef FIRST_GATHER_EMI
	// for high quality settings with emissive textures
	// 1. first gather lights+env+emi
#else
	// for medium/low quality and/or emittance without textures (we do this since rev1417)
	// 1. first gather lights+env
	params.indirect.materialEmittanceMultiplier = 0;
#endif
	if (!gatherPerTrianglePhysical(&params,finalGather,numPostImportTriangles))
	{
		delete finalGather;
		return false;
	}

	// tmparray -> solver.direct
#ifdef FIRST_GATHER_EMI
	// 2. then tell arch solver to fill solver.direct with gathered lights+env+emi
	priv->scene->illuminationReset(false,true,0,NULL,NULL,finalGather->data[LS_LIGHTMAP]);
#else
	// 2. then tell arch solver to fill solver.direct with gathered lights+env and add emi.color from materials
	//    (arch solver does not yet support sampling from emi textures)
	priv->scene->illuminationReset(false,true,_params?_params->indirect.materialEmittanceMultiplier:1,NULL,NULL,finalGather->data[LS_LIGHTMAP]);
#endif
	// 3. later we will calculate solver.indirect from solver.direct
	// 4. later we will final gather emi from textures and when ray hits dif.surface, we illuminate it with solver.direct+solver.indirect [#41]
	priv->solutionVersion++;
	delete finalGather;

	return true;
}

class EndByQuality : public RRStaticSolver::EndFunc
{
public:
	virtual bool requestsEnd()
	{
		int now = int(staticSolver->illuminationAccuracy());
		static int old = -1; if (now/10!=old/10) {old=now;RRReporter::report(INF3,"%d/%d \n",now,targetQuality);}
		return (now > targetQuality) || *aborting;
	}
	virtual bool requestsRealtimeResponse()
	{
		// used by offline solver, go for lower responsiveness, faster calculation
		return false;
	}
	RRStaticSolver* staticSolver;
	int targetQuality;
	bool* aborting;
};

/////////////////////////////////////////////////////////////////////////////
//
// baking: updates indirect illumination in solver

bool RRSolver::updateSolverIndirectIllumination(const UpdateParameters* _paramsIndirect)
{
	if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		RRReportInterval report(INF2,"Smoothing scene ...\n");
		calculateCore(0,&priv->previousCalculateParameters);
		if (!getMultiObject() || !priv->scene || !getMultiObject()->getCollider()->getMesh()->getNumTriangles())
		{
			if (priv->packedSolver)
				RRReporter::report(WARN,"Calculation not supported by Fireball, call leaveFireball() to enable Architect solver.\n");
			else
				RRReporter::report(WARN,"RRSolver::updateSolverIndirectIllumination: Empty scene (%d %d %d).\n",
					getMultiObject()?1:0,
					priv->scene?1:0,
					getMultiObject()?getMultiObject()->getCollider()->getMesh()->getNumTriangles():0);
			return false;
		}
	}
	// set default params instead of NULL
	UpdateParameters paramsIndirect;
	paramsIndirect.useCurrentSolution = false;
	if (_paramsIndirect)
	{
		paramsIndirect = *_paramsIndirect;
		// disable debugging in first gather
		paramsIndirect.debugObject = UINT_MAX;
		paramsIndirect.debugTexel = UINT_MAX;
		paramsIndirect.debugTriangle = UINT_MAX;
		paramsIndirect.debugRay = NULL;
	}

	RRReportInterval report(INF2,"Updating solver indirect(%ls).\n",getIndirectParamsAsString(paramsIndirect).w_str());

	// fix all dirty flags, so next calculateCore doesn't call detectDirectIllumination etc
	calculateCore(0,&priv->previousCalculateParameters);
	
	// reset illumination in solver
	// we need to do it even if there is no work (no light sources),
	// because user can call updateLightmaps(direct(useCurrentSolution),NULL) and start finalgathering solver
	priv->scene->illuminationReset(true,true,paramsIndirect.indirect.materialEmittanceMultiplier,NULL,NULL,NULL);

	// gather direct for requested indirect and propagate in solver
	if (paramsIndirect.indirect.lightMultiplier || paramsIndirect.indirect.environmentMultiplier || paramsIndirect.indirect.materialEmittanceMultiplier)
	{
		// first gather
		paramsIndirect.direct.lightMultiplier = paramsIndirect.indirect.lightMultiplier; // updateSolverDirectIllumination reads direct.lightMultiplier
		unsigned tmp = paramsIndirect.quality;
		paramsIndirect.quality /= 2; // at 50% quality
		bool updated = updateSolverDirectIllumination(&paramsIndirect);
		paramsIndirect.quality = tmp;
		if (!updated)
		{
			// aborting or not enough memory
			return false;
		}

		// propagate
		if (!aborting && paramsIndirect.qualityFactorRadiosity>0)
		{
			EndByQuality endByQuality;
			endByQuality.staticSolver = priv->scene;
			endByQuality.targetQuality = (int)RR_MAX(5,(paramsIndirect.quality*RR_CLAMPED(paramsIndirect.qualityFactorRadiosity,0,100)));
			endByQuality.aborting = &aborting;
			RRReportInterval reportProp(INF2,"Radiosity(%d)...\n",endByQuality.targetQuality);
			RRStaticSolver::Improvement improvement = priv->scene->illuminationImprove(endByQuality);
			switch(improvement)
			{
				case RRStaticSolver::IMPROVED: RRReporter::report(INF3,"Improved.\n");break;
				case RRStaticSolver::NOT_IMPROVED: RRReporter::report(INF2,"Not improved.\n");break;
				case RRStaticSolver::FINISHED: RRReporter::report(WARN,"Scene is completely dark.\n");break;
				case RRStaticSolver::INTERNAL_ERROR: RRReporter::report(ERRO,"Infinite brightness, possibly caused by wrong inputs. Try checkConsistency().\n");break;
			}

			// optimization: free memory taken by factors (we won't need them anymore), but preserve accumulators (we need them for final gather)
			priv->scene->illuminationReset(true,false,paramsIndirect.indirect.materialEmittanceMultiplier,NULL,NULL,NULL);
		}
	}
	return true;
}

} // namespace

