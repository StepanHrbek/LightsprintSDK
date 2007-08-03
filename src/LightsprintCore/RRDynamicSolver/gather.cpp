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


class SkipTriangle : public RRCollisionHandler
{
public:
	SkipTriangle(unsigned askip) : skip(askip) {}
	virtual void init()
	{
		result = false;
	}
	virtual bool collides(const RRRay* ray)
	{
		result = result || (ray->hitTriangle!=skip);
		return ray->hitTriangle!=skip;
	}
	virtual bool done()
	{
		return result;
	}
	unsigned skip;
private:
	bool result;
};


// thread safe: yes except for first call (pixelBuffer could allocate memory in renderTexel).
//   someone must call us at least once, before multithreading starts.
// returns:
//   if tc->pixelBuffer is set, custom scale irradiance is rendered and returned
//   if tc->pixelBuffer is not set, physical scale irradiance is returned
// pozor na:
//   - bazi pro strileni do hemisfery si vyrabi z normaly
//     baze (n3/u3/v3) je nespojita funkce normaly (n3), tj. nepatrne vychyleny triangl muze strilet uplne jinym smerem
//      nez jeho kamaradi -> pri nizke quality pak ziska zretelne jinou barvu
//     bazi by slo generovat spojite -> zlepseni kvality pri nizkem quality
ProcessTexelResult processTexel(const ProcessTexelParams& pti)
{
	if(!pti.context.solver || !pti.context.solver->getMultiObjectCustom())
	{
		RRReporter::report(WARN,"processTexel: No objects in scene\n");
		RR_ASSERT(0);
		return ProcessTexelResult();
	}


	// prepare scaler
	const RRScaler* scaler = pti.context.solver->getScaler();

	// prepare collider
	const RRCollider* collider = pti.context.solver->getMultiObjectCustom()->getCollider();
	SkipTriangle skip(pti.tri.triangleIndex);

	// prepare environment
	const RRIlluminationEnvironmentMap* environment = pti.context.params->applyEnvironment ? pti.context.solver->getEnvironment() : NULL;
	
	// prepare map info
	unsigned mapWidth = pti.context.pixelBuffer ? pti.context.pixelBuffer->getWidth() : 0;
	unsigned mapHeight = pti.context.pixelBuffer ? pti.context.pixelBuffer->getHeight() : 0;

	// check where to shoot
	bool shootToHemisphere = environment || pti.context.params->applyCurrentSolution;
	bool shootToLights = pti.context.params->applyLights && pti.context.solver->getLights().size();
	if(!shootToHemisphere && !shootToLights)
	{
		LIMITED_TIMES(1,RRReporter::report(WARN,"processTexel: Zero workload.\n");RR_ASSERT(0));		
		return ProcessTexelResult();
	}

	// prepare ray
	pti.ray->rayOrigin = pti.tri.pos3d;
	pti.ray->collisionHandler = &skip;

	// shoot into hemisphere
	RRColorRGBF irradianceHemisphere = RRColorRGBF(0);
	RRVec3 bentNormalHemisphere = RRVec3(0);
	RRReal reliabilityHemisphere = 1;
	if(shootToHemisphere)
	{
		// prepare ortonormal base
		RRVec3 n3 = pti.tri.normal.normalized();
		RRVec3 u3 = normalized(ortogonalTo(n3));
		RRVec3 v3 = normalized(ortogonalTo(n3,u3));
		// prepare homogenous filler
		HomogenousFiller2 fillerDir;
		HomogenousFiller2 fillerPos;
		fillerDir.Reset(pti.resetFiller);
		fillerPos.Reset(pti.resetFiller);
		// init counters
		unsigned rays = pti.context.params->quality ? pti.context.params->quality : 1;
		unsigned hitsReliable = 0;
		unsigned hitsUnreliable = 0;
		unsigned hitsInside = 0;
		unsigned hitsRug = 0;
		unsigned hitsSky = 0;
		unsigned hitsScene = 0;
		// init watchdogs
		RRReal maxSingleRayContribution = 0; // max sum of all irradiance components in physical scale
		// shoot batch of 'rays' rays
shoot_new_batch:
		for(unsigned i=0;i<rays;i++)
		{
			/////////////////////////////////////////////////////////////////
			// get random position in texel
			RRVec2 uvInTriangle;
			if(pti.context.pixelBuffer)
			{
				// get random position in texel & triangle
				unsigned retries = 0;
retry:
				RRVec2 uvInTexel; // 0..1 in texel
				fillerPos.GetSquare01Point(&uvInTexel.x,&uvInTexel.y);
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
						pti.ray->rayOrigin = pti.tri.pos3d;
						goto shoot_from_center;
						//break; // stop shooting, result is unreliable
					}
				}
			}
			else
			{
				// get random position in triangle
				fillerPos.GetSquare01Point(&uvInTriangle.x,&uvInTriangle.y);
				if(uvInTriangle[0]+uvInTriangle[1]>1)
				{
					uvInTriangle[0] = 1-uvInTriangle[0];
					uvInTriangle[1] = 1-uvInTriangle[1];
				}
			}
			// fill position in 3d
			pti.ray->rayOrigin = pti.tri.triangleBody.vertex0 + pti.tri.triangleBody.side1*uvInTriangle[0] + pti.tri.triangleBody.side2*uvInTriangle[1];
shoot_from_center:
			/////////////////////////////////////////////////////////////////

			// random exit dir
			RRVec3 dir = getRandomExitDir(fillerDir, n3, u3, v3);
			RRReal dirsize = dir.length();
			// intersect scene
			pti.ray->rayDirInv[0] = dirsize/dir[0];
			pti.ray->rayDirInv[1] = dirsize/dir[1];
			pti.ray->rayDirInv[2] = dirsize/dir[2];
			pti.ray->rayLengthMax = pti.context.params->locality;
			pti.ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
			bool hit = collider->intersect(pti.ray);
#ifdef DIAGNOSTIC_RAYS
			bool unreliable = hit && (!pti.ray->hitFrontSide || pti.ray->hitDistance<pti.context.params->rugDistance);
			LOG_RAY(pti.ray->rayOrigin,dir,hit?pti.ray->hitDistance:0.2f,!hit,unreliable);
#endif
			if(!hit)
			{
				// read irradiance on sky
				if(environment)
				{
					//irradianceIndirect += environment->getValue(dir);
					RRColorRGBF irrad = environment->getValue(dir);
					if(scaler) scaler->getPhysicalScale(irrad);
					maxSingleRayContribution = MAX(maxSingleRayContribution,irrad.sum());
					irradianceHemisphere += irrad;
					bentNormalHemisphere += dir * (irrad.abs().avg()/dirsize);
				}
				hitsSky++;
				hitsReliable++;
			}
			else
			if(!pti.ray->hitFrontSide) //!!! predelat na obecne, respektovat surfaceBits
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
				// read texel irradiance as face exitance
				if(pti.context.params->applyCurrentSolution)
				{
					RRVec3 irrad;
					// muzu do RM_EXITANCE_PHYSICAL dat pti.context.params->measure.direct/indirect?
					//	-ve 1st gatheru chci direct(emisivita facu a realtime spotlight), indirect je 0
					//	-ve final gatheru chci direct(emisivita facu a realtime spotlight) i indirect(neco spoctene minulym calculate)
					// ano ale je to neprakticke, neudelam to.
					// protoze je pracne vzdy spravne zapnout dir+indir, casto se v tom udela chyba
					pti.context.solver->getStaticSolver()->getTriangleMeasure(pti.ray->hitTriangle,3,
						RM_EXITANCE_PHYSICAL,
						//RRRadiometricMeasure(1,0,0,pti.context.params->measure.direct,pti.context.params->measure.indirect),
						NULL,irrad);
					maxSingleRayContribution = MAX(maxSingleRayContribution,irrad.sum());
					irradianceHemisphere += irrad;
					bentNormalHemisphere += dir * (irrad.abs().avg()/dirsize);
				}
				hitsScene++;
				hitsReliable++;
			}		
		}

		// automatically increase num of rays
		if(hitsReliable<=rays/10 && hitsUnreliable<rays*100)
		{
			// gather at least rays/10 reliable rays, but do no more than rays*100 attempts
//			printf(".");
			goto shoot_new_batch;
		}
		const RRReal maxError = 0.05f;
		if(hitsReliable && maxSingleRayContribution>irradianceHemisphere.sum()*maxError && hitsReliable+hitsUnreliable<rays*10)
		{
			// get error below maxError, but do no more than rays*10 attempts
//			printf(":");
//			goto shoot_new_batch;
		}
