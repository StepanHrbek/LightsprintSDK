#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/RRRealtimeRadiosity.h"
#include "../src/RRMath/RRMathPrivate.h"

#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}
#define REPORT(a) a
#define HOMOGENOUS_FILL // enables homogenous rather than random(noisy) shooting
//#define BLUR 4 // enables full lightmap blur, higher=stronger

namespace rr
{

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
	void Reset() {num=0;}
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
	const RRDynamicSolver::UpdateLightmapParameters* params;
	unsigned triangleIndex;
};

// thread safe: yes except for first call (pixelBuffer could allocate memory in renderTexel).
//   someone must call us at least once, before multithreading starts.
void processTexel(const unsigned uv[2], const RRVec3& pos3d, const RRVec3& normal, unsigned triangleIndex, void* context)
{
	if(!context)
	{
		RRReporter::report(RRReporter::WARN,"processTexel: context==NULL\n");
		RR_ASSERT(0);
		return;
	}
	TexelContext* tc = (TexelContext*)context;
	if(!tc->params || !tc->solver || !tc->solver->getMultiObjectCustom())
	{
		RRReporter::report(RRReporter::WARN,"processTexel: No params or no objects in scene\n");
		RR_ASSERT(0);
		return;
	}

	// prepare scaler
	const RRScaler* scaler = tc->solver->getScaler();
	
	// prepare collider
	const RRCollider* collider = tc->solver->getMultiObjectCustom()->getCollider();
	SkipTriangle skip(triangleIndex);

	// prepare environment
	const RRIlluminationEnvironmentMap* environment = tc->params->applyEnvironment ? tc->solver->getEnvironment() : NULL;

	// check where to shoot
	bool shootToHemisphere = environment || tc->params->applyCurrentIndirectSolution;
	bool shootToLights = tc->params->applyLights && tc->solver->getLights().size();
	if(!shootToHemisphere && !shootToLights)
	{
		LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"processTexel: Zero workload.\n"));
		RR_ASSERT(0);
		return;
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
		filler.Reset();
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
			// intesect scene
			ray->rayDirInv[0] = dirsize/dir[0];
			ray->rayDirInv[1] = dirsize/dir[1];
			ray->rayDirInv[2] = dirsize/dir[2];
			ray->rayLengthMax = 100000; //!!! hard limit
			ray->rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
			if(!collider->intersect(ray))
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
				if(tc->params->applyCurrentIndirectSolution)
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
		const RRDynamicSolver::Lights& lights = tc->solver->getLights();
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
			if(dot(dir,normal)<=0)
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
					irradianceLights += light->getIrradiance(pos3d,scaler);
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
	if(scaler) scaler->getCustomScale(irradiance);

#ifdef BLUR
	reliability /= BLUR;
#endif

	// multiply by reliability
	irradiance *= reliability;
	irradiance[3] = reliability;

	/*/ diagnostic output
	if(tc->params->diagnosticOutput)
	{
		//if(irradiance[3] && irradiance[3]!=1) printf("%d/%d ",hitsInside,hitsSky);
		irradiance[0] = hitsInside/(float)rays;
		irradiance[1] = (rays-hitsInside-hitsSky)/(float)rays;
		irradiance[2] = hitsSky/(float)rays;
		irradiance[3] = 1;
	}*/

	// store irradiance in custom scale
	tc->pixelBuffer->renderTexel(uv,irradiance);
}

