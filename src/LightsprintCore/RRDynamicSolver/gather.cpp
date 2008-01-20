
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

//#define RELIABILITY_FILTERING // prepares data for postprocess, that grows reliable texels into unreliable areas
// chyba1: kdyz se podari 1 paprsek z 1000, reliability je tak mala, ze vlastni udaj behem filtrovani
//   zanikne a je prevalcovan irradianci sousednich facu
//   chyba1 vznika jen v GPU filtrovani, bude opraveno prevodem filtrovani vzdy na CPU
// chyba2: leaky na velkou vzdalenost jsou temer vzdy chyba, pritom ale mohou snadno nastat
//   kdyz ma cely cluster velmi malou rel., pritece nahodna barva ze sousedniho clusteru

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

static RRVec3 getRandomExitDir(HomogenousFiller2& filler, const RRVec3& norm, const RRVec3& u3, const RRVec3& v3)
// ortonormal space: norm, u3, v3
// returns random direction exitting diffuse surface with 1 or 2 sides and normal norm
{
#ifdef HOMOGENOUS_FILL
	RRReal x,y;
	RRReal cosa=sqrt(1-filler.GetCirclePoint(&x,&y));
	return norm*cosa + u3*x + v3*y;
#else
	// select random vector from srcPoint3 to one halfspace
	// power is assumed to be 1
	RRReal tmp=(RRReal)rand()/RAND_MAX*1;
	RRReal cosa=sqrt(1-tmp);
	RRReal sina=sqrt( tmp );                  // a = rotation angle from normal to side, sin(a) = distance from center of circle
	RRReal b=rand()*2*3.14159265f/RAND_MAX;         // b = rotation angle around normal
	return norm*cosa + u3*(sina*cos(b)) + v3*(sina*sin(b));
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
		collisionHandler(_pti.context.solver->getMultiObjectCustom(),_pti.tri.triangleIndex,true),
		gatherer(&_pti.rays[0],_pti.context.solver->priv->scene,_tools.environment,_tools.scaler,_pti.context.gatherDirectEmitors,_pti.context.params->applyCurrentSolution)
	{
		// used by processTexel even when not shooting to hemisphere
		irradianceHemisphere = RRVec3(0);
		bentNormalHemisphere = RRVec3(0);
		reliabilityHemisphere = 1;
		rays = (tools.environment || pti.context.params->applyCurrentSolution || pti.context.gatherDirectEmitors) ? MAX(1,pti.context.params->quality) : 0;
	}

	// once before shooting (full init)
	// inputs:
	//  - pti.tri.normal
	void init()
	{
		if(!rays) return;
		// prepare ortonormal base
		n3 = pti.tri.normal.normalized();
		u3 = normalized(ortogonalTo(n3));
		v3 = normalized(ortogonalTo(n3,u3));
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
	//  - ray.rayOrigin
	void shotRay()
	{
			// random exit dir
			RRVec3 dir = getRandomExitDir(fillerDir, n3, u3, v3);
			RRReal dirsize = dir.length();

RRVec3 irrad = gatherer.gather(pti.rays[0].rayOrigin,dir,pti.tri.triangleIndex,RRVec3(1));
//RR_ASSERT(irrad[0]>=0 && irrad[1]>=0 && irrad[2]>=0); may be negative by rounding error
irradianceHemisphere += irrad;
bentNormalHemisphere += dir * (irrad.abs().avg()/dirsize);
hitsScene++;
hitsReliable++;
/*return;

			// intersect scene
			pti.ray->rayDirInv[0] = dirsize/dir[0];
			pti.ray->rayDirInv[1] = dirsize/dir[1];
			pti.ray->rayDirInv[2] = dirsize/dir[2];
			pti.ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE|RRRay::FILL_POINT2D; // 2d is only for point materials
			pti.ray->collisionHandler = &collisionHandler;
			bool hit = tools.collider->intersect(pti.ray);
#ifdef DIAGNOSTIC_RAYS
			bool unreliable = hit && (!pti.ray->hitFrontSide || pti.ray->hitDistance<pti.context.params->rugDistance);
			LOG_RAY(pti.ray->rayOrigin,dir,hit?pti.ray->hitDistance:0.2f,!hit,unreliable);
#endif
			if(!hit)
			{
				// read irradiance on sky
				if(tools.environment)
				{
					//irradianceIndirect += environment->getValue(dir);
					RRVec3 irrad = tools.environment->getValue(dir);
					if(tools.scaler) tools.scaler->getPhysicalScale(irrad);
					maxSingleRayContribution = MAX(maxSingleRayContribution,irrad.sum());
					irradianceHemisphere += irrad;
					bentNormalHemisphere += dir * (irrad.abs().avg()/dirsize);
				}
				hitsSky++;
				hitsReliable++;
			}
			else
			if(!pti.ray->hitFrontSide) //!!! zde nerespektuje surfaceBits
			{
				// ray was lost inside object, 
				// increase our transparency, so our color doesn't leak outside object
				hitsInside++;
				hitsUnreliable++;
			}
			else
			if(pti.ray->hitDistance<pti.context.params->rugDistance)
			{
				// ray hit rug, very close object
				hitsRug++;
				hitsUnreliable++;
			}
			else
			{
				//!!! zde chybi odraz od leskleho, refrakce
				// read texel irradiance as face exitance
				if(pti.context.params->applyCurrentSolution)
				{
					RRVec3 irrad;
					// muzu do RM_EXITANCE_PHYSICAL dat pti.context.params->measure.direct/indirect?
					//	-ve 1st gatheru chci direct(emisivita facu a realtime spotlight), indirect je 0
					//	-ve final gatheru chci direct(emisivita facu a realtime spotlight) i indirect(neco spoctene minulym calculate)
					// ano ale je to neprakticke, neudelam to.
					// protoze je pracne vzdy spravne zapnout dir+indir, casto se v tom udela chyba
					pti.context.solver->priv->scene->getTriangleMeasure(pti.ray->hitTriangle,3,
						RM_EXITANCE_PHYSICAL,
						//RRRadiometricMeasure(1,0,0,pti.context.params->measure.direct,pti.context.params->measure.indirect),
						NULL,irrad);
					maxSingleRayContribution = MAX(maxSingleRayContribution,irrad.sum());
					irradianceHemisphere += irrad;
					bentNormalHemisphere += dir * (irrad.abs().avg()/dirsize);
				}
				hitsScene++;
				hitsReliable++;
			}*/
	}

	// once after shooting
	void done()
	{
		if(!rays) return;
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			irradianceHemisphere = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
			//irradianceHemisphere = RRVec3(1,0,0);
			//reliabilityHemisphere = 1;
		}
		else
		if(hitsInside>(hitsReliable+hitsUnreliable)*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceHemisphere = RRVec3(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		{
			// get average hit, hemisphere hits don't accumulate
			irradianceHemisphere /= (RRReal)hitsReliable;
			// compute reliability
			reliabilityHemisphere = hitsReliable/(RRReal)rays;
		}
		//RR_ASSERT(irradianceHemisphere[0]>=0 && irradianceHemisphere[1]>=0 && irradianceHemisphere[2]>=0); may be negative by rounding error
	}

	RRVec3 irradianceHemisphere;
	RRVec3 bentNormalHemisphere;
	RRReal reliabilityHemisphere;
	unsigned rays;
	unsigned hitsReliable;
	unsigned hitsUnreliable;

protected:
	const GatheringTools& tools;
	const ProcessTexelParams& pti;
	Gatherer gatherer;
	// ortonormal base
	RRVec3 n3; // normal
	RRVec3 u3;
	RRVec3 v3;
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
		: tools(_tools), pti(_pti), collisionHandler(_pti.context.solver->getMultiObjectPhysical(),_pti.context.singleObjectReceiver,NULL,_pti.tri.triangleIndex,true)
		// handler: multiObjectPhysical is sufficient because only sideBits and transparency(physical) are tested
	{
		// filter lights
		const RRLights& allLights = _pti.context.solver->getLights();
		const RRObject* multiObject = _pti.context.solver->getMultiObjectCustom();
		for(unsigned i=0;i<allLights.size();i++)
			if(multiObject->getTriangleMaterial(_pti.tri.triangleIndex,allLights[i],NULL))
				lights.push_back(allLights[i]);
		// more init (depends on filtered lights)
		irradianceLights = RRVec3(0);
		bentNormalLights = RRVec3(0);
		reliabilityLights = 1;
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
	void shotRay(const RRLight* _light)
	{
		if(!_light) return;
		// set dir to light
		RRVec3 dir = (_light->type==RRLight::DIRECTIONAL)?-_light->direction:(_light->position-pti.tri.pos3d);
		RRReal dirsize = dir.length();
		dir /= dirsize;
		if(_light->type==RRLight::DIRECTIONAL) dirsize *= pti.context.params->locality;
		float normalIncidence = dot(dir,pti.tri.normal);
		if(normalIncidence<=0)
		{
			// face is not oriented towards light -> reliable black (selfshadowed)
			hitsScene++;
			hitsReliable++;
		}
		else
		{
			RRRay* ray = &pti.rays[1];
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
				RRVec3 irrad = _light->getIrradiance(pti.tri.pos3d,tools.scaler) * ( normalIncidence * (_light->castShadows?collisionHandler.getVisibility():1) );
				irradianceLights += irrad;
				bentNormalLights += dir * irrad.abs().avg();
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
	void shotRayPerLight()
	{
		for(unsigned i=0;i<lights.size();i++)
			shotRay(lights[i]);
		shotRounds++;
	}

	void done()
	{
		if(!rays) return;
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			irradianceLights = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		if(hitsInside>rays*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceLights = RRVec3(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		{
			// get average result from 1 round (lights accumulate inside 1 round, but multiple rounds must be averaged)
			irradianceLights /= (RRReal)shotRounds;
			// compute reliability (lights have unknown intensities, so result is usually bad in partially reliable scene.
			//  however, scheme works well for most typical 100% and 0% reliable pixels)
			reliabilityLights = hitsReliable/(RRReal)rays;
		}
	}

	unsigned getNumMaterialAcceptedLights()
	{
		return lights.size();
	}

	RRVec3 irradianceLights;
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

// thread safe: yes except for first call (pixelBuffer could allocate memory in renderTexel).
//   someone must call us at least once, before multithreading starts.
// returns:
//   if tc->pixelBuffer is set, physical scale irradiance is rendered and returned
//   if tc->pixelBuffer is not set, physical scale irradiance is returned
// pozor na:
//   - bazi pro strileni do hemisfery si vyrabi z normaly
//     baze (n3/u3/v3) je nespojita funkce normaly (n3), tj. nepatrne vychyleny triangl muze strilet uplne jinym smerem
//      nez jeho kamaradi -> pri nizke quality pak ziska zretelne jinou barvu
//     bazi by slo generovat spojite -> zlepseni kvality pri nizkem quality
// volano z:
//  updateLightmap->enumerateTexels->processTexel
//  updateSolverIndirectIllumination->processTexel
//  updateSolverDirectIllumination->gatherPerTriangle->processTexel
ProcessTexelResult processTexel(const ProcessTexelParams& pti)
{
	if(!pti.context.solver || !pti.context.solver->getMultiObjectCustom() || !pti.context.solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles())
	{
		RRReporter::report(WARN,"processTexel: Empty scene.\n");
		RR_ASSERT(0);
		return ProcessTexelResult();
	}


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

	// prepare map info
	unsigned mapWidth = pti.context.pixelBuffer ? pti.context.pixelBuffer->getWidth() : 0;
	unsigned mapHeight = pti.context.pixelBuffer ? pti.context.pixelBuffer->getHeight() : 0;

	hemisphere.init();
	gilights.init();

	// shoot
	while(1)
	{
		/////////////////////////////////////////////////////////////////
		//
		// get random position in texel

		RRVec2 uvInTriangle;
		if(pti.context.pixelBuffer)
		{
			// get random position in texel & triangle
			unsigned retries = 0;
retry:
			RRVec2 uvInTexel; // 0..1 in texel
			tools.fillerPos.GetSquare01Point(&uvInTexel.x,&uvInTexel.y);
			// convert it to triangle uv
			RRVec2 uvInMap = RRVec2((pti.uv[0]+uvInTexel[0])/mapWidth,(pti.uv[1]+uvInTexel[1])/mapHeight); // 0..1 in map
			uvInTriangle = RRVec2(POINT_LINE_DISTANCE_2D(uvInMap,pti.tri.line2InMap),POINT_LINE_DISTANCE_2D(uvInMap,pti.tri.line1InMap)); // 0..1 in triangle
			// is position in triangle?
			bool isInsideTriangle = uvInTriangle[0]>=0 && uvInTriangle[1]>=0 && uvInTriangle[0]+uvInTriangle[1]<=1;
			// no -> retry
			if(!isInsideTriangle)
			{
				if(retries<1000)
				{
					retries++;
					goto retry;
				}
				else
				{
					pti.rays[0].rayOrigin = pti.rays[1].rayOrigin = pti.tri.pos3d;
					goto shoot_from_center;
					//break; // stop shooting, result is unreliable
				}
			}
		}
		else
		{
			// get random position in triangle
			tools.fillerPos.GetSquare01Point(&uvInTriangle.x,&uvInTriangle.y);
			if(uvInTriangle[0]+uvInTriangle[1]>1)
			{
				uvInTriangle[0] = 1-uvInTriangle[0];
				uvInTriangle[1] = 1-uvInTriangle[1];
			}
		}
		// fill position in 3d
		pti.rays[0].rayOrigin = pti.rays[1].rayOrigin = pti.tri.triangleBody.vertex0 + pti.tri.triangleBody.side1*uvInTriangle[0] + pti.tri.triangleBody.side2*uvInTriangle[1];
shoot_from_center:


		/////////////////////////////////////////////////////////////////
		//
		// shoot into hemisphere

		bool shootHemisphere = hemisphere.rays && (hemisphere.hitsReliable<=hemisphere.rays/10 || hemisphere.hitsReliable+hemisphere.hitsUnreliable<=hemisphere.rays);
		if(shootHemisphere)
		{
			hemisphere.shotRay();
		}
		

		/////////////////////////////////////////////////////////////////
		//
		// shoot into lights

		bool shootLights = gilights.rays && (gilights.hitsReliable<=gilights.rays/10 || gilights.hitsReliable+gilights.hitsUnreliable<=gilights.rays);
		if(shootLights)
		{
			gilights.shotRayPerLight();
		}


		/////////////////////////////////////////////////////////////////
		//
		// break when no shooting is requested or too many failures were detected

		if((!shootHemisphere || hemisphere.hitsUnreliable>hemisphere.rays*100)
			&& (!shootLights || gilights.hitsUnreliable>gilights.rays*100))
			break;
	}

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
	// sum direct and indirect results
	ProcessTexelResult result;
	result.irradiance = gilights.irradianceLights + hemisphere.irradianceHemisphere; // [3] = 0
	if(gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
	{
		result.irradiance[3] = 1; // only completely unreliable results stay at 0, others get 1 here
	}
	result.bentNormal = gilights.bentNormalLights + hemisphere.bentNormalHemisphere; // [3] = 0
	if(gilights.reliabilityLights || hemisphere.reliabilityHemisphere)
	{
		result.bentNormal[3] = 1; // only completely unreliable results stay at 0, others get 1 here
	}
	if(result.bentNormal[0] || result.bentNormal[1] || result.bentNormal[2]) // avoid NaN
	{
		result.bentNormal.RRVec3::normalize();
	}

#ifdef RELIABILITY_FILTERING
	RRReal reliability = MIN(reliabilityLights,reliabilityHemisphere);

#ifdef BLUR
	reliability /= BLUR;
#endif

	// multiply by reliability, it will be corrected in postprocess
	result.irradiance *= reliability; // filtr v pixbufu tento ukon anuluje

	result.bentNormal *= result.irradiance.RRVec3::abs().avg(); // filtr v pixbufu tento ukon anuluje
#endif

	// diagnostic output
	//if(pti.context.params->diagnosticOutput)
	//{
	//	//if(irradiance[3] && irradiance[3]!=1) printf("%d/%d ",hitsInside,hitsSky);
	//	irradiance[0] = hitsInside/(float)rays;
	//	irradiance[1] = (rays-hitsInside-hitsSky)/(float)rays;
	//	irradiance[2] = hitsSky/(float)rays;
	//	irradiance[3] = 1;
	//}
//	irradiance[1]=(irradiance[0]+irradiance[1]+irradiance[2])/3;
//	irradiance[0]=1-reliability;
//	irradiance[2]=0;

	// store irradiance in custom scale
	if(pti.context.pixelBuffer)
		pti.context.pixelBuffer->renderTexel(pti.uv,result.irradiance);

	// store bent normal
	if(pti.context.bentNormalsPerPixel)
	{
		if(pti.context.pixelBuffer &&
			(pti.context.bentNormalsPerPixel->getWidth()!=pti.context.pixelBuffer->getWidth()
			|| pti.context.bentNormalsPerPixel->getHeight()!=pti.context.pixelBuffer->getHeight()))
		{
			LIMITED_TIMES(1,RRReporter::report(ERRO,"processTexel: Lightmap and BentNormalMap sizes must be equal.\n"));
			RR_ASSERT(0);
		}
		pti.context.bentNormalsPerPixel->renderTexel(pti.uv,
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
bool RRDynamicSolver::gatherPerTriangle(const UpdateParameters* aparams, ProcessTexelResult* results, unsigned numResultSlots, bool _gatherDirectEmitors)
{
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
	tc.pixelBuffer = NULL;
	tc.params = &params;
	tc.bentNormalsPerPixel = NULL;
	tc.singleObjectReceiver = NULL; // later modified per triangle
	tc.gatherDirectEmitors = _gatherDirectEmitors;
	RR_ASSERT(numResultSlots==numPostImportTriangles);

	// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
	RRRay* rays = RRRay::create(2*omp_get_max_threads());
#else
	RRRay* rays = RRRay::create(2);
#endif

#pragma omp parallel for schedule(dynamic)
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		tc.singleObjectReceiver = getObject(RRMesh::MultiMeshPreImportNumber(multiMesh->getPreImportTriangle(t)).object);
		ProcessTexelParams pti(tc);
		pti.tri.triangleIndex = (unsigned)t;
		multiMesh->getTriangleBody(pti.tri.triangleIndex,pti.tri.triangleBody);
		RRMesh::TriangleNormals normals;
		multiMesh->getTriangleNormals(t,normals);
		pti.tri.pos3d = pti.tri.triangleBody.vertex0+(pti.tri.triangleBody.side1+pti.tri.triangleBody.side2)*0.333333f;
		pti.tri.normal = (normals.norm[0]+normals.norm[1]+normals.norm[2])*0.333333f;
#ifdef _OPENMP
		pti.rays = rays+2*omp_get_thread_num();
#else
		pti.rays = rays;
#endif
		pti.rays[0].rayLengthMin = priv->minimalSafeDistance;
		pti.rays[1].rayLengthMin = priv->minimalSafeDistance;
		ProcessTexelResult tr = processTexel(pti);
		results[t] = tr;
		//RR_ASSERT(results[t].irradiance[0]>=0 && results[t].irradiance[1]>=0 && results[t].irradiance[2]>=0); small float error may generate negative value
	}

	delete[] rays;
	return true;
}

// CPU version, detects per-triangle direct from RRLights, environment, gathers from current solution
bool RRDynamicSolver::updateSolverDirectIllumination(const UpdateParameters* aparams, bool updateBentNormals)
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
	if(!gatherPerTriangle(aparams,finalGather,numPostImportTriangles,false)) // this is first gather -> don't gather emitors
	{
		delete[] finalGather;
		return false;
	}

	// tmparray -> object
	RRObjectWithIllumination* multiObject = priv->multiObjectPhysicalWithIllumination;
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		multiObject->setTriangleIllumination(t,RM_IRRADIANCE_PHYSICAL,updateBentNormals ? finalGather[t].bentNormal : finalGather[t].irradiance);
	}
	delete[] finalGather;

	// object -> solver.direct
	priv->scene->illuminationReset(false,true);
	priv->solutionVersion++;

	return true;
}

static bool endByTime(void *context)
{
#if PER_SEC==1
	// floating point time without overflows
	return GETTIME>*(TIME*)context;
#else
	// fixed point time with overlaps
	TIME now = GETTIME;
	TIME end = *(TIME*)context;
	TIME max = (TIME)(end+ULONG_MAX/2);
	return ( end<now && now<max ) || ( now<max && max<end ) || ( max<end && end<now );
#endif
}

bool RRDynamicSolver::updateSolverIndirectIllumination(const UpdateParameters* aparamsIndirect, unsigned benchTexels, unsigned benchQuality)
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
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;

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

		// first gather
		TIME t0 = GETTIME;
		updateSolverDirectIllumination(&paramsIndirect,false);
		RRReal secondsInGather = (RRReal)((GETTIME-t0)/(RRReal)PER_SEC);

		// auto quality for propagate
		float secondsInPropagatePlan;
		if(paramsIndirect.applyEnvironment)
		{
			// first gather time is robust measure
			secondsInPropagatePlan = 2*secondsInGather;
		}
		else
		{
			// when not doing paramsIndirect.applyEnvironment, first gather is extra fast
			// benchmark:
			//  shoot 1% of final gather rays
			//  set gather time 20x higher
			TIME benchStart = GETTIME;
			benchTexels /= 100;
			TexelContext tc;
			tc.solver = this;
			tc.pixelBuffer = NULL;
			UpdateParameters params;
			params.quality = benchQuality;
			tc.params = &params;
			tc.bentNormalsPerPixel = NULL;
			tc.singleObjectReceiver = NULL;
			tc.gatherDirectEmitors = priv->staticObjectsContainEmissiveMaterials; // this is simulation of 1% of final gather rays -> gather from emitors
			RRMesh* multiMesh = getMultiObjectCustom()->getCollider()->getMesh();

			// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
			RRRay* rays = RRRay::create(2*omp_get_max_threads());
#else
			RRRay* rays = RRRay::create(2);
#endif

#pragma omp parallel for
			for(int i=0;i<(int)benchTexels;i++)
			{
				unsigned triangleIndex = (unsigned)i%multiMesh->getNumTriangles();
				RRMesh::TriangleNormals normals;
				multiMesh->getTriangleNormals(triangleIndex,normals);
				ProcessTexelParams pti(tc);
				multiMesh->getTriangleBody(triangleIndex,pti.tri.triangleBody);
				pti.tri.triangleIndex = triangleIndex;
				pti.tri.pos3d = pti.tri.triangleBody.vertex0+pti.tri.triangleBody.side1*0.33f+pti.tri.triangleBody.side2*0.33f;
				pti.tri.normal = (normals.norm[0]+normals.norm[1]+normals.norm[2])*0.333333f;
#ifdef _OPENMP
				pti.rays = rays+2*omp_get_thread_num();
#else
				pti.rays = rays;
#endif
				pti.rays[0].rayLengthMin = priv->minimalSafeDistance;
				pti.rays[1].rayLengthMin = priv->minimalSafeDistance;
				processTexel(pti);
			}

			delete[] rays;

			RRReal secondsInBench = (RRReal)((GETTIME-benchStart)/(RRReal)PER_SEC);
			secondsInPropagatePlan = MAX(0.1f,20*secondsInBench);
		}

		// propagate
		RRReportInterval reportProp(INF2,"Propagating (scheduled %sfor %.1fs) ...\n",paramsIndirect.applyEnvironment?"":"by bench ",secondsInPropagatePlan);
		TIME now = GETTIME;
		TIME end = (TIME)(now+secondsInPropagatePlan*PER_SEC);
		RRStaticSolver::Improvement improvement = priv->scene->illuminationImprove(endByTime,(void*)&end);
		switch(improvement)
		{
			case RRStaticSolver::IMPROVED: RRReporter::report(INF3,"Improved.\n");break;
			case RRStaticSolver::NOT_IMPROVED: RRReporter::report(INF2,"Not improved.\n");break;
			case RRStaticSolver::FINISHED: RRReporter::report(WARN,"No light in scene.\n");break;
			case RRStaticSolver::INTERNAL_ERROR: RRReporter::report(ERRO,"Internal error.\n");break;
		}
		// set solver to reautodetect direct illumination (direct illum in solver was just overwritten)
		//  before further realtime rendering
//		reportDirectIlluminationChange(true);
	}
	return true;
}

} // namespace