//		printf(" ");

		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			irradianceHemisphere = RRColorRGBAF(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
			//irradianceHemisphere = RRColorRGBAF(1,0,0,1);
			//reliabilityHemisphere = 1;
		}
		else
		if(hitsInside>(hitsReliable+hitsUnreliable)*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceHemisphere = RRColorRGBAF(0);
			bentNormalHemisphere = RRVec3(0);
			reliabilityHemisphere = 0;
		}
		else
		{
			// convert to full irradiance (as if all rays hit scene)
			irradianceHemisphere /= (RRReal)hitsReliable;
			// compute reliability
			reliabilityHemisphere = hitsReliable/(RRReal)rays;
		}
	}

	// shoot into lights
	RRColorRGBF irradianceLights = RRColorRGBF(0);
	RRVec3 bentNormalLights = RRVec3(0);
	RRReal reliabilityLights = 1;
	if(shootToLights)
	{
		unsigned hitsReliable = 0;
		unsigned hitsUnreliable = 0;
		unsigned hitsLight = 0;
		unsigned hitsInside = 0;
		unsigned hitsRug = 0;
		unsigned hitsScene = 0;
		const RRLights& lights = pti.context.solver->getLights();
		unsigned rays = (unsigned)lights.size();
		for(unsigned i=0;i<rays;i++)
		{
			const RRLight* light = lights[i];
			if(!light) continue;
			// set dir to light
			RRVec3 dir = (light->type==RRLight::DIRECTIONAL)?-light->direction:(light->position-pti.tri.pos3d);
			RRReal dirsize = dir.length();
			dir /= dirsize;
			if(light->type==RRLight::DIRECTIONAL) dirsize *= pti.context.params->locality;
			float normalIncidence = dot(dir,pti.tri.normal);
			if(normalIncidence<=0)
			{
				// face is not oriented towards light -> reliable black (selfshadowed)
				hitsScene++;
				hitsReliable++;
			}
			else
			{
				// intesect scene
				pti.ray->rayDirInv[0] = 1/dir[0];
				pti.ray->rayDirInv[1] = 1/dir[1];
				pti.ray->rayDirInv[2] = 1/dir[2];
				pti.ray->rayLengthMax = dirsize;
				pti.ray->rayFlags = RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
				if(!collider->intersect(pti.ray))
				{
					// add irradiance from light
					RRVec3 irrad = light->getIrradiance(pti.tri.pos3d,scaler) * normalIncidence;
					irradianceLights += irrad;
					bentNormalLights += dir * irrad.abs().avg();
					hitsLight++;
					hitsReliable++;
				}
				else
				if(!pti.ray->hitFrontSide) //!!! predelat na obecne, respoktovat surfaceBits
				{
					// ray was lost inside object -> unreliable
					hitsInside++;
					hitsUnreliable++;
				}
				else
				if(pti.ray->hitDistance<pti.context.params->rugDistance)
				{
					// ray hit rug, very close object -> unreliable
					hitsRug++;
					hitsUnreliable++;
				}
				else
				{
					// ray hit scene -> reliable black (shadowed)
					hitsScene++;
					hitsReliable++;
				}
			}
		}
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			irradianceLights = RRColorRGBAF(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		if(hitsInside>rays*pti.context.params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceLights = RRColorRGBAF(0);
			bentNormalLights = RRVec3(0);
			reliabilityLights = 0;
		}
		else
		{
			// compute reliability
			reliabilityLights = hitsReliable/(RRReal)rays;
		}
	}

	// sum direct and indirect results
	ProcessTexelResult result;
	result.irradiance = irradianceLights + irradianceHemisphere; // [3] = 0
	if(reliabilityLights || reliabilityHemisphere)
	{
		result.irradiance[3] = 1; // only completely unreliable results stay at 0, others get 1 here
	}
	result.bentNormal = bentNormalLights + bentNormalHemisphere; // [3] = 0
	if(reliabilityLights || reliabilityHemisphere)
	{
		result.bentNormal[3] = 1; // only completely unreliable results stay at 0, others get 1 here
	}
	if(result.bentNormal[0] || result.bentNormal[1] || result.bentNormal[2]) // avoid NaN
	{
		result.bentNormal.RRVec3::normalize();
	}

	// scale irradiance (full irradiance, not fraction) to custom scale
	if(pti.context.pixelBuffer && pti.context.params->measure.scaled && scaler)
		scaler->getCustomScale(result.irradiance);

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

	return result;
}


