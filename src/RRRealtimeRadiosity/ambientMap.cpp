#include <cassert>
#include <cfloat>
#ifdef _OPENMP
#include <omp.h>
#endif
#include "RRRealtimeRadiosity.h"
#include "../src/RRMath/RRMathPrivate.h"

namespace rr
{

struct RenderSubtriangleContext
{
	RRIlluminationPixelBuffer* pixelBuffer;
	RRObject::TriangleMapping triangleMapping;
};

void renderSubtriangle(const RRScene::SubtriangleIllumination& si, void* context)
{
	RenderSubtriangleContext* context2 = (RenderSubtriangleContext*)context;
	RRIlluminationPixelBuffer::IlluminatedTriangle si2;
	for(unsigned i=0;i<3;i++)
	{
		si2.iv[i].measure = si.measure[i];
		assert(context2->triangleMapping.uv[0][0]>=0 && context2->triangleMapping.uv[0][0]<=1);
		assert(context2->triangleMapping.uv[0][1]>=0 && context2->triangleMapping.uv[0][1]<=1);
		assert(context2->triangleMapping.uv[1][0]>=0 && context2->triangleMapping.uv[1][0]<=1);
		assert(context2->triangleMapping.uv[1][1]>=0 && context2->triangleMapping.uv[1][1]<=1);
		assert(context2->triangleMapping.uv[2][0]>=0 && context2->triangleMapping.uv[2][0]<=1);
		assert(context2->triangleMapping.uv[2][1]>=0 && context2->triangleMapping.uv[2][1]<=1);
		assert(si.texCoord[i][0]>=0 && si.texCoord[i][0]<=1);
		assert(si.texCoord[i][1]>=0 && si.texCoord[i][1]<=1);
		// si.texCoord 0,0 prevest na context2->triangleMapping.uv[0]
		// si.texCoord 1,0 prevest na context2->triangleMapping.uv[1]
		// si.texCoord 0,1 prevest na context2->triangleMapping.uv[2]
		si2.iv[i].texCoord = context2->triangleMapping.uv[0] + (context2->triangleMapping.uv[1]-context2->triangleMapping.uv[0])*si.texCoord[i][0] + (context2->triangleMapping.uv[2]-context2->triangleMapping.uv[0])*si.texCoord[i][1];
		assert(si2.iv[i].texCoord[0]>=0 && si2.iv[i].texCoord[0]<=1);
		assert(si2.iv[i].texCoord[1]>=0 && si2.iv[i].texCoord[1]<=1);
		for(unsigned j=0;j<3;j++)
		{
			assert(_finite(si2.iv[i].measure[j]));
			assert(si2.iv[i].measure[j]>=0);
			assert(si2.iv[i].measure[j]<1500000);
		}
	}
	context2->pixelBuffer->renderTriangle(si2);
}

RRIlluminationPixelBuffer* RRRealtimeRadiosity::newPixelBuffer(RRObject* object)
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// homogenous filling:
//   generates points that nearly homogenously (low hustota fluctuations) fill some 2d area

class HomogenousFiller
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

static RRVec3 getRandomExitDir(HomogenousFiller& filler, const RRVec3& norm, const RRVec3& u3, const RRVec3& v3)
// ortonormal space: norm, u3, v3
// returns random direction exitting diffuse surface with 1 or 2 sides and normal norm
{
#if 1 // homogenous fill
	RRReal x,y;
	RRReal cosa=sqrt(1-filler.GetCirclePoint(&x,&y));
	return norm*cosa + u3*x + v3*y;
#else
	// select random vector from srcPoint3 to one halfspace
	// power is assumed to be 1
	RRReal tmp=(RRReal)rand()/RAND_MAX*1;
	RRReal cosa=sqrt(1-tmp);
	RRReal sina=sqrt( tmp );                  // a = rotation angle from normal to side, sin(a) = distance from center of circle
	RRReal b=rand()*2*3.14159265/RAND_MAX;         // b = rotation angle around normal
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
	RRRealtimeRadiosity* solver;
	RRIlluminationPixelBuffer* pixelBuffer;
	const RRRealtimeRadiosity::UpdateLightmapParameters* params;
	unsigned triangleIndex;
};

// thread safe: yes except for first call (pixelBuffer could allocate memory in renderTexel).
//   someone must call us at least once, before multithreading starts.
void processTexel(const unsigned uv[2], const RRVec3& pos3d, const RRVec3& normal, unsigned triangleIndex, void* context)
{
	if(!context)
	{
		assert(0);
		return;
	}
	TexelContext* tc = (TexelContext*)context;
	if(!tc->params || (!tc->params->directQuality && !tc->params->indirectQuality) || !tc->solver || !tc->solver->getMultiObjectCustom())
	{
		assert(0);
		return;
	}
	// cast rays and calculate irradiance
	// - prepare scaler
	const RRScaler* scaler = tc->solver->getScaler();
	// - prepare environment
	const RRIlluminationEnvironmentMap* environment = tc->solver->getEnvironment();
	// - prepare ortonormal base
	RRVec3 n3 = normal.normalized();
	RRVec3 u3 = normalized(ortogonalTo(n3));
	RRVec3 v3 = normalized(ortogonalTo(n3,u3));
	// - prepare homogenous filler
	HomogenousFiller filler;
	filler.Reset();
	// - prepare collider
	const RRCollider* collider = tc->solver->getMultiObjectCustom()->getCollider();
	SkipTriangle skip(triangleIndex);
	// - prepare ray
	RRRay* ray = RRRay::create();
	ray->rayOrigin = pos3d;
	ray->rayLengthMin = 0;
	ray->collisionHandler = &skip;
	// - shoot into scene
	RRColorRGBF irradianceIndirect = RRColorRGBF(0);
	RRReal reliabilityIndirect = 1;
	if(tc->params->indirectQuality)
	{
		unsigned rays = tc->params->indirectQuality;
		unsigned hitsIndirectReliable = 0;
		unsigned hitsIndirectUnreliable = 0;
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
					irradianceIndirect += irrad;
				}
				hitsSky++;
				hitsIndirectReliable++;
			}
			else
			if(!ray->hitFrontSide) //!!! predelat na obecne, respoktovat surfaceBits
			{
				// ray was lost inside object, 
				// increase our transparency, so our color doesn't leak outside object
				hitsInside++;
				hitsIndirectUnreliable++;
			}
			else
			if(ray->hitDistance<tc->params->rugDistance)
			{
				// ray hit rug, very close object
				hitsRug++;
				hitsIndirectUnreliable++;
			}
			else
			{
				// read cube irradiance as face exitance
				RRVec3 irrad;
				tc->solver->getScene()->getTriangleMeasure(ray->hitTriangle,3,RM_EXITANCE_PHYSICAL,NULL,irrad);
				irradianceIndirect += irrad;
				hitsScene++;
				hitsIndirectReliable++;
			}		
		}
		// compute irradiance and reliability
		if(hitsIndirectReliable==0)
		{
			// completely unreliable
			irradianceIndirect = RRColorRGBAF(0);
		}
		else
		if(hitsInside>rays*tc->params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceIndirect = RRColorRGBAF(0);
		}
		else
		{
			// convert to full irradiance (as if all rays hit scene)
			irradianceIndirect /= (RRReal)hitsIndirectReliable;
			// compute reliability
			reliabilityIndirect = hitsIndirectReliable/(RRReal)(hitsIndirectReliable+hitsIndirectUnreliable);
		}
	}
	// - shoot into lights
	RRColorRGBF irradianceDirect = RRColorRGBF(0);
	RRReal reliabilityDirect = 1;
	if(tc->params->directQuality)
	{
		//unsigned hitsDirectReliable = 0;
		//unsigned hitsDirectUnreliable = 0;
		//unsigned hitsLight = 0;
		//unsigned hitsInside = 0;
		//unsigned hitsRug = 0;
		//unsigned hitsScene = 0;
		const RRRealtimeRadiosity::Lights& lights = tc->solver->getLights();
		for(unsigned i=0;i<lights.size();i++)
		{
			const RRLight* light = lights[i];
			if(!light) continue;
			// set dir to light
			RRVec3 dir = (light->type==RRLight::DIRECTIONAL)?-light->direction:(light->position-pos3d);
			RRReal dirsize = dir.length();
			// intesect scene
			ray->rayDirInv[0] = dirsize/dir[0];
			ray->rayDirInv[1] = dirsize/dir[1];
			ray->rayDirInv[2] = dirsize/dir[2];
			ray->rayLengthMax = dirsize;
			ray->rayFlags = RRRay::FILL_SIDE|RRRay::FILL_DISTANCE;
			if(!collider->intersect(ray))
			{
				// add irradiance from light
				irradianceDirect += light->getIrradiance(pos3d,scaler);
		}}/*
				hitsLight++;
				hitsDirectReliable++;
			}
			else
			if(!ray->hitFrontSide) //!!! predelat na obecne, respoktovat surfaceBits
			{
				// ray was lost inside object, 
				// increase our transparency, so our color doesn't leak outside object
				hitsInside++;
				hitsDirectUnreliable++;
			}
			else
			if(ray->hitDistance<tc->params->rugDistance)
			{
				// ray hit rug, very close object
				hitsRug++;
				hitsDirectUnreliable++;
			}
			else
			{
				hitsScene++;
				hitsDirectReliable++;
			}
		}
		// compute irradiance and reliability
		if(hitsDirectReliable==0 && hitsDirectUnreliable)
		{
			// completely unreliable
			irradianceDirect = RRColorRGBAF(0);
		}
		else
		if(hitsInside>rays*tc->params->insideObjectsTreshold)
		{
			// remove exterior visibility from texels inside object
			//  stops blackness from exterior leaking under the wall into interior (koupelna4 scene)
			irradianceIndirect = RRColorRGBAF(0);
		}
		else
		{
			// compute reliability
			reliabilityDirect = hitsDirectReliable/(RRReal)(hitsDirectReliable+hitsDirectUnreliable);
		}*/
	}
	// cleanup ray
	delete ray;
	// sum direct and indirect results
	RRColorRGBAF irradiance = irradianceDirect + irradianceIndirect;
	RRReal reliability = reliabilityIndirect;
	// scale irradiance (full irradiance, not fraction) to custom scale
	if(scaler) scaler->getCustomScale(irradiance);
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
void RRRealtimeRadiosity::enumerateTexels(unsigned objectNumber, unsigned mapWidth, unsigned mapHeight,
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
		assert(0);
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
			RRObject::TriangleNormals normals;
			multiObject->getTriangleNormals(t,normals);
			RRObject::TriangleMapping mapping;
			multiObject->getTriangleMapping(t,mapping);
			// rasterize triangle t
			//  find minimal bounding box
			RRReal xmin = mapWidth  * MIN(mapping.uv[0][0],MIN(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal xmax = mapWidth  * MAX(mapping.uv[0][0],MAX(mapping.uv[1][0],mapping.uv[2][0]));
			RRReal ymin = mapHeight * MIN(mapping.uv[0][1],MIN(mapping.uv[1][1],mapping.uv[2][1]));
			RRReal ymax = mapHeight * MAX(mapping.uv[0][1],MAX(mapping.uv[1][1],mapping.uv[2][1]));
			assert(xmin>=0 && xmax<mapWidth);
			assert(ymin>=0 && ymax<mapHeight);
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

void RRRealtimeRadiosity::updateLightmap(unsigned objectHandle, RRIlluminationPixelBuffer* pixelBuffer, const UpdateLightmapParameters* params)
//!!! pocita jen indirect
{
	if(!scene)
	{
		assert(0);
		return;
	}
	if(objectHandle>=objects.size())
	{
		assert(0);
		return;
	}
	// for one object
	{
		RRObject* object = getMultiObjectCustom();
		if(!object)
		{
			assert(0);
			return;
		}
		RRMesh* mesh = object->getCollider()->getMesh();
		unsigned numPostImportTriangles = mesh->getNumTriangles();
		if(!pixelBuffer)
		{
			RRObjectIllumination* illumination = getIllumination(objectHandle);
			RRObjectIllumination::Channel* channel = illumination->getChannel(resultChannelIndex);
			if(!channel->pixelBuffer) channel->pixelBuffer = newPixelBuffer(getObject(objectHandle));
			pixelBuffer = channel->pixelBuffer;
		}

		if(pixelBuffer)
		{
			pixelBuffer->renderBegin();
			if(params && params->indirectQuality)
			{
				TexelContext tc;
				tc.solver = this;
				tc.pixelBuffer = pixelBuffer;
				UpdateLightmapParameters tmp;
				tc.params = params ? params : &tmp;
				// process one texel. for safe preallocations inside texel processor
				unsigned uv[2]={0,0};
				processTexel(uv,RRVec3(0),RRVec3(1),0,&tc);
				// continue with all texels, possibly in multiple threads
				enumerateTexels(objectHandle,pixelBuffer->getWidth(),pixelBuffer->getHeight(),processTexel,&tc);
				pixelBuffer->renderEnd(true);
			}
			else
			{
				// for each triangle in multimesh
				for(unsigned postImportTriangle=0;postImportTriangle<numPostImportTriangles;postImportTriangle++)
				{
					// process only triangles belonging to objectHandle
					RRMesh::MultiMeshPreImportNumber preImportTriangle = mesh->getPreImportTriangle(postImportTriangle);
					if(preImportTriangle.object==objectHandle)
					{
						// multiObject must preserve mapping (all objects overlap in one map)
						//!!! this is satisfied now, but it may change in future
						RenderSubtriangleContext rsc;
						rsc.pixelBuffer = pixelBuffer;
						object->getTriangleMapping(postImportTriangle,rsc.triangleMapping);
						// render all subtriangles into pixelBuffer using object's unwrap
						scene->getSubtriangleMeasure(postImportTriangle,RM_IRRADIANCE_CUSTOM_INDIRECT,getScaler(),renderSubtriangle,&rsc);
					}
				}
				pixelBuffer->renderEnd(false);
			}
		}
	}
}

void RRRealtimeRadiosity::readPixelResults()
{
	for(unsigned objectHandle=0;objectHandle<objects.size();objectHandle++)
	{
		updateLightmap(objectHandle,NULL,NULL);
	}
}

} // namespace
