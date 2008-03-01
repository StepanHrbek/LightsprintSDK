// --------------------------------------------------------------------------
// Final gathering.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"
#include "private.h"
#include "gather.h"
#include "../RRStaticSolver/gatherer.h" //!!! vola neverejny interface static solveru
#include "../RRObject/RRCollisionHandler.h"

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}
#define HOMOGENOUS_FILL // enables homogenous rather than random(noisy) shooting
//#define BLUR 4 // enables full lightmap blur, higher=stronger
//#define UNWRAP_DIAGNOSTIC // kazdy triangl dostane vlastni nahodnou barvu, tam kde bude videt prechod je spatny unwrap nebo rasterizace

namespace rr
{

#ifdef DIAGNOSTIC_RAYS
unsigned logTexelIndex = 0;
#define LOG_RAY(aeye,adir,adist,ainfinite,aunreliable) { \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[0]=aeye; \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[1]=(aeye)+(adir)*(adist); \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].infinite=ainfinite; \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].unreliable=aunreliable; \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].index=logTexelIndex; \
	++RRStaticSolver::getSceneStatistics()->numLineSegments%=RRStaticSolver::getSceneStatistics()->MAX_LINES; }
#endif


//////////////////////////////////////////////////////////////////////////////
//
// directional lightmaps, compatible with Unreal Engine 3

