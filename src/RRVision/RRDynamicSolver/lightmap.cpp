#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../src/RRMath/RRMathPrivate.h"

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}
#define REPORT(a) a
#define HOMOGENOUS_FILL // enables homogenous rather than random(noisy) shooting
//#define BLUR 4 // enables full lightmap blur, higher=stronger
//#define DIAGNOSTIC // sleduje texel overlapy, vypisuje histogram velikosti trianglu
//#define UNWRAP_DIAGNOSTIC // kazdy triangl dostane vlastni nahodnou barvu, tam kde bude videt prechod je spatny unwrap nebo rasterizace
//#define DIAGNOSTIC_RAYS // zaznamenava paprsky vystreleny z texelu lightmapy (RR_DEVELOPMENT must be on)
#define RELIABILITY_FILTERING // prepares data for postprocess, that grows reliable texels into unreliable areas

namespace rr
{

#define LOG_RAY(aeye,adir,adist,hit) { \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[0]=aeye; \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].point[1]=(aeye)+(adir)*(adist); \
	RRStaticSolver::getSceneStatistics()->lineSegments[RRStaticSolver::getSceneStatistics()->numLineSegments].infinite=!hit; \
	++RRStaticSolver::getSceneStatistics()->numLineSegments%=RRStaticSolver::getSceneStatistics()->MAX_LINES; }

struct RenderSubtriangleContext
{
	RRIlluminationPixelBuffer* pixelBuffer;
	RRMesh::TriangleMapping triangleMapping;
};