// Enumerates all important texels in ambient map, using softare rasterizer.
void RRDynamicSolver::enumerateTexels(unsigned objectNumber, unsigned mapWidth, unsigned mapHeight,
	void (callback)(const unsigned uv[2], const RRVec3& pos3d, const RRVec3& normal, unsigned triangleIndex, void* context), void* context)
{
	// Iterate through all multimesh triangles (rather than single object's mesh triangles)
	// Advantages:
	//  + vertices and normals in world space
	//  + no degenerated faces
	//!!! Warning: mapping must be preserved by multiobject.
	//    Current multiobject lets object mappings overlap, but it could change in future.

	RRObject* multiObject = getMultiObjectCustom();
	if(!multiObject)
	{
		RR_ASSERT(0);
		return;
	}
	RRMesh* multiMesh = multiObject->getCollider()->getMesh();
	unsigned numTriangles = multiMesh->getNumTriangles();

	#pragma omp parallel for schedule(static,1)
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
			RR_ASSERT(xmin>=0 && xmax<mapWidth);
			RR_ASSERT(ymin>=0 && ymax<mapHeight);
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
			for(unsigned y=(unsigned)ymin;y<MIN((unsigned)ymax,mapHeight);y++)
			{
				for(unsigned x=(unsigned)xmin;x<MIN((unsigned)xmax,mapWidth);x++)
				{
					// compute uv in triangle
					//  xy = mapSize*mapping[0] -> uvInTriangle = 0,0
					//  xy = mapSize*mapping[1] -> uvInTriangle = 1,0
					//  xy = mapSize*mapping[2] -> uvInTriangle = 0,1
					RRVec2 uvInMapF = RRVec2((x+0.5f)/mapWidth,(y+0.5f)/mapHeight); // in 0..1 map space
					RRVec2 uvInTriangle = RRVec2(
						POINT_LINE_DISTANCE(uvInMapF,line2InMap),
						POINT_LINE_DISTANCE(uvInMapF,line1InMap));
					// process only texels inside triangle
					if(uvInTriangle[0]>=0 && uvInTriangle[1]>=0 && uvInTriangle[0]+uvInTriangle[1]<=1)
					{
						// compute uv in map and pos/norm in worldspace
						unsigned uvInMapI[2] = {x,y};
						RRVec3 posWorld = body.vertex0 + body.side1*uvInTriangle[0] + body.side2*uvInTriangle[1];
						RRVec3 normalWorld = normals.norm[0] + (normals.norm[1]-normals.norm[0])*uvInTriangle[0] + (normals.norm[2]-normals.norm[0])*uvInTriangle[1];
						// enumerate texel
						callback(uvInMapI,posWorld,normalWorld,t,context);
					}
				}
			}
		}
	}
}

bool RRDynamicSolver::updateLightmap(unsigned objectNumber, RRIlluminationPixelBuffer* pixelBuffer, const UpdateLightmapParameters* aparams)
{
	if(!getMultiObjectCustom() || !getStaticSolver())
	{
		// create objects
		calculateCore(0,0);
		if(!getMultiObjectCustom() || !getStaticSolver())
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: No objects in scene.\n");
			return false;
		}
	}
	if(objectNumber>=getNumObjects())
	{
		RR_ASSERT(0);
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmap: Invalid objectNumber (%d, valid is 0..%d).\n",objectNumber,getNumObjects()-1);
		return false;
	}
	RRObject* object = getMultiObjectCustom();
	RRMesh* mesh = object->getCollider()->getMesh();
	unsigned numPostImportTriangles = mesh->getNumTriangles();
	if(!pixelBuffer)
	{
		RRObjectIllumination* illumination = getIllumination(objectNumber);
		RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
		if(!channel->pixelBuffer) channel->pixelBuffer = newPixelBuffer(getObject(objectNumber));
		pixelBuffer = channel->pixelBuffer;
		if(!pixelBuffer)
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmap: newPixelBuffer(getObject(%d)) returned NULL\n",objectNumber);
			return false;
		}
	}

	// validate params
	UpdateLightmapParameters params;
	if(aparams) params = *aparams;
	
	// optimize params
	if(params.applyLights && !getLights().size())
		params.applyLights = false;
	if(params.applyEnvironment && !getEnvironment())
		params.applyEnvironment = false;

	pixelBuffer->renderBegin();
	if(params.applyLights || params.applyEnvironment || (params.applyCurrentIndirectSolution && params.quality))
	{
		RRReporter::report(RRReporter::INFO,"Updating lightmap, object %d/%d, res %d*%d ...",objectNumber+1,getNumObjects(),pixelBuffer->getWidth(),pixelBuffer->getHeight());
		TexelContext tc;
		tc.solver = this;
		tc.pixelBuffer = pixelBuffer;
		tc.params = &params;
		// process one texel. for safe preallocations inside texel processor
		unsigned uv[2]={0,0};
		processTexel(uv,RRVec3(0),RRVec3(1),0,&tc);
		// continue with all texels, possibly in multiple threads
		enumerateTexels(objectNumber,pixelBuffer->getWidth(),pixelBuffer->getHeight(),processTexel,&tc);
		pixelBuffer->renderEnd(true);
		RRReporter::report(RRReporter::CONT," done.\n");
	}
	else
	if(params.applyCurrentIndirectSolution)
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
				scene->getSubtriangleMeasure(postImportTriangle,RM_IRRADIANCE_CUSTOM_INDIRECT,getScaler(),renderSubtriangle,&rsc);
			}
		}
		pixelBuffer->renderEnd(false);
	}
	else
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmap: No lightsources.\n");
		pixelBuffer->renderEnd(false);
		RR_ASSERT(0);
	}
	return true;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