// CPU, gathers per-triangle lighting from RRLights, RREnvironment, current solution
bool RRDynamicSolver::gatherPerTriangle(const UpdateParameters* aparams, ProcessTexelResult* results, unsigned numResultSlots)
{
	if(!getMultiObjectCustom() || !getStaticSolver())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !getStaticSolver())
		{
			RRReporter::report(WARN,"RRDynamicSolver::gatherPerTriangle: No objects in scene.\n");
			RR_ASSERT(0);
			return false;
		}
	}
	RRObjectWithIllumination* multiObject = getMultiObjectPhysicalWithIllumination();
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

	RRReportInterval report(INF2,"Gathering(%s%s%s%s%s%d) ...\n",
		params.applyLights?"lights ":"",params.applyEnvironment?"env ":"",(params.applyCurrentSolution&&params.measure.direct)?"D":"",(params.applyCurrentSolution&&params.measure.indirect)?"I":"",params.applyCurrentSolution?"cur ":"",params.quality);
	TexelContext tc;
	tc.solver = this;
	tc.pixelBuffer = NULL;
	tc.params = &params;
	tc.bentNormalsPerPixel = NULL;
	RR_ASSERT(numResultSlots==numPostImportTriangles);

	// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
	RRRay* rays = RRRay::create(omp_get_max_threads());