static RRVec3 g_lightmapDirections[NUM_LIGHTMAPS] =
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
		while(n)
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
		do GetTrianglePoint(a,b); while((dist=*a**a+*b**b)>=1);
		return dist;
	}

	// square: -1,-1 .. 1,1
	void GetSquare11Point(RRReal *a,RRReal *b)
	{
		RRReal siz = 0.732050807568876f; // lezi na hrane trianglu, tj. x=2-2*0.86602540378444*x, x=1/(0.5+0.86602540378444)=0.73205080756887656833031125171467
		RRReal sizInv = 1.366025403784441f;
		do GetTrianglePoint(a,b); while(*a<-siz || *a>siz || *b<-siz || *b>siz);
		*a *= sizInv;
		*b *= sizInv;
	}

	// square: 0,0 .. 1,1
	void GetSquare01Point(RRReal *a,RRReal *b)
	{
		RRReal siz = 0.732050807568876f; // lezi na hrane trianglu, tj. x=2-2*0.86602540378444*x, x=1/(0.5+0.86602540378444)=0.73205080756887656833031125171467
		RRReal sizInv = 1.366025403784441f/2;
		do GetTrianglePoint(a,b); while(*a<-siz || *a>siz || *b<-siz || *b>siz);
		*a = *a*sizInv+0.5f;
		*b = *b*sizInv+0.5f;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// random exiting ray

// returns random exit direction from diffuse surface
// result is normalized only if base is ortonormal
static RRVec3 getRandomExitDir(HomogenousFiller2& filler, const RRMesh::TangentBasis& basis)
{
#ifdef HOMOGENOUS_FILL
	RRReal x,y;
	RRReal cosa=sqrt(1-filler.GetCirclePoint(&x,&y));
	return basis.normal*cosa + basis.tangent*x + basis.bitangent*y;
#else
	// select random vector from srcPoint3 to one halfspace
	RRReal tmp=(RRReal)rand()/RAND_MAX*1;
	RRReal cosa=sqrt(1-tmp);
	RRReal sina=sqrt( tmp );                  // a = rotation angle from normal to side, sin(a) = distance from center of circle
	RRReal b=rand()*2*3.14159265f/RAND_MAX;         // b = rotation angle around normal
	return basis.normal*cosa + basis.tangent*(sina*cos(b)) + basis.bitangent*(sina*sin(b));
#endif
}


/////////////////////////////////////////////////////////////////////////////
//
// for 1 texel: helper objects used while gathering

class GatheringTools
{
public:
	GatheringTools(const ProcessTexelParams& pti)
	{
		scaler = pti.context.solver->getScaler();
		collider = pti.context.solver->getMultiObjectCustom()->getCollider();
		environment = pti.context.params->applyEnvironment ? pti.context.solver->getEnvironment() : NULL;
		fillerPos.Reset(pti.resetFiller);
	}

	const RRScaler* scaler;
	const RRCollider* collider;
	const RRBuffer* environment;
	HomogenousFiller2 fillerPos;
};


/////////////////////////////////////////////////////////////////////////////
//
// for 1 texel: irradiance gathered from hemisphere

class GatheredIrradianceHemisphere
{
public:
	// once before shooting (quick init that computes 'rays')
	// inputs:
	//  - pti.ray[0]
	//  - pti.tri.triangleIndex
	GatheredIrradianceHemisphere(const GatheringTools& _tools, const ProcessTexelParams& _pti) :
		tools(_tools),
		pti(_pti),
		// collisionHandler: multiObjectCustom is sufficient because only sideBits are tested, we don't need phys scale
		collisionHandler(_pti.context.solver->getMultiObjectCustom(),true),
		gatherer(&_pti.rays[0],_pti.context.solver->priv->scene,_tools.environment,_tools.scaler,_pti.context.gatherDirectEmitors,_pti.context.params->applyCurrentSolution)
	{
		RR_ASSERT(_pti.subTexels && _pti.subTexels->size());
		// used by processTexel even when not shooting to hemisphere
		for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
			irradianceHemisphere[i] = RRVec3(0);
		bentNormalHemisphere = RRVec3(0);
		reliabilityHemisphere = 0;
		rays = (tools.environment || pti.context.params->applyCurrentSolution || pti.context.gatherDirectEmitors) ? MAX(1,pti.context.params->quality) : 0;
	}

	// once before shooting (full init)
	// inputs:
	//  - pti.tri.normal
	void init()
	{
		if(!rays) return;
		// prepare homogenous filler
		fillerDir.Reset(pti.resetFiller);
		// init counters
		hitsReliable = 0;
		hitsUnreliable = 0;
		hitsInside = 0;
		hitsRug = 0;
		hitsSky = 0;
		hitsScene = 0;
		// init watchdogs
		maxSingleRayContribution = 0; // max sum of all irradiance components in physical scale
		// init ray
		pti.rays[0].rayLengthMin = 0; // kdybych na to nesahal, bude tam od volajiciho pekne male cislo
		pti.rays[0].rayLengthMax = pti.context.params->locality;
	}

	// 1 ray
	// inputs:
	//  - pti.rays[0].rayOrigin
	void shotRay(const RRMesh::TangentBasis& _basis, unsigned _skipTriangleIndex)
	{
		// random exit dir
		RRVec3 dir = getRandomExitDir(fillerDir,_basis).normalized(); // basis not ortonormal, result normalized manually

		// gather 1 ray
		RRVec3 irrad = gatherer.gather(pti.rays[0].rayOrigin,dir,_skipTriangleIndex,RRVec3(1));
		//RR_ASSERT(irrad[0]>=0 && irrad[1]>=0 && irrad[2]>=0); may be negative by rounding error
		if(!pti.context.gatherAllDirections)
		{
			// single irradiance is computed
			// no need to compute dot(dir,normal), it is already compensated by picking dirs close to normal more often
			irradianceHemisphere[LS_LIGHTMAP] += irrad;
		}
		else
		{
			// 4 irradiance values are computed for different normal directions
			// dot(dir,normal) must be compensated twice because dirs close to main normal are picked more often
			float normalIncidence1 = dot(dir,_basis.normal.normalized());
			// dir je spocteny z neortogonalni baze, nejsou tedy zadne zaruky ze dot vyjde >0
			if(normalIncidence1>0)
			{
				float normalIncidence1Inv = 1/normalIncidence1;
				for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				{
					RRVec3 lightmapDirection = _basis.tangent*g_lightmapDirections[i][0] + _basis.bitangent*g_lightmapDirections[i][1] + _basis.normal*g_lightmapDirections[i][2];
					float normalIncidence2 = dot(dir,lightmapDirection.normalized());
					if(normalIncidence2>0)
						irradianceHemisphere[i] += irrad * (normalIncidence2*normalIncidence1Inv);
					//!!! pocita jako reliable i kdyz irradianceHemisphere vubec nezmenim
				}
			}
		}
		bentNormalHemisphere += dir * irrad.abs().avg();
		hitsScene++;
		hitsReliable++;
	}

	// once after shooting
	void done()
	{
		if(!rays) return;
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceHemisphere[i] = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		if(hitsInside>(hitsReliable+hitsUnreliable)*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceHemisphere[i] = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		{
			// get average hit, hemisphere hits don't accumulate
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceHemisphere[i] /= (RRReal)hitsReliable;
			// compute reliability
			reliabilityHemisphere = hitsReliable/(RRReal)rays;
		}
		//RR_ASSERT(irradianceHemisphere[0]>=0 && irradianceHemisphere[1]>=0 && irradianceHemisphere[2]>=0); may be negative by rounding error
	}

	RRVec3 irradianceHemisphere[NUM_LIGHTMAPS];
	RRVec3 bentNormalHemisphere;
	RRReal reliabilityHemisphere;
	unsigned rays;
	unsigned hitsReliable;
	unsigned hitsUnreliable;

protected:
	const GatheringTools& tools;
	const ProcessTexelParams& pti;
	Gatherer gatherer;
	// homogenous filler
	HomogenousFiller2 fillerDir;
	// counters
	unsigned hitsInside;
	unsigned hitsRug;
	unsigned hitsSky;
	unsigned hitsScene;
	// watchdogs
	RRReal maxSingleRayContribution; // max sum of all irradiance components in physical scale
	// collision handler
	RRCollisionHandlerFirstReceiver collisionHandler;
};


/////////////////////////////////////////////////////////////////////////////
//
// for 1 texel: irradiance gathered from lights
//
// handler computes direct visibility to light, taking transparency into account.
// light paths with refraction and reflection are silently skipped

class GatheredIrradianceLights
{
public:
	// once before shooting
	// inputs:
	//  - pti.ray[1]
	//  - ...
	GatheredIrradianceLights(const GatheringTools& _tools, const ProcessTexelParams& _pti)
		: tools(_tools), pti(_pti), collisionHandler(_pti.context.solver->getMultiObjectPhysical(),_pti.context.singleObjectReceiver,NULL,true)
		// handler: multiObjectPhysical is sufficient because only sideBits and transparency(physical) are tested
	{
		RR_ASSERT(_pti.subTexels && _pti.subTexels->size());
		// filter lights
		//  lights that don't illuminate subTexel[0] shall not illuminate other texels too
		//  (because other texels are from the same object and for now, users turn off lighting per-object, not per triangle)
		const RRLights& allLights = _pti.context.solver->getLights();
		const RRObject* multiObject = _pti.context.solver->getMultiObjectCustom();
		for(unsigned i=0;i<allLights.size();i++)
			if(multiObject->getTriangleMaterial(_pti.subTexels->at(0).multiObjPostImportTriIndex,allLights[i],NULL))
				lights.push_back(allLights[i]);
		// more init (depends on filtered lights)
		for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
			irradianceLights[i] = RRVec3(0);
		bentNormalLights = RRVec3(0);
		reliabilityLights = 0;
		rounds = (pti.context.params->applyLights && lights.size()) ? pti.context.params->quality/10+1 : 0;
		rays = lights.size()*rounds;
	}

	// before shooting
	void init()
	{
		if(!rays) return;
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
	void shotRay(const RRLight* _light, const RRMesh::TangentBasis& _basis)
	{
		if(!_light) return;
		RRRay* ray = &pti.rays[1];
		// set dir to light
		RRVec3 dir = (_light->type==RRLight::DIRECTIONAL)?-_light->direction:(_light->position-ray->rayOrigin);
		RRReal dirsize = dir.length();
		dir /= dirsize;
		if(_light->type==RRLight::DIRECTIONAL) dirsize *= pti.context.params->locality;
		float normalIncidence = dot(dir,_basis.normal.normalized());
		if(normalIncidence<=0)
		{
			// face is not oriented towards light -> reliable black (selfshadowed)
			hitsScene++;
			hitsReliable++;
		}
		else
		{
			// intesect scene
			ray->rayDirInv[0] = 1/dir[0];
			ray->rayDirInv[1] = 1/dir[1];
			ray->rayDirInv[2] = 1/dir[2];
			ray->rayLengthMax = dirsize;
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE|RRRay::FILL_POINT2D; // triangle+2d is only for point materials
			ray->collisionHandler = &collisionHandler;
			collisionHandler.light = _light;
			if(!_light->castShadows || !tools.collider->intersect(ray))
			{
				// direct visibility found (at least partial), add irradiance from light
				// !_light->castShadows -> direct visibility guaranteed even without raycast
				RRVec3 irrad = _light->getIrradiance(ray->rayOrigin,tools.scaler) * (_light->castShadows?collisionHandler.getVisibility():1);
				if(!pti.context.gatherAllDirections)
				{
					irradianceLights[LS_LIGHTMAP] += irrad * normalIncidence;
//RRReporter::report(INF1,"%d/%d +(%f*%f=%f) avg=%f\n",hitsReliable+1,shotRounds+1,irrad[0],normalIncidence,irrad[0]*normalIncidence,irradianceLights[LS_LIGHTMAP][0]/(shotRounds+1));
				}
				else
				{
					for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
					{
						RRVec3 lightmapDirection = _basis.tangent*g_lightmapDirections[i][0] + _basis.bitangent*g_lightmapDirections[i][1] + _basis.normal*g_lightmapDirections[i][2];
						float normalIncidence = dot( dir, lightmapDirection.normalized() );
						if(normalIncidence>0)
							irradianceLights[i] += irrad * normalIncidence;
					}
				}
				bentNormalLights += dir * (irrad.abs().avg()*normalIncidence);
				hitsLight++;
				hitsReliable++;
			}
			else
			if(!collisionHandler.isLegal())
			{
				// illegal side encountered, ray was lost inside object or other harmful situation -> unreliable
				hitsInside++;
				hitsUnreliable++;
			}
			else
			if(ray->hitDistance<pti.context.params->rugDistance)
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
	void shotRayPerLight(const RRMesh::TangentBasis& _basis, unsigned _skipTriangleIndex)
	{
		collisionHandler.skipTriangleIndex = _skipTriangleIndex;
		for(unsigned i=0;i<lights.size();i++)
			shotRay(lights[i],_basis);
		shotRounds++;
	}

	void done()
	{
		if(!rays) return;
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceLights[i] = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		if(hitsInside>rays*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceLights[i] = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		{
			// get average result from 1 round (lights accumulate inside 1 round, but multiple rounds must be averaged)
			for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
				irradianceLights[i] /= (RRReal)shotRounds;
			// compute reliability (lights have unknown intensities, so result is usually bad in partially reliable scene.
			//  however, scheme works well for most typical 100% and 0% reliable pixels)
			reliabilityLights = hitsReliable/(RRReal)rays;
		}
	}

	unsigned getNumMaterialAcceptedLights()
	{
		return lights.size();
	}

	RRVec3 irradianceLights[NUM_LIGHTMAPS];
	RRVec3 bentNormalLights;
	RRReal reliabilityLights;
	unsigned hitsReliable;
	unsigned hitsUnreliable;
	unsigned rounds; // requested number of rounds of shooting to all lights
	unsigned rays; // requested number of rays (rounds*lights)

protected:
	const GatheringTools& tools;
	const ProcessTexelParams& pti;
	unsigned hitsLight;
	unsigned hitsInside;
	unsigned hitsRug;
	unsigned hitsScene;
	unsigned shotRounds;
	RRLights lights;
	// collision handler
	RRCollisionHandlerVisibility collisionHandler;
};


/////////////////////////////////////////////////////////////////////////////
//
// for 1 texel: complete gathering

// thread safe: yes
// returns:
//   if tc->pixelBuffer is set, physical scale irradiance is rendered and returned
//   if tc->pixelBuffer is not set, physical scale irradiance is returned
ProcessTexelResult processTexel(const ProcessTexelParams& pti)
{
	if(!pti.context.solver || !pti.context.solver->getMultiObjectCustom() || !pti.context.solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		RRReporter::report(WARN,"processTexel: Empty scene.\n");
		RR_ASSERT(0);
		return ProcessTexelResult();
	}
	RR_ASSERT(pti.subTexels->size());


	// init helper objects: scaler, collider, environment, fillerPos
	GatheringTools tools(pti);

	// prepare irradiance accumulators, set .rays properly (0 when shooting is disabled for any reason)
	GatheredIrradianceHemisphere hemisphere(tools,pti);
	GatheredIrradianceLights gilights(tools,pti);

	// bail out if no work here
	if(!hemisphere.rays && !gilights.rays)
	{
		LIMITED_TIMES(1,RRReporter::report(WARN,"processTexel: No lightsources (lights=%d, material.accepted.lights=%d, applyLights=%d, env=%d, applyEnv=%d).\n",pti.context.solver->getLights().size(),gilights.getNumMaterialAcceptedLights(),pti.context.params->applyLights,pti.context.solver->getEnvironment()?1:0,pti.context.params->applyEnvironment));
		return ProcessTexelResult();
	}

	hemisphere.init();
	gilights.init();

	// cached data reused for all rays from one triangleIndex
	unsigned cache_subTexelIndex = UINT_MAX;
	unsigned cache_triangleIndex = UINT_MAX;
	RRMesh::TriangleBody cache_tb;
	RRMesh::TriangleNormals cache_bases;
	RRMesh::TangentBasis cache_basis;


	// init subtexel selector
	unsigned subTexelIndex = 0;
	RRReal areaAccu = -pti.subTexels->at(0).areaInMapSpace;
	RRReal areaMax = pti.subTexels->getAreaInMapSpace();
	RRReal areaStep = areaMax/(MAX(hemisphere.rays,gilights.rays)+0.91f);
/*
//!!!! 1. kdyz chci 10 do svetel a 100 do hemisfery, paprsky do svetel jdou jen z krajnich 10% subtexelu
//!!!! 2. kdyz nahrazuju unreliable (pridavam raye), musim vzdy dostrilet cely dalsi kolo abych je nepridal jen z kraje

A) vratit plnou randomizaci
 a) kazdy texel by cachoval jen svoje subtexely
    - cache nutne alokovat predem spolu s rays[2] (kazdy kdo nas vola)
    . cache typicky mala, max velikost zjistim snadno
 b) udelat si pole vsech tbody a tnormals a sdilet ho vsemi texely, 144MB/mil trianglu
    - cache nutne alokovat predem spolu s rays[2] (kazdy kdo nas vola)
    +zrychleni gatheru
	-dal bych pouzival FASTER (vic zabrane pameti,tbody duplikovane v collideru i gatheru)
 c) zapnout v multimeshi accelerate
    +zrychleni gatheru
	.misto FASTER pouzit FAST (jen o neco vic pameti, jen o neco pomalejsi)
	  +COMPACT,FAST jsou ted rychlejsi, muzu si je dovolit
	   FASTER a FASTEST duplikuji pamet, nepouzivat
	*dalsi zrychleni mozne kdyby gather i collider dostali primy pristup k poli v meshi
B) YES projizdet po kolech, pokud jedno nestaci, projet cele dalsi
 -- rozptyleni mene castych glight mezi castejsi hemi (nebo naopak) = slozity kod nachylny k chybam
 - kazde kolo by melo byt trochu jine, jinak vznikne bias u svetel, nektere subtexely pouzije mockrat, jine 0x
 - muze o par % zpomalit, protoze pri 99% reliabilite vystrili zbytecne 2x tolik rayu
*/
	// shoot
	extern void (*g_logRay)(const RRRay* ray,bool hit);
	g_logRay = pti.context.params->debugRay;
	while(1)
	{
		/////////////////////////////////////////////////////////////////
		//
		// break when no shooting is requested or too many failures were detected

		bool shootHemisphere = hemisphere.hitsReliable+hemisphere.hitsUnreliable<hemisphere.rays || hemisphere.hitsReliable<hemisphere.rays/10;
		bool shootLights = gilights.hitsReliable+gilights.hitsUnreliable<gilights.rays || gilights.hitsReliable<gilights.rays/10;
		if((!shootHemisphere || hemisphere.hitsUnreliable>hemisphere.rays*100)
			&& (!shootLights || gilights.hitsUnreliable>gilights.rays*100))
			break;


		/////////////////////////////////////////////////////////////////
		//
		// get random position in texel

		// select subtexel
		areaAccu += areaStep;
		while(areaAccu>0) areaAccu -= pti.subTexels->at(++subTexelIndex%=pti.subTexels->size()).areaInMapSpace;
		SubTexel* subTexel = &pti.subTexels->at(subTexelIndex);

		// update cached triangle data
//static unsigned q=0;q++;static unsigned w=0;if(q>10000){printf("%d ",w);q=0;w=0;}
		if(subTexel->multiObjPostImportTriIndex!=cache_triangleIndex)
		{
//w++;
			cache_triangleIndex = subTexel->multiObjPostImportTriIndex;
			RRMesh* mesh = pti.context.solver->getMultiObjectCustom()->getCollider()->getMesh();
			mesh->getTriangleBody(subTexel->multiObjPostImportTriIndex,cache_tb);
			mesh->getTriangleNormals(subTexel->multiObjPostImportTriIndex,cache_bases);
		}
		// update cached subtexel data
		// (simplification: average tangent base is used for all rays from subtexel)
		if(subTexelIndex!=cache_subTexelIndex)
		{
			cache_subTexelIndex = subTexelIndex;
			// tangent basis is precomputed for center of texel is used by all rays from subtexel, saves 6% of time in lightmap build
			RRVec2 uvInTriangleSpace = ( subTexel->uvInTriangleSpace[0] + subTexel->uvInTriangleSpace[1] + subTexel->uvInTriangleSpace[2] )*0.333333333f; // uv of center of subtexel
			RRReal wInTriangleSpace = 1-uvInTriangleSpace[0]-uvInTriangleSpace[1];
			cache_basis.normal = cache_bases.vertex[0].normal*wInTriangleSpace + cache_bases.vertex[1].normal*uvInTriangleSpace[0] + cache_bases.vertex[2].normal*uvInTriangleSpace[1];
			cache_basis.tangent = cache_bases.vertex[0].tangent*wInTriangleSpace + cache_bases.vertex[1].tangent*uvInTriangleSpace[0] + cache_bases.vertex[2].tangent*uvInTriangleSpace[1];
			cache_basis.bitangent = cache_bases.vertex[0].bitangent*wInTriangleSpace + cache_bases.vertex[1].bitangent*uvInTriangleSpace[0] + cache_bases.vertex[2].bitangent*uvInTriangleSpace[1];
			// tangent basis for point in triangle was computed as linear interpolation of vertex bases
			// -> result is not ortogonal, lengths are not unit
			//    compatibility with Unreal Engine 3 is secured
		}

		// random 2d pos in subtexel
		unsigned u=rand();
		unsigned v=rand();
		if(u+v>RAND_MAX)
		{
			u=RAND_MAX-u;
			v=RAND_MAX-v;
		}
		RRVec2 uvInTriangleSpace = subTexel->uvInTriangleSpace[0] + (subTexel->uvInTriangleSpace[1]-subTexel->uvInTriangleSpace[0])*(u/(RRReal)RAND_MAX) + (subTexel->uvInTriangleSpace[2]-subTexel->uvInTriangleSpace[0])*(v/(RRReal)RAND_MAX);

		// 3d pos, norm
		pti.rays[1].rayOrigin = pti.rays[0].rayOrigin = cache_tb.vertex0 + cache_tb.side1*uvInTriangleSpace[0] + cache_tb.side2*uvInTriangleSpace[1];


		/////////////////////////////////////////////////////////////////
		//
		// shoot into hemisphere

		if(shootHemisphere)
		{
			hemisphere.shotRay(cache_basis,subTexel->multiObjPostImportTriIndex);
		}
		

		/////////////////////////////////////////////////////////////////
		//
		// shoot into lights

		if(shootLights)
		{
			gilights.shotRayPerLight(cache_basis,subTexel->multiObjPostImportTriIndex);
		}
	}
	g_logRay = NULL;

//	const RRReal maxError = 0.05f;
//	if(hitsReliable && maxSingleRayContribution>irradianceHemisphere.sum()*maxError && hitsReliable+hitsUnreliable<rays*10)
	{
		// get error below maxError, but do no more than rays*10 attempts
//		printf(":");
//		goto shoot_new_batch;
	}
//	printf(" ");

	hemisphere.done();
	gilights.done();

	// sum and store irradiance
	ProcessTexelResult result;
	for(unsigned i=0;i<NUM_LIGHTMAPS;i++)
	{
		result.irradiance[i] = gilights.irradianceLights[i] + hemisphere.irradianceHemisphere[i]; // [3] = 0
		if(gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
			result.irradiance[i][3] = 1; // only completely unreliable results stay at 0, others get 1 here
		// store irradiance (no scaling yet)
		if(pti.context.pixelBuffers[i])
			pti.context.pixelBuffers[i]->renderTexel(pti.uv,result.irradiance[i]);
	}
//RRReporter::report(INF1,"texel=%f+%f=%f\n",gilights.irradianceLights[LS_LIGHTMAP][0],hemisphere.irradianceHemisphere[LS_LIGHTMAP][0],result.irradiance[LS_LIGHTMAP][0]);

	// sum bent normals
	result.bentNormal = gilights.bentNormalLights + hemisphere.bentNormalHemisphere; // [3] = 0
	if(gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
		result.bentNormal[3] = 1; // only completely unreliable results stay at 0, others get 1 here
	if(result.bentNormal[0] || result.bentNormal[1] || result.bentNormal[2]) // avoid NaN
	{
		result.bentNormal.RRVec3::normalize();
	}
	// store bent normal (no scaling)
	if(pti.context.pixelBuffers[LS_BENT_NORMALS])
	{
		pti.context.pixelBuffers[LS_BENT_NORMALS]->renderTexel(pti.uv,
			// instead of result.bentNormal
			// pass (x+1)/2 to prevent underflow when saving -1..1 in 8bit 0..1
			(result.bentNormal+RRVec4(1,1,1,0))*RRVec4(0.5f,0.5f,0.5f,1)
			);
	}

	//RR_ASSERT(result.irradiance[0]>=0 && result.irradiance[1]>=0 && result.irradiance[2]>=0); small float error may generate negative value

	return result;
}


// CPU, gathers per-triangle lighting from RRLights, environment, current solution
// may be called as first gather or final gather
bool RRDynamicSolver::gatherPerTriangle(const UpdateParameters* aparams, ProcessTexelResult* results, unsigned numResultSlots, bool _gatherDirectEmitors, bool _gatherAllDirections)
{
	if(aborting)
		return false;
	if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RRReporter::report(WARN,"RRDynamicSolver::gatherPerTriangle: Empty scene.\n");
			RR_ASSERT(0);
			return false;
		}
	}
	RRObjectWithIllumination* multiObject = priv->multiObjectPhysicalWithIllumination;
	RRMesh* multiMesh = multiObject->getCollider()->getMesh();
	unsigned numPostImportTriangles = multiMesh->getNumTriangles();

	// validate params
	UpdateParameters params;
	if(aparams) params = *aparams;
	params.quality = MAX(1,params.quality);
	
	// optimize params
	if(params.applyLights && !getLights().size())
		params.applyLights = false;
	if(params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;

	RRReportInterval report(INF2,"Gathering(%s%s%s%d) ...\n",
		params.applyLights?"lights ":"",params.applyEnvironment?"env ":"",params.applyCurrentSolution?"cur ":"",params.quality);
	TexelContext tc;
	tc.solver = this;
	for(unsigned i=0;i<NUM_BUFFERS;i++) tc.pixelBuffers[i] = NULL;
	tc.params = &params;
	tc.singleObjectReceiver = NULL; // later modified per triangle
	tc.gatherDirectEmitors = _gatherDirectEmitors;
	tc.gatherAllDirections = _gatherAllDirections;
	RR_ASSERT(numResultSlots==numPostImportTriangles);

	// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
	int num_threads = omp_get_max_threads();
#else
	int num_threads = 1;
#endif
	RRRay* rays = RRRay::create(2*num_threads);

	// preallocates texels
	TexelSubTexels* subTexels = new TexelSubTexels[num_threads];
	SubTexel subTexel;
	subTexel.areaInMapSpace = 1; // absolute value of area is not important because subtexel is only one
	subTexel.uvInTriangleSpace[0] = RRVec2(0,0);
	subTexel.uvInTriangleSpace[1] = RRVec2(1,0);
	subTexel.uvInTriangleSpace[2] = RRVec2(0,1);
	for(int i=0;i<num_threads;i++)
		subTexels[i].push_back(subTexel);

#pragma omp parallel for schedule(dynamic)
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		if((t%10000)==0) RRReporter::report(INF3,"step %d/%d\n",t/10000,(numPostImportTriangles+10000-1)/10000);
		if((params.debugTriangle==UINT_MAX || params.debugTriangle==t) && !aborting) // skip other triangles when debugging one
		{
#ifdef _OPENMP
			int thread_num = omp_get_thread_num();
#else
			int thread_num = 0;
#endif
			tc.singleObjectReceiver = getObject(RRMesh::MultiMeshPreImportNumber(multiMesh->getPreImportTriangle(t)).object);
			ProcessTexelParams ptp(tc);
			ptp.subTexels = subTexels+thread_num;
			ptp.subTexels->at(0).multiObjPostImportTriIndex = t;
			ptp.rays = rays+2*thread_num;
			ptp.rays[0].rayLengthMin = priv->minimalSafeDistance;
			ptp.rays[1].rayLengthMin = priv->minimalSafeDistance;
			results[t] = processTexel(ptp);
			//RR_ASSERT(results[t].irradiance[0]>=0 && results[t].irradiance[1]>=0 && results[t].irradiance[2]>=0); small float error may generate negative value
		}
	}

	delete[] subTexels;
	delete[] rays;
	return true;
}

// CPU version, detects per-triangle direct from RRLights, environment, gathers from current solution
bool RRDynamicSolver::updateSolverDirectIllumination(const UpdateParameters* aparams)
{
	RRReportInterval report(INF2,"Updating solver direct ...\n");

	if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RR_ASSERT(0);
			RRReporter::report(WARN,"Empty scene.\n");
			return false;
		}
	}

	// solution+lights+env -gather-> tmparray
	unsigned numPostImportTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
	ProcessTexelResult* finalGather = new ProcessTexelResult[numPostImportTriangles];
	if(!gatherPerTriangle(aparams,finalGather,numPostImportTriangles,false,false)) // this is first gather -> don't gather emitors, don't gather directions
	{
		delete[] finalGather;
		return false;
	}

	// tmparray -> object
	RRObjectWithIllumination* multiObject = priv->multiObjectPhysicalWithIllumination;
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		multiObject->setTriangleIllumination(t,RM_IRRADIANCE_PHYSICAL,finalGather[t].irradiance[LS_LIGHTMAP]);
	}
	delete[] finalGather;

	// object -> solver.direct
	priv->scene->illuminationReset(false,true);
	priv->solutionVersion++;

	return true;
}

struct EBQContext
{
	RRStaticSolver* staticSolver;
	int targetQuality;
	bool* aborting;
};
static bool endByQuality(void *context)
{
	EBQContext* c = (EBQContext*)context;
	int now = int(c->staticSolver->illuminationAccuracy());
	static int old = -1; if(now/10!=old/10) {old=now;RRReporter::report(INF3,"%d/%d \n",now,c->targetQuality);}
	return (now > c->targetQuality) || *c->aborting;
}

bool RRDynamicSolver::updateSolverIndirectIllumination(const UpdateParameters* aparamsIndirect)
{
	if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		// create objects
		RRReportInterval report(INF2,"Smoothing scene ...\n");
		calculateCore(0);
		if(!getMultiObjectCustom() || !priv->scene || !getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
		{
			RRReporter::report(WARN,"RRDynamicSolver::updateSolverIndirectIllumination: Empty scene.\n");
			return false;
		}
	}
	// set default params instead of NULL
	UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyLights = false;
	paramsIndirect.applyEnvironment = false;
	//paramsDirect.applyCurrentSolution = false;
	if(aparamsIndirect)
	{
		paramsIndirect = *aparamsIndirect;
		// disable debugging in first gather
		paramsIndirect.debugObject = UINT_MAX;
		paramsIndirect.debugTexel = UINT_MAX;
		paramsIndirect.debugTriangle = UINT_MAX;
		paramsIndirect.debugRay = NULL;
	}

	RRReportInterval report(INF2,"Updating solver indirect(%s%s%s).\n",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	if(paramsIndirect.applyCurrentSolution)
	{
		RRReporter::report(WARN,"paramsIndirect.applyCurrentSolution ignored, set it in paramsDirect instead.\n");
		paramsIndirect.applyCurrentSolution = 0;
	}
	else
	if(!paramsIndirect.applyLights && !paramsIndirect.applyEnvironment)
	{
		RR_ASSERT(0); // no lightsource enabled, todo: fill solver.direct with zeroes
	}

	// gather direct for requested indirect and propagate in solver
	if(paramsIndirect.applyLights || paramsIndirect.applyEnvironment)
	{
		// fix all dirty flags, so next calculateCore doesn't call detectDirectIllumination etc
		calculateCore(0);
		priv->scene->illuminationReset(true,true); // required by endByQuality()

		// first gather
		updateSolverDirectIllumination(&paramsIndirect);

		// propagate
		if(!aborting)
		{
			EBQContext context;
			context.staticSolver = priv->scene;
			context.targetQuality = MAX(5,2*paramsIndirect.quality);
			context.aborting = &aborting;
			RRReportInterval reportProp(INF2,"Propagating(%d)...\n",context.targetQuality);
			RRStaticSolver::Improvement improvement = priv->scene->illuminationImprove(endByQuality,(void*)&context);
			switch(improvement)
			{
				case RRStaticSolver::IMPROVED: RRReporter::report(INF3,"Improved.\n");break;
				case RRStaticSolver::NOT_IMPROVED: RRReporter::report(INF2,"Not improved.\n");break;
				case RRStaticSolver::FINISHED: RRReporter::report(WARN,"No light in scene.\n");break;
				case RRStaticSolver::INTERNAL_ERROR: RRReporter::report(ERRO,"Internal error.\n");break;
			}
			// set solver to reautodetect direct illumination (direct illum in solver was just overwritten)
			//  before further realtime rendering
			//reportDirectIlluminationChange(true);
		}
	}
	return true;
}

} // namespace