bool RRDynamicSolver::updateLightmaps(unsigned lightmapChannelNumber, const UpdateLightmapParameters* aparamsDirect, const UpdateLightmapParameters* aparamsIndirect)
{
	if(!getMultiObjectCustom() || !scene)
	{
		// create objects
		calculateCore(0,0);
		if(!getMultiObjectCustom() || !scene)
		{
			RR_ASSERT(0);
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: No objects in scene.\n");
			return false;
		}
	}
	// set default params instead of NULL
	UpdateLightmapParameters paramsDirect;
	UpdateLightmapParameters paramsIndirect;
	paramsIndirect.applyCurrentIndirectSolution = false;
	paramsIndirect.applyLights = false;
	paramsIndirect.applyEnvironment = false;
	//paramsDirect.applyCurrentIndirectSolution = false;
	if(aparamsDirect) paramsDirect = *aparamsDirect;
	if(aparamsIndirect) paramsIndirect = *aparamsIndirect;

	RRReporter::report(RRReporter::INFO,"Updating lightmaps (%d,DIRECT(%s%s%s),INDIRECT(%s%s%s)).\n",
		lightmapChannelNumber,
		paramsDirect.applyLights?"lights ":"",paramsDirect.applyEnvironment?"env ":"",paramsDirect.applyCurrentIndirectSolution?"cur ":"",
		paramsIndirect.applyLights?"lights ":"",paramsIndirect.applyEnvironment?"env ":"",paramsIndirect.applyCurrentIndirectSolution?"cur ":"");

	if(paramsIndirect.applyCurrentIndirectSolution)
	{
		RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: paramsIndirect.applyCurrentIndirectSolution ignored, set it in paramsDirect instead.\n");
		paramsIndirect.applyCurrentIndirectSolution = 0;
	}

	// gather direct for requested indirect and propagate in solver
	if(paramsIndirect.applyLights || paramsIndirect.applyEnvironment)
	{
		if(paramsDirect.applyCurrentIndirectSolution)
		{
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: paramsDirect.applyCurrentIndirectSolution ignored, can't be combined with applyLights and/or applyEnvironment.\n");
			paramsDirect.applyCurrentIndirectSolution = false;
		}
		// fix all dirty flags, so next calculateCore doesn't call detectDirectIllumination etc
		calculateCore(0,0);
		// gather
		TIME t0 = GETTIME;
		for(unsigned object=0;object<getNumObjects();object++)
		{
			RRObjectIllumination::Channel* channel = getIllumination(object)->getChannel(lightmapChannelNumber);
			if(!channel->pixelBuffer) channel->pixelBuffer = newPixelBuffer(getObject(object));
			if(channel->pixelBuffer)
				updateLightmap(object,channel->pixelBuffer,&paramsIndirect);
			else
			{
				RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: newPixelBuffer(getObject(%d)) returned NULL\n",object);
				RR_ASSERT(0);
			}
		}
		RRReal secondsInGather = (GETTIME-t0)/(RRReal)PER_SEC;
		// feed solver with recently gathered illum
		//!!! float precision is lost here
		//!!! v quake levelu tu zdetekuje 0
		detectDirectIlluminationFromLightmaps(lightmapChannelNumber);
		// propagate
		RRReporter::report(RRReporter::INFO,"Propagating ...");
		scene->illuminationReset(false,true);
		float secondsInPropagate = MAX(secondsInGather/MAX(paramsIndirect.quality,1)*MAX(1,((RRReal)paramsDirect.quality+paramsIndirect.quality)/2),5);
		TIME now = GETTIME;
		TIME end = (TIME)(now+secondsInPropagate*PER_SEC);
		scene->illuminationImprove(endByTime,(void*)&end);
		RRReporter::report(RRReporter::CONT," done.\n");
		// set solution generated here to be gathered in second gather
		paramsDirect.applyCurrentIndirectSolution = true;
		// set solver to reautodetect direct illumination (direct illum in solver was just overwritten)
		//  before further realtime rendering
//		reportDirectIlluminationChange(true);
	}
	// gather requested direct and solution
	for(unsigned object=0;object<getNumObjects();object++)
	{
		RRObjectIllumination::Channel* channel = getIllumination(object)->getChannel(lightmapChannelNumber);
		if(!channel->pixelBuffer) channel->pixelBuffer = newPixelBuffer(getObject(object));
		if(channel->pixelBuffer)
			updateLightmap(object,channel->pixelBuffer,&paramsDirect);
		else
		{
			RRReporter::report(RRReporter::WARN,"RRDynamicSolver::updateLightmaps: newPixelBuffer(getObject(%d)) returned NULL\n",object);
			RR_ASSERT(0);
		}
	}
	return true;
}

void RRDynamicSolver::readPixelResults()
{
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		updateLightmap(objectHandle,NULL,NULL);
	}
}

} // namespace