#else
	RRRay* rays = RRRay::create(1);
#endif

#pragma omp parallel for schedule(dynamic)
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		ProcessTexelParams pti(tc);
		pti.tri.triangleIndex = (unsigned)t;
		multiMesh->getTriangleBody(pti.tri.triangleIndex,pti.tri.triangleBody);
		RRMesh::TriangleNormals normals;
		multiMesh->getTriangleNormals(t,normals);
		pti.tri.pos3d = pti.tri.triangleBody.vertex0+(pti.tri.triangleBody.side1+pti.tri.triangleBody.side2)*0.333333f;
		pti.tri.normal = (normals.norm[0]+normals.norm[1]+normals.norm[2])*0.333333f;
#ifdef _OPENMP
		pti.ray = rays+omp_get_thread_num();
#else
		pti.ray = rays;
#endif
		pti.ray->rayLengthMin = priv->minimalSafeDistance;
		ProcessTexelResult tr = processTexel(pti);
		results[t] = tr;
	}

	delete[] rays;
	return true;
}

// CPU version, detects per-triangle direct from RRLights, RREnvironment, gathers from current solution
bool RRDynamicSolver::updateSolverDirectIllumination(const UpdateParameters* aparams, bool updateBentNormals)
{
	RRReportInterval report(INF2,"Updating solver direct ...\n");

	if(!getMultiObjectCustom() || !getStaticSolver())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !getStaticSolver())
		{
			RR_ASSERT(0);
			RRReporter::report(WARN,"No objects in scene.\n");
			return false;
		}
	}

	// solution+lights+env -gather-> tmparray
	unsigned numPostImportTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
	ProcessTexelResult* finalGather = new ProcessTexelResult[numPostImportTriangles];
	if(!gatherPerTriangle(aparams,finalGather,numPostImportTriangles))
	{
		delete[] finalGather;
		return false;
	}

	// tmparray -> object
	RRObjectWithIllumination* multiObject = getMultiObjectPhysicalWithIllumination();
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
	TIME now = GETTIME;
	TIME end = *(TIME*)context;
	return now>end && now<(TIME)(end+ULONG_MAX/2);
}

bool RRDynamicSolver::updateSolverIndirectIllumination(const UpdateParameters* aparamsIndirect, unsigned benchTexels, unsigned benchQuality)
{
	if(!getMultiObjectCustom() || !priv->scene)
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !priv->scene)
		{
			RR_ASSERT(0);
			RRReporter::report(WARN,"RRDynamicSolver::updateSolverIndirectIllumination: No objects in scene.\n");
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

	RRReportInterval report(INF2,"Updating solver indirect(%s%s%s%s%s).\n",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.direct)?"D":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.indirect)?"I":"",
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
		RRReal secondsInGather = (GETTIME-t0)/(RRReal)PER_SEC;

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
			RRMesh* multiMesh = getMultiObjectCustom()->getCollider()->getMesh();

			// preallocates rays, allocating inside for cycle costs more
#ifdef _OPENMP
			RRRay* rays = RRRay::create(omp_get_max_threads());
#else
			RRRay* rays = RRRay::create(1);
#endif

#pragma omp parallel for schedule(dynamic)
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
				pti.ray = rays+omp_get_thread_num();
#else
				pti.ray = rays;
#endif
				pti.ray->rayLengthMin = priv->minimalSafeDistance;
				processTexel(pti);
			}

			delete[] rays;

			RRReal secondsInBench = (GETTIME-benchStart)/(RRReal)PER_SEC;
			secondsInPropagatePlan = MAX(0.1f,20*secondsInBench);
		}

		// propagate
		RRReportInterval reportProp(INF2,"Propagating (scheduled %sfor %.1fs) ...\n",paramsIndirect.applyEnvironment?"":"by bench ",secondsInPropagatePlan);
		TIME now = GETTIME;
		TIME end = (TIME)(now+secondsInPropagatePlan*PER_SEC);
		RRStaticSolver::Improvement improvement = priv->scene->illuminationImprove(endByTime,(void*)&end);
		if(improvement!=RRStaticSolver::IMPROVED)
		{
		//	RRReporter::report(CONT," scheduled for %.1fs, ",secondsInPropagatePlan);
		}
		RRReporter::report((improvement==RRStaticSolver::IMPROVED)?INF3:INF2,(improvement==RRStaticSolver::IMPROVED)?"Improved.\n":((improvement==RRStaticSolver::NOT_IMPROVED)?"Not improved.\n":((improvement==RRStaticSolver::FINISHED)?"No light in scene.\n":"Error.\n")));
		// set solver to reautodetect direct illumination (direct illum in solver was just overwritten)
		//  before further realtime rendering
//		reportDirectIlluminationChange(true);
	}
	return true;
}

} // namespace