void renderSubtriangle(const RRStaticSolver::SubtriangleIllumination& si, void* context)
{
	RenderSubtriangleContext* context2 = (RenderSubtriangleContext*)context;
	RRIlluminationPixelBuffer::IlluminatedTriangle si2;
	for(unsigned i=0;i<3;i++)
	{
		si2.iv[i].measure = si.measure[i];
		RR_ASSERT(context2->triangleMapping.uv[0][0]>=0 && context2->triangleMapping.uv[0][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[0][1]>=0 && context2->triangleMapping.uv[0][1]<=1);
		RR_ASSERT(context2->triangleMapping.uv[1][0]>=0 && context2->triangleMapping.uv[1][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[1][1]>=0 && context2->triangleMapping.uv[1][1]<=1);
		RR_ASSERT(context2->triangleMapping.uv[2][0]>=0 && context2->triangleMapping.uv[2][0]<=1);
		RR_ASSERT(context2->triangleMapping.uv[2][1]>=0 && context2->triangleMapping.uv[2][1]<=1);
		RR_ASSERT(si.texCoord[i][0]>=0 && si.texCoord[i][0]<=1);
		RR_ASSERT(si.texCoord[i][1]>=0 && si.texCoord[i][1]<=1);
		// si.texCoord 0,0 prevest na context2->triangleMapping.uv[0]
		// si.texCoord 1,0 prevest na context2->triangleMapping.uv[1]
		// si.texCoord 0,1 prevest na context2->triangleMapping.uv[2]
		si2.iv[i].texCoord = context2->triangleMapping.uv[0] + (context2->triangleMapping.uv[1]-context2->triangleMapping.uv[0])*si.texCoord[i][0] + (context2->triangleMapping.uv[2]-context2->triangleMapping.uv[0])*si.texCoord[i][1];
		RR_ASSERT(si2.iv[i].texCoord[0]>=0 && si2.iv[i].texCoord[0]<=1);
		RR_ASSERT(si2.iv[i].texCoord[1]>=0 && si2.iv[i].texCoord[1]<=1);
		for(unsigned j=0;j<3;j++)
		{
			RR_ASSERT(_finite(si2.iv[i].measure[j]));
			RR_ASSERT(si2.iv[i].measure[j]>=0);
			RR_ASSERT(si2.iv[i].measure[j]<1500000);
		}
	}
	context2->pixelBuffer->renderTriangle(si2);
}

RRIlluminationPixelBuffer* RRDynamicSolver::newPixelBuffer(RRObject* object)
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// homogenous filling:
//   generates points that nearly homogenously (low hustota fluctuations) fill some 2d area

class HomogenousFiller2
{
	unsigned num;
public:
	void Reset(unsigned progress) {num=progress;}
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
	RRReal GetCirclePoint(RRReal *a,RRReal *b)
	{
		RRReal dist;
		do GetTrianglePoint(a,b); while((dist=*a**a+*b**b)>=1);
		return dist;
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


struct TexelContext
{
	RRDynamicSolver* solver;
	RRIlluminationPixelBuffer* pixelBuffer;
	const RRDynamicSolver::UpdateParameters* params;
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
RRColorRGBAF processTexel(const unsigned uv[2], const RRVec3& pos3d, const RRVec3& normal, unsigned triangleIndex, void* context, unsigned resetFiller)
{
	if(!context)
	{
		RRReporter::report(RRReporter::WARN,"processTexel: context==NULL\n");
		RR_ASSERT(0);
		return RRColorRGBAF(0);
	}
	TexelContext* tc = (TexelContext*)context;
	if(!tc->params || !tc->solver || !tc->solver->getMultiObjectCustom())
	{
		RRReporter::report(RRReporter::WARN,"processTexel: No params or no objects in scene\n");
		RR_ASSERT(0);
		return RRColorRGBAF(0);
	}


	// prepare scaler
	const RRScaler* scaler = tc->solver->getScaler();
	
	// prepare collider
	const RRCollider* collider = tc->solver->getMultiObjectCustom()->getCollider();
	SkipTriangle skip(triangleIndex);

	// prepare environment
	const RRIlluminationEnvironmentMap* environment = tc->params->applyEnvironment ? tc->solver->getEnvironment() : NULL;

	// check where to shoot
	bool shootToHemisphere = environment || tc->params->applyCurrentSolution;
	bool shootToLights = tc->params->applyLights && tc->solver->getLights().size();
	if(!shootToHemisphere && !shootToLights)
	{
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"processTexel: Zero workload.\n"));
		RR_ASSERT(0);
		return RRColorRGBAF(0);
	}
	
	// prepare ray
	RRRay* ray = RRRay::create();
	ray->rayOrigin = pos3d;
	ray->rayLengthMin = 0;
	ray->collisionHandler = &skip;

	// shoot into hemisphere
	RRColorRGBF irradianceHemisphere = RRColorRGBF(0);
	RRReal reliabilityHemisphere = 1;
	if(shootToHemisphere)
	{
		// prepare ortonormal base
		RRVec3 n3 = normal.normalized();
		RRVec3 u3 = normalized(ortogonalTo(n3));
		RRVec3 v3 = normalized(ortogonalTo(n3,u3));
		// prepare homogenous filler
		HomogenousFiller2 filler;
		filler.Reset(resetFiller);
		// init counters
		unsigned rays = tc->params->quality ? tc->params->quality : 1;
		unsigned hitsReliable = 0;
		unsigned hitsUnreliable = 0;
		unsigned hitsInside = 0;
		unsigned hitsRug = 0;
		unsigned hitsSky = 0;
		unsigned hitsScene = 0;
		for(unsigned i=0;i<rays;i++)
		{
			// random exit dir
			RRVec3 dir = getRandomExitDir(filler, n3, u3, v3);
			RRReal dirsize = dir.length();
			// intersect scene
			ray->rayDirInv[0] = dirsize/dir[0];
			ray->rayDirInv[1] = dirsize/dir[1];
			ray->rayDirInv[2] = dirsize/dir[2];
			ray->rayLengthMax = 100000; //!!! hard limit
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
			bool hit = collider->intersect(ray);
#ifdef DIAGNOSTIC_RAYS
			LOG_RAY(ray->rayOrigin,dir,hit?ray->hitDistance:0.2f,hit);
#endif
			if(!hit)
			{
				// read irradiance on sky
				if(environment)
				{
					//irradianceIndirect += environment->getValue(dir);
					RRColorRGBF irrad = environment->getValue(dir);
					if(scaler) scaler->getPhysicalScale(irrad);
					irradianceHemisphere += irrad;
				}
				hitsSky++;
				hitsReliable++;
			}
			else
			if(!ray->hitFrontSide) //!!! predelat na obecne, respektovat surfaceBits
			{
				// ray was lost inside object, 
				// increase our transparency, so our color doesn't leak outside object
				hitsInside++;
				hitsUnreliable++;
			}
			else
			if(ray->hitDistance<tc->params->rugDistance)
			{
				// ray hit rug, very close object
				hitsRug++;
				hitsUnreliable++;
			}
			else
			{
				// read cube irradiance as face exitance
				if(tc->params->applyCurrentSolution)
				{
					RRVec3 irrad;
					tc->solver->getStaticSolver()->getTriangleMeasure(ray->hitTriangle,3,RM_EXITANCE_PHYSICAL,NULL,irrad);
					irradianceHemisphere += irrad;
				}
				hitsScene++;
				hitsReliable++;
			}		
		}
		// compute irradiance and reliability
		if(hitsReliable==0)
		{
			// completely unreliable
			irradianceHemisphere = RRColorRGBAF(0);
			reliabilityHemisphere = 0;
		}
		else
		if(hitsInside>rays*tc->params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceHemisphere = RRColorRGBAF(0);
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
	RRReal reliabilityLights = 1;
	if(shootToLights)
	{
		unsigned hitsReliable = 0;
		unsigned hitsUnreliable = 0;
		unsigned hitsLight = 0;
		unsigned hitsInside = 0;
		unsigned hitsRug = 0;
		unsigned hitsScene = 0;
		const RRLights& lights = tc->solver->getLights();
		unsigned rays = (unsigned)lights.size();
		for(unsigned i=0;i<rays;i++)
		{
			const RRLight* light = lights[i];
			if(!light) continue;
			// set dir to light
			RRVec3 dir = (light->type==RRLight::DIRECTIONAL)?-light->direction:(light->position-pos3d);
			RRReal dirsize = dir.length();
			dir /= dirsize;
			if(light->type==RRLight::DIRECTIONAL) dirsize *= 1e6; //!!! fudge number
			float normalIncidence = dot(dir,normal);
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
				ray->rayFlags = RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
				if(!collider->intersect(ray))
				{
					// add irradiance from light
					irradianceLights += light->getIrradiance(pos3d,scaler) * normalIncidence;
					hitsLight++;
					hitsReliable++;
				}
				else
				if(!ray->hitFrontSide) //!!! predelat na obecne, respoktovat surfaceBits
				{
					// ray was lost inside object -> unreliable
					hitsInside++;
					hitsUnreliable++;
				}
				else
				if(ray->hitDistance<tc->params->rugDistance)
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
			reliabilityLights = 0;
		}
		else
		if(hitsInside>rays*tc->params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceLights = RRColorRGBAF(0);
			reliabilityLights = 0;
		}
		else
		{
			// compute reliability
			reliabilityLights = hitsReliable/(RRReal)rays;
		}
	}

	// cleanup ray
	delete ray;

	// sum direct and indirect results
	RRColorRGBAF irradiance = irradianceLights + irradianceHemisphere;
	RRReal reliability = MIN(reliabilityLights,reliabilityHemisphere);

	// scale irradiance (full irradiance, not fraction) to custom scale
	if(tc->pixelBuffer && tc->params->measure.scaled && scaler)
		scaler->getCustomScale(irradiance);

#ifdef BLUR
	reliability /= BLUR;
#endif

#ifdef RELIABILITY_FILTERING 
	// multiply by reliability, it will be corrected in postprocess
	irradiance *= reliability;
	irradiance[3] = reliability;
#endif

	/*/ diagnostic output
	if(tc->params->diagnosticOutput)
	{
		//if(irradiance[3] && irradiance[3]!=1) printf("%d/%d ",hitsInside,hitsSky);
		irradiance[0] = hitsInside/(float)rays;
		irradiance[1] = (rays-hitsInside-hitsSky)/(float)rays;
		irradiance[2] = hitsSky/(float)rays;
		irradiance[3] = 1;
	}*/
	//irradiance[1]=1-reliability;

	// store irradiance in custom scale
	if(tc->pixelBuffer)
		tc->pixelBuffer->renderTexel(uv,irradiance);

	return irradiance;
}

// CPU version, detects per-triangle direct from RRLights, RREnvironment, gathers from current solution
bool RRDynamicSolver::updateSolverDirectIllumination(const UpdateParameters* aparams)
{
	if(!getMultiObjectCustom() || !getStaticSolver())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !getStaticSolver())
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::detectDirectIllumination: No objects in scene.\n");
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

	RRReporter::report(RRReporter::INFO,"Updating solver direct(%s%s%s%s%s)...",
		params.applyLights?"lights ":"",params.applyEnvironment?"env ":"",(params.applyCurrentSolution&&params.measure.direct)?"D":"",(params.applyCurrentSolution&&params.measure.indirect)?"I":"",params.applyCurrentSolution?"cur ":"");
	TIME start = GETTIME;
	TexelContext tc;
	tc.solver = this;
	tc.pixelBuffer = NULL;
	tc.params = &params;
	// split work between multiple shooters on triangle surface
	unsigned numShooters = (unsigned)sqrtf((float)params.quality);
	params.quality /= numShooters;
	// process one texel. for safe preallocations inside texel processor
	unsigned uv[2]={0,0};
	processTexel(uv,RRVec3(0),RRVec3(1),0,&tc,0);
#pragma omp parallel for schedule(dynamic)
	for(int t=0;t<(int)numPostImportTriangles;t++)
	{
		unsigned triangleIndex = (unsigned)t;
		RRMesh::TriangleBody body;
		multiMesh->getTriangleBody(triangleIndex,body);
		RRVec3 vertices[3] = {body.vertex0,body.vertex0+body.side1,body.vertex0+body.side2};
		RRMesh::TriangleNormals normals;
		multiMesh->getTriangleNormals(t,normals);
		normals.norm[1] -= normals.norm[0]; // create delta normals
		normals.norm[2] -= normals.norm[0];
		RRColor color = RRColor(0);
		for(unsigned i=0;i<numShooters;i++)
		{
			// consider using homogenous filler
			RRReal u = rand()*(1.f/RAND_MAX);
			RRReal v = rand()*(1.f/RAND_MAX);
			if(u+v>1)
			{
				u = 1-u;
				v = 1-v;
			}
			RRVec3 pos = body.vertex0+body.side1*u+body.side2*v;
			RRVec3 norm = normals.norm[0]+normals.norm[1]*u+normals.norm[2]*v;
			color += processTexel(uv,pos,norm,triangleIndex,&tc,i*params.quality);
		}
		multiObject->setTriangleIllumination(triangleIndex,RM_IRRADIANCE_PHYSICAL,color/numShooters);
	}
	float secs = (GETTIME-start)/(float)PER_SEC;
	RRReporter::report(RRReporter::CONT," done in %.1fs.\n",secs);

	// object -> solver.direct
	scene->illuminationReset(false,true);
	solutionVersion++;

	return true;
}

#ifdef DIAGNOSTIC
unsigned hist[100];
void logReset()
{
	memset(hist,0,sizeof(hist));
}
void logRasterizedTriangle(unsigned numTexels)
{
	if(numTexels>=100) numTexels = 99;
	hist[numTexels]++;
}
void logPrint()
{
	printf("Numbers of triangles with 0/1/2/...97/98/more texels:\n");
	for(unsigned i=0;i<100;i++)
		printf("%d ",hist[i]);
}
#endif

const int RightHandSide        = -1;
const int LeftHandSide         = +1;
const int CollinearOrientation =  0;

template<typename T>
inline int orientation(const T& x1, const T& y1,
					   const T& x2, const T& y2,
					   const T& px, const T& py)
{
	T orin = (x2 - x1) * (py - y1) - (px - x1) * (y2 - y1);

	if (orin > 0.0)      return LeftHandSide;         /* Orientaion is to the left-hand side  */
	else if (orin < 0.0) return RightHandSide;        /* Orientaion is to the right-hand side */
	else                 return CollinearOrientation; /* Orientaion is neutral aka collinear  */
}

template<typename T>
inline void closest_point_on_segment_from_point(const T& x1, const T& y1,
												const T& x2, const T& y2,
												const T& px, const T& py,
												T& nx,       T& ny)
{
	T vx = x2 - x1;
	T vy = y2 - y1;
	T wx = px - x1;
	T wy = py - y1;

	T c1 = vx * wx + vy * wy;

	if (c1 <= T(0.0))
	{
		nx = x1;
		ny = y1;
		return;
	}

	T c2 = vx * vx + vy * vy;

	if (c2 <= c1)
	{
		nx = x2;
		ny = y2;
		return;
	}

	T ratio = c1 / c2;

	nx = x1 + ratio * vx;
	ny = y1 + ratio * vy;
}

template<typename T>
inline T distance(const T& x1, const T& y1, const T& x2, const T& y2)
{
	T dx = (x1 - x2);
	T dy = (y1 - y2);
	return sqrt(dx * dx + dy * dy);
}

template<typename T>
inline void closest_point_on_triangle_from_point(const T& x1, const T& y1,
												 const T& x2, const T& y2,
												 const T& x3, const T& y3,
												 const T& px, const T& py,
												 T& nx,       T& ny)
{
	if (orientation(x1,y1,x2,y2,px,py) != orientation(x1,y1,x2,y2,x3,y3))
	{
		closest_point_on_segment_from_point(x1,y1,x2,y2,px,py,nx,ny);
		if (orientation(x2,y2,x3,y3,px,py) != orientation(x2,y2,x3,y3,x1,y1))
		{
			RRVec2 m;
			closest_point_on_segment_from_point(x2,y2,x3,y3,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		else
		if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
		{
			RRVec2 m;
			closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		return;
	}

	if (orientation(x2,y2,x3,y3,px,py) != orientation(x2,y2,x3,y3,x1,y1))
	{
		closest_point_on_segment_from_point(x2,y2,x3,y3,px,py,nx,ny);
		RRVec2 m;
		if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
		{
			closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,m.x,m.y);
			if((RRVec2(nx,ny)-RRVec2(px,py)).length2()>(m-RRVec2(px,py)).length2())
			{
				nx = m.x;
				ny = m.y;
			}
		}
		return;
	}

	if (orientation(x3,y3,x1,y1,px,py) != orientation(x3,y3,x1,y1,x2,y2))
	{
		closest_point_on_segment_from_point(x3,y3,x1,y1,px,py,nx,ny);
		return;
	}
	nx = px;
	ny = py;
}

// Enumerates all important texels in ambient map, using softare rasterizer.
void RRDynamicSolver::enumerateTexels(unsigned objectNumber, unsigned mapWidth, unsigned mapHeight,
	RRColorRGBAF (callback)(const unsigned uv[2], const RRVec3& pos3d, const RRVec3& normal, unsigned triangleIndex, void* context, unsigned fillerReset), void* context)
{
	// Iterate through all multimesh triangles (rather than single object's mesh triangles)
	// Advantages:
	//  + vertices and normals in world space
	//  + no degenerated faces
	//!!! Warning: mapping must be preserved by multiobject.
	//    Current multiobject lets object mappings overlap, but it could change in future.

	const RRObject* multiObject = getMultiObjectCustom();
	if(!multiObject)
	{
		RR_ASSERT(0);
		return;
	}
	RRMesh* multiMesh = multiObject->getCollider()->getMesh();
	unsigned numTriangles = multiMesh->getNumTriangles();

	// flag all texels as not yet enumerated
	char* enumerated = new char[mapWidth*mapHeight];
	memset(enumerated,0,mapWidth*mapHeight);

#ifndef DIAGNOSTIC_RAYS
	#pragma omp parallel for schedule(dynamic) // fastest: dynamic, static,1, static
#endif
	for(int tt=0;tt<(int)numTriangles;tt++)
	{
		unsigned t = (unsigned)tt;
		RRMesh::MultiMeshPreImportNumber preImportNumber = multiMesh->getPreImportTriangle(t);
		if(preImportNumber.object==objectNumber)
		{
			// gather data about triangle t
			RRMesh::TriangleBody body;
			multiMesh->getTriangleBody(t,body);
			RRVec3 vertices[3] = {body.vertex0,body.vertex0+body.side1,body.vertex0+body.side2};
			RRMesh::TriangleNormals normals;
			multiMesh->getTriangleNormals(t,normals);
			RRMesh::TriangleMapping mapping;
			multiMesh->getTriangleMapping(t,mapping);
			// rasterize triangle t
			//  find minimal bounding box
			RRReal xmin = mapWidth  * MIN(mapping.uv[0][0],MIN(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal xmax = mapWidth  * MAX(mapping.uv[0][0],MAX(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal ymin = mapHeight * MIN(mapping.uv[0][1],MIN(mapping.uv[1][1],mapping.uv[2][1]));
			RRReal ymax = mapHeight * MAX(mapping.uv[0][1],MAX(mapping.uv[1][1],mapping.uv[2][1]));
			RR_ASSERT(xmin>=0 && xmax<=mapWidth);
			RR_ASSERT(ymin>=0 && ymax<=mapHeight);
			//  precompute mapping[0]..mapping[1] line and mapping[0]..mapping[2] line equations in 2d map space
			#define POINT_LINE_DISTANCE(point,line) \
				((line)[0]*(point)[0]+(line)[1]*(point)[1]+(line)[2])
			#define LINE_EQUATION(lineEquation,lineDirection,pointInDistance0,pointInDistance1) \
				lineEquation = RRVec3((lineDirection)[1],-(lineDirection)[0],0); \
				lineEquation[2] = -POINT_LINE_DISTANCE(pointInDistance0,lineEquation); \
				lineEquation *= 1/POINT_LINE_DISTANCE(pointInDistance1,lineEquation);
			RRVec3 line1InMap; // line equation in 0..1,0..1 map space
			RRVec3 line2InMap;
			LINE_EQUATION(line1InMap,mapping.uv[1]-mapping.uv[0],mapping.uv[0],mapping.uv[2]);
			LINE_EQUATION(line2InMap,mapping.uv[2]-mapping.uv[0],mapping.uv[0],mapping.uv[1]);
			//  for all texels in bounding box
			unsigned numTexels = 0;
			const int overlap = 0; // 0=only texels that contain part of triangle, 1=all texels that touch them by edge or corner
			for(int y=MAX((int)ymin-overlap,0);y<MIN((int)ymax+1+overlap,mapHeight);y++)
			{
				for(int x=MAX((int)xmin-overlap,0);x<MIN((int)xmax+1+overlap,mapWidth);x++)
				{
					// compute uv in triangle
					//  xy = mapSize*mapping[0] -> uvInTriangle = 0,0
					//  xy = mapSize*mapping[1] -> uvInTriangle = 1,0
					//  xy = mapSize*mapping[2] -> uvInTriangle = 0,1
					// do it for 4 corners of texel
					// do it with 1 texel overlap, so that filtering is not necessary
					RRVec2 uvInMapF0 = RRVec2(RRReal(x-overlap)/mapWidth,RRReal(y-overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF1 = RRVec2(RRReal(x-overlap)/mapWidth,RRReal(y+1+overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF2 = RRVec2(RRReal(x+1+overlap)/mapWidth,RRReal(y-overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInMapF3 = RRVec2(RRReal(x+1+overlap)/mapWidth,RRReal(y+1+overlap)/mapHeight); // in 0..1 map space
					RRVec2 uvInTriangle0 = RRVec2(POINT_LINE_DISTANCE(uvInMapF0,line2InMap),POINT_LINE_DISTANCE(uvInMapF0,line1InMap));
					RRVec2 uvInTriangle1 = RRVec2(POINT_LINE_DISTANCE(uvInMapF1,line2InMap),POINT_LINE_DISTANCE(uvInMapF1,line1InMap));
					RRVec2 uvInTriangle2 = RRVec2(POINT_LINE_DISTANCE(uvInMapF2,line2InMap),POINT_LINE_DISTANCE(uvInMapF2,line1InMap));
					RRVec2 uvInTriangle3 = RRVec2(POINT_LINE_DISTANCE(uvInMapF3,line2InMap),POINT_LINE_DISTANCE(uvInMapF3,line1InMap));
					// process only texels at least partially inside triangle
					if((uvInTriangle0[0]>=0 || uvInTriangle1[0]>=0 || uvInTriangle2[0]>=0 || uvInTriangle3[0]>=0)
						&& (uvInTriangle0[1]>=0 || uvInTriangle1[1]>=0 || uvInTriangle2[1]>=0 || uvInTriangle3[1]>=0)
						&& (uvInTriangle0[0]+uvInTriangle0[1]<=1 || uvInTriangle1[0]+uvInTriangle1[1]<=1 || uvInTriangle2[0]+uvInTriangle2[1]<=1 || uvInTriangle3[0]+uvInTriangle3[1]<=1)
						) // <= >= makes small overlap
					{
						// find nearest position inside triangle
						// - center of texel
						RRVec2 uvInMapF = RRVec2((x+0.5f)/mapWidth,(y+0.5f)/mapHeight); // in 0..1 map space
						// - clamp to inside triangle (so that rays are shot from reasonable shooter)
						//   1. clamp to one of 3 lines of triangle - could stay outside triangle
						RRVec2 uvInTriangleTexelCenter = RRVec2(POINT_LINE_DISTANCE(uvInMapF ,line2InMap),POINT_LINE_DISTANCE(uvInMapF ,line1InMap));
						RRVec2 uvInTriangle;
	//					if(uvInTriangle[0]<0 || uvInTriangle[1]<0 || uvInTriangle[0]+uvInTriangle[1]>1)
						{
							RRVec2 uvInMapF2;
							closest_point_on_triangle_from_point<RRReal>(
								mapping.uv[0][0],mapping.uv[0][1],
								mapping.uv[1][0],mapping.uv[1][1],
								mapping.uv[2][0],mapping.uv[2][1],
								uvInMapF[0],uvInMapF[1],
								uvInMapF2[0],uvInMapF2[1]);
/*RRVec2 uvInTriangle2 = RRVec2(POINT_LINE_DISTANCE(uvInMapF2,line2InMap),POINT_LINE_DISTANCE(uvInMapF2,line1InMap));
{
	printf("MAP uv: %.2f_%.2f -> %.2f_%.2f  delta: %.4f_%.4f\n",
		uvInMapF[0],uvInMapF[1],uvInMapF2[0],uvInMapF2[1],uvInMapF2[0]-uvInMapF[0],uvInMapF2[1]-uvInMapF[1]);
	printf("TRI uv: %.2f_%.2f -> %.2f_%.2f  delta: %.4f_%.4f\n",
		uvInTriangle[0],uvInTriangle[1],uvInTriangle2[0],uvInTriangle2[1],uvInTriangle2[0]-uvInTriangle[0],uvInTriangle2[1]-uvInTriangle[1]);
}*/
							uvInTriangle = RRVec2(POINT_LINE_DISTANCE(uvInMapF2,line2InMap),POINT_LINE_DISTANCE(uvInMapF2,line1InMap));
							// hide precision errors
							if(uvInTriangle[0]<0) uvInTriangle[0] = 0;
							if(uvInTriangle[1]<0) uvInTriangle[1] = 0;
							RRReal uvInTriangleSum = uvInTriangle[0]+uvInTriangle[1];
							if(uvInTriangleSum>1) uvInTriangle /= uvInTriangleSum;
						}

						// skip texels outside border, that were already enumerated before
						if(uvInTriangle!=uvInTriangleTexelCenter)
						{
							if(enumerated[x+y*mapWidth])
								continue;
						}

						// compute uv in map and pos/norm in worldspace
						unsigned uvInMapI[2] = {x,y};
						RRVec3 posWorld = body.vertex0 + body.side1*uvInTriangle[0] + body.side2*uvInTriangle[1];
						RRVec3 normalWorld = normals.norm[0] + (normals.norm[1]-normals.norm[0])*uvInTriangle[0] + (normals.norm[2]-normals.norm[0])*uvInTriangle[1];
						// enumerate texel
						enumerated[x+y*mapWidth] = 1;
						callback(uvInMapI,posWorld,normalWorld,t,context,0);
						numTexels++;
					}
				}
			}
#ifdef DIAGNOSTIC
			logRasterizedTriangle(numTexels);
#endif
		}
	}
	delete[] enumerated;
}

unsigned RRDynamicSolver::updateLightmap(unsigned objectNumber, RRIlluminationPixelBuffer* pixelBuffer, const UpdateParameters* aparams)
{
	if(!pixelBuffer)
	{
		RR_ASSERT(0);
		return 0;
	}
	if(!getMultiObjectCustom() || !getStaticSolver())
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !getStaticSolver())
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: No objects in scene.\n");
			return 0;
		}
	}
	if(objectNumber>=getNumObjects())
	{
		RR_ASSERT(0);
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmap: Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getNumObjects()-1);
		return 0;
	}
	const RRObject* object = getMultiObjectCustom();
	RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numPostImportTriangles = mesh->getNumTriangles();

	// validate params
	UpdateParameters params;
	if(aparams) params = *aparams;
	
	// optimize params
	if(params.applyLights && !getLights().size())
		params.applyLights = false;
	if(params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;

#ifdef DIAGNOSTIC
	logReset();
#endif
	pixelBuffer->renderBegin();
	if(params.applyLights || params.applyEnvironment || (params.applyCurrentSolution && params.quality))
	{
		RRReporter::report(RRReporter::INFO,"Updating lightmap, object %d(0..%d), res %d*%d ...",objectNumber,getNumObjects()-1,pixelBuffer->getWidth(),pixelBuffer->getHeight());
		TIME start = GETTIME;
		TexelContext tc;
		tc.solver = this;
		tc.pixelBuffer = pixelBuffer;
		tc.params = &params;
		// process one texel. for safe preallocations inside texel processor
		unsigned uv[2]={0,0};
		processTexel(uv,RRVec3(0),RRVec3(1),0,&tc,0);
		// continue with all texels, possibly in multiple threads
		enumerateTexels(objectNumber,pixelBuffer->getWidth(),pixelBuffer->getHeight(),processTexel,&tc);
		pixelBuffer->renderEnd(true);
#ifdef DIAGNOSTIC
		logPrint();
#endif
		unsigned secs10 = (GETTIME-start)*10/PER_SEC;
		RRReporter::report(RRReporter::CONT," done in %d.%ds.\n",secs10/10,secs10%10);
	}
	else
	if(params.applyCurrentSolution)
	{
		// for each triangle in multimesh
		for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
		{
			// process only triangles belonging to objectHandle
			RRMesh::MultiMeshPreImportNumber preImportTriangle = mesh->getPreImportTriangle(postImportTriangle);
			if(preImportTriangle.object==objectNumber)
			{
				// multiObject must preserve mapping (all objects overlap in one map)
				//!!! this is satisfied now, but it may change in future
				RenderSubtriangleContext rsc;
				rsc.pixelBuffer = pixelBuffer;
				mesh->getTriangleMapping(postImportTriangle,rsc.triangleMapping);
				// render all subtriangles into pixelBuffer using object's unwrap
				scene->getSubtriangleMeasure(postImportTriangle,params.measure,getScaler(),renderSubtriangle,&rsc);
			}
		}
		pixelBuffer->renderEnd(false);
#ifdef DIAGNOSTIC
		logPrint();
#endif
	}
	else
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmap: No lightsources.\n");
		pixelBuffer->renderEnd(false);
		RR_ASSERT(0);
	}
	return 1;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

bool RRDynamicSolver::updateSolverIndirectIllumination(const UpdateParameters* aparamsIndirect, unsigned benchTexels, unsigned benchQuality)
{
	if(!getMultiObjectCustom() || !scene)
	{
		// create objects
		calculateCore(0);
		if(!getMultiObjectCustom() || !scene)
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateSolverIndirectIllumination: No objects in scene.\n");
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

	RRReporter::report(RRReporter::INFO,"Updating solver indirect(%s%s%s%s%s).\n",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.direct)?"D":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.indirect)?"I":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	if(paramsIndirect.applyCurrentSolution)
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: paramsIndirect.applyCurrentSolution ignored, set it in paramsDirect instead.\n");
		paramsIndirect.applyCurrentSolution = 0;
	}

	// gather direct for requested indirect and propagate in solver
	if(paramsIndirect.applyLights || paramsIndirect.applyEnvironment)
	{
		// fix all dirty flags, so next calculateCore doesn't call detectDirectIllumination etc
		calculateCore(0);

		// first gather
		TIME t0 = GETTIME;
		updateSolverDirectIllumination(&paramsIndirect);
		RRReal secondsInGather = (GETTIME-t0)/(RRReal)PER_SEC;

		// auto quality for propagate
		float secondsInPropagatePlan;
		if(paramsIndirect.applyEnvironment)
		{
			// first gather time is robust measure
			secondsInPropagatePlan = secondsInGather;
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
			RRMesh* multiMesh = getMultiObjectCustom()->getCollider()->getMesh();
#pragma omp parallel for schedule(dynamic)
			for(int i=0;i<(int)benchTexels;i++)
			{
				unsigned triangleIndex = (unsigned)i%multiMesh->getNumTriangles();
				RRMesh::TriangleBody body;
				multiMesh->getTriangleBody(triangleIndex,body);
				RRVec3 vertices[3] = {body.vertex0,body.vertex0+body.side1,body.vertex0+body.side2};
				RRMesh::TriangleNormals normals;
				multiMesh->getTriangleNormals(triangleIndex,normals);
				normals.norm[1] -= normals.norm[0]; // create delta normals
				normals.norm[2] -= normals.norm[0];
				RRVec3 pos = body.vertex0+body.side1*0.33f+body.side2*0.33f;
				RRVec3 norm = normals.norm[0]+normals.norm[1]*0.33f+normals.norm[2]*0.33f;
				unsigned uv[2];
				processTexel(uv,pos,norm,triangleIndex,&tc,0);
			}
			RRReal secondsInBench = (GETTIME-benchStart)/(RRReal)PER_SEC;
			secondsInPropagatePlan = MAX(0.1f,15*secondsInBench);
		}

		// propagate
		RRReporter::report(RRReporter::INFO,"Propagating ...");
		//RRReporter::report(RRReporter::CONT," scheduled for %.1fs, ",secondsInPropagatePlan);
		TIME now = GETTIME;
		TIME end = (TIME)(now+secondsInPropagatePlan*PER_SEC);
		RRStaticSolver::Improvement improvement = scene->illuminationImprove(endByTime,(void*)&end);
		RRReal secondsInPropagateReal = (GETTIME-now)/(float)PER_SEC;
		if(improvement!=RRStaticSolver::IMPROVED)
		{
			RRReporter::report(RRReporter::CONT," scheduled for %.1fs, ",secondsInPropagatePlan);
		}
		RRReporter::report(RRReporter::CONT," %s in %.1fs.\n",
			(improvement==RRStaticSolver::IMPROVED)?"improved":((improvement==RRStaticSolver::NOT_IMPROVED)?"not improved":((improvement==RRStaticSolver::FINISHED)?"no light in scene":"error")),
			secondsInPropagateReal
			);
		// set solver to reautodetect direct illumination (direct illum in solver was just overwritten)
		//  before further realtime rendering
//		reportDirectIlluminationChange(true);
	}
	return true;
}

unsigned RRDynamicSolver::updateLightmaps(unsigned layerNumber, bool createMissingBuffers, const UpdateParameters* aparamsDirect, const UpdateParameters* aparamsIndirect)
{
	UpdateParameters paramsIndirect;
	UpdateParameters paramsDirect;
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;
	if(aparamsDirect) paramsDirect = *aparamsDirect;

	RRReporter::report(RRReporter::INFO,"Updating lightmaps (%d,DIRECT(%s%s%s%s%s),INDIRECT(%s%s%s%s%s)).\n",
		layerNumber,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.direct)?"D":"",
		(paramsDirect.applyCurrentSolution&&paramsDirect.measure.indirect)?"I":"",
		paramsDirect.applyCurrentSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.direct)?"D":"",
		(paramsIndirect.applyCurrentSolution&&paramsIndirect.measure.indirect)?"I":"",
		paramsIndirect.applyCurrentSolution?"cur ":"");

	// 0. create missing buffers
	if(createMissingBuffers)
	{
		for(unsigned object=0;object<getNumObjects();object++)
		{
			RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumber);
			if(!layer->pixelBuffer) layer->pixelBuffer = newPixelBuffer(getObject(object));
		}
	}

	if(paramsDirect.applyCurrentSolution && (paramsIndirect.applyLights || paramsIndirect.applyEnvironment))
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: paramsDirect.applyCurrentSolution ignored, can't be combined with paramsIndirect.applyLights/applyEnvironment.\n");
		paramsDirect.applyCurrentSolution = false;
	}

	if(aparamsIndirect)
	{
		// auto quality for first gather
		// shoot 10x less indirect rays than direct
		unsigned numTexels = 0;
		for(unsigned object=0;object<getNumObjects();object++)
		{
			RRIlluminationPixelBuffer* lmap = getIllumination(object)->getLayer(layerNumber)->pixelBuffer;
			if(lmap) numTexels += lmap->getWidth()*lmap->getHeight();
		}
		unsigned numTriangles = getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles();
		paramsIndirect.quality = (unsigned)(paramsDirect.quality*0.1f*numTexels/(numTriangles+1))+1;

		// 1. first gather into solver.direct
		// 2. propagate
		if(!updateSolverIndirectIllumination(&paramsIndirect,numTexels,paramsDirect.quality))
			return 0;
		paramsDirect.applyCurrentSolution = true; // set solution generated here to be gathered in second gather
	}

	// 3. final gather into buffers
	unsigned updatedBuffers = 0;
	for(unsigned object=0;object<getNumObjects();object++)
	{
		RRObjectIllumination::Layer* layer = getIllumination(object)->getLayer(layerNumber);
		if(layer->pixelBuffer)
		{
			updatedBuffers += updateLightmap(object,layer->pixelBuffer,&paramsDirect);
		}
	}
	return updatedBuffers;
}

} // namespace
