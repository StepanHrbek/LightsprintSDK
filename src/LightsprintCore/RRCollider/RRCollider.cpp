//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// RRCollider class - generic code for ray-mesh intersections.
// --------------------------------------------------------------------------

#include <map>
#include "RRCollisionHandler.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#ifdef _MSC_VER
	#include <excpt.h> // EXCEPTION_EXECUTE_HANDLER
#endif
#ifdef SUPPORT_EMBREE
	#include "embree3/rtcore.h"
	#include "embree3/rtcore_ray.h"
	#pragma comment(lib,"embree3.lib")
#endif

namespace rr
{

#ifdef SUPPORT_EMBREE

////////////////////////////////////////////////////////////////////////////
//
// Embree

static unsigned s_numEmbreeColliders = 0;
static RTCDevice s_embreeDevice = nullptr;

// as a ray context, we pass this extended structure
struct RTCIntersectContextEx : public RTCIntersectContext
{
	RRRay* rrRay;
};

class EmbreeCollider : public RRCollider
{
	const RRMesh* rrMesh;
	RTCScene rtcScene;
	RRCollider::IntersectTechnique technique;

	struct Ray : public RTCRayHit
	{
		RRRay& rrRay;
		Ray(RRRay& _rrRay) : rrRay(_rrRay) {}
	};

	static void copyRrRayToRtc(const RRRay& rrRay, RTCRay& rtcRay)
	{
		rtcRay.dir_x = rrRay.rayDir.x;
		rtcRay.dir_y = rrRay.rayDir.y;
		rtcRay.dir_z = rrRay.rayDir.z;
		if (rrRay.rayLengthMin>=0)
		{
			rtcRay.org_x = rrRay.rayOrigin.x;
			rtcRay.org_y = rrRay.rayOrigin.y;
			rtcRay.org_z = rrRay.rayOrigin.z;
			rtcRay.tnear = rrRay.rayLengthMin;
			rtcRay.tfar = rrRay.rayLengthMax;
		}
		else
		{
			// our rayLengthMin is usually negative for orthogonal cameras
			// embree would clamp tnear to 0, we need to prevent that
			rtcRay.org_x = rrRay.rayOrigin.x+rrRay.rayDir.x*rrRay.rayLengthMin;
			rtcRay.org_y = rrRay.rayOrigin.y+rrRay.rayDir.y*rrRay.rayLengthMin;
			rtcRay.org_z = rrRay.rayOrigin.z+rrRay.rayDir.z*rrRay.rayLengthMin;
			rtcRay.tnear = 0;
			rtcRay.tfar = rrRay.rayLengthMax-rrRay.rayLengthMin;
		}
		rtcRay.time = 0;
		rtcRay.mask = 0xffffffff;
		rtcRay.id = 0; // not used
		rtcRay.flags = 0; // reserved, must be 0
	}

	static void copyRtcHitToRr(const RTCRayHit& rtcRay, RRRay& rrRay)
	{
		float rtcRay_tfar_fixed = (rrRay.rayLengthMin>=0) ? rtcRay.ray.tfar : (rtcRay.ray.tfar+rrRay.rayLengthMin);
		if (rrRay.rayFlags&RRRay::FILL_DISTANCE)
			rrRay.hitDistance = rtcRay_tfar_fixed; // in callback: "The hit distance is provided as the tfar value of the ray." rtcIntersect1: "When an intersection is found, the hit distance is written into the tfar member of the ray"
		if (rrRay.rayFlags&RRRay::FILL_TRIANGLE)
			rrRay.hitTriangle = rtcRay.hit.primID;
		if (rrRay.rayFlags&RRRay::FILL_POINT2D)
			rrRay.hitPoint2d = RRVec2(rtcRay.hit.u,rtcRay.hit.v);
		if (rrRay.rayFlags&RRRay::FILL_POINT3D)
			rrRay.hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay_tfar_fixed;
		if (rrRay.rayFlags&RRRay::FILL_PLANE)
		{
			// embree Ng is unnormalized, goes from back side
			// Lightsprint RRVec3(hitPlane) is normalized, goes from front side
			RRVec3 hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay_tfar_fixed;
			RRVec3 n(rtcRay.hit.Ng_x,rtcRay.hit.Ng_y,rtcRay.hit.Ng_z);
			float siz = n.length();
			rrRay.hitPlane[0] = n.x/siz;
			rrRay.hitPlane[1] = n.y/siz;
			rrRay.hitPlane[2] = n.z/siz;
			rrRay.hitPlane[3] = -(hitPoint3d[0] * rrRay.hitPlane[0] + hitPoint3d[1] * rrRay.hitPlane[1] + hitPoint3d[2] * rrRay.hitPlane[2]);
		}
		if (rrRay.rayFlags&RRRay::FILL_SIDE)
		{
			RRVec3 n(rtcRay.hit.Ng_x,rtcRay.hit.Ng_y,rtcRay.hit.Ng_z);
			rrRay.hitFrontSide = rrRay.rayDir.dot(n.normalized())<0;
		}
	}

	// Embree's version of Lightsprint's collision handler
	static void intersectFilter(const struct RTCFilterFunctionNArguments* _args)
	{
		RTCIntersectContextEx* contextEx = (RTCIntersectContextEx*)_args->context;
		RRRay* rrRayGlobal = contextEx->rrRay;
		if (rrRayGlobal->collisionHandler)
		{
			if(_args->N!=1)
			{
				// This function should work for all packet sizes, but as we expect only size 1, we tell compiler to hardcode size=1.
				// If we later work with packets, we just remove this if.
				RRReporter::report(ERRO,"Unexpected ray packet.\n");
				return;
			}
			for (unsigned i=0;i<_args->N;i++)
			{
				//const RTCRay* ray = _args->ray[_args->N];
				//const RTCHit* hit = _args->hit[_args->N];
				//const RRRay* rrRay = (*(Ray*)&_args->geometryUserPtr)->rrRay;
				//Ray& rtcRay = *(Ray*)&_args->geometryUserPtr;
				// a) copy embree data into temporary rr ray, so we can call rr callback
				//RRRay rrRay; // possibly unaligned, would be bad for intersections, does not matter for callback
				//rrRay.collisionHandler = ?;
				//_args->geometryUserPtr
				//_args->context->filter

#if 0
				// shorter source code but slower
				RRRay rrRayLocal(*rrRayGlobal); // create local copy of original ray. possibly unaligned, would be bad for intersections, does not matter for callback
				RTCRayHit rtcRayHit;
				rtcRayHit.ray = rtcGetRayFromRayN(_args->ray,_args->N,i);
				rtcRayHit.hit = rtcGetHitFromHitN(_args->hit,_args->N,i);
				copyRtcHitToRr(rtcRayHit,rrRayLocal); // update local copy of hit from embree
#else
				RRRay rrRayLocal; // possibly unaligned, would be bad for intersections, does not matter for callback
				rrRayLocal.rayOrigin = rrRayGlobal->rayOrigin; // =
				//rrRayLocal.rayOrigin.x = RTCRayN_org_x(_args->ray,_args->N,i);
				//rrRayLocal.rayOrigin.y = RTCRayN_org_y(_args->ray,_args->N,i);
				//rrRayLocal.rayOrigin.z = RTCRayN_org_z(_args->ray,_args->N,i);
				rrRayLocal.rayDir = rrRayGlobal->rayDir; // =
				//rrRayLocal.rayDir.x = RTCRayN_dir_x(_args->ray,_args->N,i);
				//rrRayLocal.rayDir.y = RTCRayN_dir_y(_args->ray,_args->N,i);
				//rrRayLocal.rayDir.z = RTCRayN_dir_z(_args->ray,_args->N,i);
				rrRayLocal.rayLengthMin = rrRayGlobal->rayLengthMin; // = RTCRayN_tnear(_args->ray,_args->N,i)
				rrRayLocal.rayLengthMax = rrRayGlobal->rayLengthMax; // embree forgets original value, RTCRayN_tfar(_args->ray,_args->N,i) contains hitDistance
				rrRayLocal.rayFlags = rrRayGlobal->rayFlags;
				rrRayLocal.hitDistance = RTCRayN_tfar(_args->ray,_args->N,i); // in callback: "The hit distance is provided as the tfar value of the ray."
				rrRayLocal.hitTriangle = RTCHitN_primID(_args->hit,_args->N,i);
				rrRayLocal.hitPoint2d.x = RTCHitN_u(_args->hit,_args->N,i);
				rrRayLocal.hitPoint2d.y = RTCHitN_v(_args->hit,_args->N,i);
				rrRayLocal.hitPoint3d = rrRayLocal.rayOrigin + rrRayLocal.rayDir*rrRayLocal.hitDistance;
				{
					// embree Ng is unnormalized, goes from front side
					// Lightsprint RRVec3(hitPlane) is normalized, goes from front side
					RRVec3 n(RTCHitN_Ng_x(_args->hit,_args->N,i),RTCHitN_Ng_y(_args->hit,_args->N,i),RTCHitN_Ng_z(_args->hit,_args->N,i));
					float siz = n.length();
					rrRayLocal.hitPlane[0] = n.x/siz;
					rrRayLocal.hitPlane[1] = n.y/siz;
					rrRayLocal.hitPlane[2] = n.z/siz;
					rrRayLocal.hitPlane[3] = -(rrRayLocal.hitPoint3d[0] * rrRayLocal.hitPlane[0] + rrRayLocal.hitPoint3d[1] * rrRayLocal.hitPlane[1] + rrRayLocal.hitPoint3d[2] * rrRayLocal.hitPlane[2]);
				}
				rrRayLocal.hitFrontSide = rrRayLocal.rayDir.dot(rrRayLocal.hitPlane)<0;
				rrRayLocal.hitObject = rrRayGlobal->hitObject;
				rrRayLocal.collisionHandler = rrRayGlobal->collisionHandler;
#endif

				if (!rrRayLocal.collisionHandler->collides(rrRayLocal))
				{
					_args->valid[i] = 0;
				}

				// in callback: "The filter function is further allowed to change the hit and decrease the tfar value of the ray but it should not modify other ray data nor any inactive components of the ray or hit."
				// so if we ever modify hit or rayLengthMax in our collision handler, we should propagate changese to embree here
				//RTCRayN_tfar(_args->ray,_args->N,i) = rrRayLocal.rayLengthMax;
				//...
			}
		}
	}

	static RTCGeometry loadMesh(const RRMesh* _mesh)
	{
		unsigned numTriangles = _mesh->getNumTriangles();
		unsigned numVertices = _mesh->getNumVertices();
		RTCGeometry geometry = rtcNewGeometry(s_embreeDevice,RTC_GEOMETRY_TYPE_TRIANGLE);
		RRMeshArrays* meshArrays = const_cast<RRMeshArrays*>(dynamic_cast<const RRMeshArrays*>(_mesh));
		if (meshArrays)
		{
			rtcSetSharedGeometryBuffer(geometry,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,meshArrays->triangle,0,sizeof(rr::RRMesh::Triangle),numTriangles);
			rtcSetSharedGeometryBuffer(geometry,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,meshArrays->position,0,sizeof(rr::RRVec3),numVertices);
		}
		else
		{
			// indices
			rtcSetNewGeometryBuffer(geometry,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,sizeof(rr::RRMesh::Triangle),numTriangles);
			RRMesh::Triangle* buffer1 = (RRMesh::Triangle*)rtcGetGeometryBufferData(geometry,RTC_BUFFER_TYPE_INDEX,0);
			if (buffer1)
				for (unsigned i=0;i<numTriangles;i++)
					_mesh->getTriangle(i,buffer1[i]);
			rtcUpdateGeometryBuffer(geometry,RTC_BUFFER_TYPE_INDEX,0);
		
			// positions
			rtcSetNewGeometryBuffer(geometry,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,sizeof(rr::RRVec3p),numVertices);
			RRVec3p* buffer2 = (RRVec3p*)rtcGetGeometryBufferData(geometry,RTC_BUFFER_TYPE_VERTEX,0);
			if (buffer2)
				for (unsigned i=0;i<numVertices;i++)
					_mesh->getVertex(i,buffer2[i]);
			rtcUpdateGeometryBuffer(geometry,RTC_BUFFER_TYPE_VERTEX,0);
		}
		rtcSetGeometryIntersectFilterFunction(geometry,intersectFilter);
		rtcCommitGeometry(geometry);
		return geometry;
	}

	static void errorCallback(void* userPtr, RTCError code, const char* str)
	{
		RRReporter::report(ERRO,"Embree: error %d: %hs\n",code,str);
	}

public:

	EmbreeCollider(RRCollider::IntersectTechnique _technique, const RRMesh* _mesh)
	{
		if (!s_numEmbreeColliders++)
		{
			s_embreeDevice = rtcNewDevice(nullptr);
			rtcSetDeviceErrorFunction(s_embreeDevice,errorCallback,nullptr);
		}

		rrMesh = _mesh;
		rtcScene = rtcNewScene(s_embreeDevice);
		technique = _technique;
		RTCGeometry rtcGeometry = loadMesh(_mesh);
		rtcAttachGeometry(rtcScene,rtcGeometry);
		rtcCommitScene(rtcScene);
	}

	virtual bool intersect(RRRay& rrRay) const
	{
		if (rrRay.collisionHandler)
			rrRay.collisionHandler->init(rrRay);
		Ray rtcRayHit(rrRay);
		copyRrRayToRtc(rrRay,rtcRayHit.ray);
		rtcRayHit.hit.primID = RTC_INVALID_GEOMETRY_ID; // setting is not mentioned in rtcIntersect1 doc, maybe not necessary
		rtcRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
		rtcRayHit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
		RR_ASSERT(RTC_MAX_INSTANCE_LEVEL_COUNT==1); // number of elements in instId array should be 1

		RTCIntersectContextEx rtcIntersectContextEx;
		rtcInitIntersectContext(&rtcIntersectContextEx);
		rtcIntersectContextEx.rrRay = &rrRay;
		rtcIntersect1(rtcScene,&rtcIntersectContextEx,&rtcRayHit);
		bool result = rtcRayHit.hit.primID!=RTC_INVALID_GEOMETRY_ID;
		// ? order of next two ifs ?
		if (rrRay.collisionHandler)
			result = rrRay.collisionHandler->done();
		if (result)
			copyRtcHitToRr(rtcRayHit,rtcRayHit.rrRay);
		return result;
	};

	virtual const RRMesh* getMesh() const
	{
		return rrMesh;
	};

	virtual IntersectTechnique getTechnique() const
	{
		return technique;
	}

	virtual size_t getMemoryOccupied() const
	{
		return 10;
	}

	virtual ~EmbreeCollider()
	{
		if (!--s_numEmbreeColliders)
		{
			rtcReleaseDevice(s_embreeDevice);
			s_embreeDevice = nullptr;
		}
	};
};

#endif // SUPPORT_EMBREE


////////////////////////////////////////////////////////////////////////////
//
// IntersectWrapper

class IntersectWrapper : public RRCollider
{
public:
	IntersectWrapper(const RRMesh* mesh, bool& aborting)
	{
		collider = IntersectLinear::create(mesh);
	}
	virtual ~IntersectWrapper()
	{
		delete collider;
	}
	virtual bool intersect(RRRay& ray) const
	{
		return collider->intersect(ray);
	}
	virtual const RRMesh* getMesh() const
	{
		return collider->getMesh();
	}
	virtual IntersectTechnique getTechnique() const
	{
		return collider->getTechnique();
	}
	virtual void setTechnique(IntersectTechnique technique, bool& aborting)
	{
		if (technique!=collider->getTechnique())
		{
			RRCollider* newCollider = create(getMesh(),nullptr,technique,aborting);
			if (aborting)
				delete newCollider;
			else
			{
				delete collider;
				collider = newCollider;
			}
		}
	}
	virtual size_t getMemoryOccupied() const
	{
		return collider->getMemoryOccupied();
	}
protected:
	RRCollider* collider;
};

////////////////////////////////////////////////////////////////////////////
//
// RRCollider


static std::map<unsigned,RRCollider::Builder*> s_builders;

RRCollider* createMultiCollider(const RRObjects& objects, RRCollider::IntersectTechnique technique, bool& aborting);

RRCollider* defaultBuilder(const RRMesh* mesh, const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
	if (!mesh)
	{
		return objects ? createMultiCollider(*objects,intersectTechnique,aborting) : nullptr;
	}
	BuildParams bp(intersectTechnique);
	if (!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		case RRCollider::IT_BVH_COMPACT:
		case RRCollider::IT_BVH_FAST:
#ifdef SUPPORT_EMBREE
			return new EmbreeCollider(intersectTechnique,mesh);
#else
			//return nullptr;
			return defaultBuilder(mesh,objects,RRCollider::IT_BSP_FAST,aborting,cacheLocation,nullptr);
#endif
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case RRCollider::IT_BSP_COMPACT:
			if (mesh->getNumTriangles()<=256)
			{
				// we expect that mesh with <256 triangles won't produce >64k tree, CBspTree21 has 16bit offsets
				// this is satisfied only with kdleaves enabled
				typedef IntersectBspCompact<CBspTree21> T;
				T* in = T::create(mesh,intersectTechnique,aborting,cacheLocation,".compact",(BuildParams*)buildParams);
				if (in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			if (mesh->getNumTriangles()<=65536)
			{
				typedef IntersectBspCompact<CBspTree42> T;
				T* in = T::create(mesh,intersectTechnique,aborting,cacheLocation,".compact",(BuildParams*)buildParams);
				if (in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			{
				typedef IntersectBspCompact<CBspTree44> T;
				T* in = T::create(mesh,intersectTechnique,aborting,cacheLocation,".compact",(BuildParams*)buildParams);
				size_t size1 = in->getMemoryOccupied();
				if (size1>=10000000)
					RRReporter::report(INF1,"Memory taken by collider(compact): %dMB\n",(unsigned)(size1/1024/1024));
				if (size1>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case RRCollider::IT_BSP_FASTEST:
		case RRCollider::IT_BSP_FASTER:
		case RRCollider::IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(mesh,intersectTechnique,aborting,cacheLocation,(intersectTechnique==RRCollider::IT_BSP_FAST)?".fast":((intersectTechnique==RRCollider::IT_BSP_FASTER)?".faster":".fastest"),(BuildParams*)buildParams);
				size_t size1 = in->getMemoryOccupied();
				if (size1>=10000000)
					RRReporter::report(INF1,"Memory taken by collider(fast*): %dMB\n",(unsigned)(size1/1024/1024));
				if (size1>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case RRCollider::IT_VERIFICATION:
			{
				return IntersectVerification::create(mesh,aborting);
			}
		case RRCollider::IT_LINEAR:
			{
				return new IntersectWrapper(mesh,aborting);
			}
		default:
		linear:
			{
				return IntersectLinear::create(mesh);
			}
	}
}

void RRCollider::registerTechnique(unsigned intersectTechnique, Builder* builder)
{
	s_builders[intersectTechnique] = builder;
}

void RRCollider::setTechnique(IntersectTechnique technique, bool& aborting)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"setTechnique() ignored, collider was not created with IT_LINEAR.\n"));
}

RRCollider* tryBuilder(RRCollider::Builder* builder, const RRMesh* mesh, const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
#ifdef _MSC_VER
	__try
	{
#endif
		return builder(mesh,objects,intersectTechnique,aborting,cacheLocation,buildParams);
#ifdef _MSC_VER
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return (RRCollider*)1;
	}
#endif
}

RRCollider* RRCollider::create(const RRMesh* mesh, const RRObjects* objects, IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
	//RRReportInterval report(INF2,"Creating collider...\n");
	if (s_builders.empty())
	{
		registerTechnique(IT_LINEAR,defaultBuilder);
		registerTechnique(IT_BSP_COMPACT,defaultBuilder);
		registerTechnique(IT_BSP_FAST,defaultBuilder);
		registerTechnique(IT_BSP_FASTER,defaultBuilder);
		registerTechnique(IT_BSP_FASTEST,defaultBuilder);
		registerTechnique(IT_BVH_COMPACT,defaultBuilder);
		registerTechnique(IT_BVH_FAST,defaultBuilder);
		registerTechnique(IT_VERIFICATION,defaultBuilder);
	}

	Builder* builder = s_builders[intersectTechnique];

	if (!builder)
	{
		char tmp[300] = "";
		for (std::map<unsigned,RRCollider::Builder*>::const_iterator i=s_builders.begin();i!=s_builders.end();++i)
			if (strlen(tmp)<270)
				sprintf(tmp+strlen(tmp)," %d",(unsigned)i->first);
		RRReporter::report(ERRO,"No builder registered for IntersectTechnique %d. Registered builders:%s.\n",(int)intersectTechnique,tmp);
		return nullptr;
	}

	RRCollider* result = (RRCollider*)1;
	try
	{
		result = tryBuilder(builder,mesh,objects,intersectTechnique,aborting,cacheLocation,buildParams);
	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, collider not created.\n");
		result = nullptr;
	}
	catch(...)
	{
		RRReporter::report(ERRO,"Builder for IntersectTechnique %d failed.\n",(int)intersectTechnique);
		result = nullptr;
	}
	if (result==(RRCollider*)1)
	{
		RRReporter::report(ERRO,"Builder for IntersectTechnique %d crashed.\n",(int)intersectTechnique);
		result = nullptr;
	}
	if (!result && intersectTechnique!=IT_LINEAR)
	{
		return create(mesh,objects,IT_LINEAR,aborting,cacheLocation,buildParams);
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////
//
// RRCollider getDistanceFromXxx

static void addRay(const RRCollider* collider, RRRay& ray, RRVec3 dir, RRVec2& distanceMinMax)
{
	float dirLength = dir.length();
	dir.normalize();
	ray.rayDir = dir;
	if (dir.finite() && collider->intersect(ray))
	{
		// calculation of distanceOfPotentialNearPlane depends on dir length
		float distanceOfPotentialNearPlane = ray.hitDistance/dirLength;
		distanceMinMax[0] = RR_MIN(distanceMinMax[0],distanceOfPotentialNearPlane);
		distanceMinMax[1] = RR_MAX(distanceMinMax[1],distanceOfPotentialNearPlane);
	}
}

void RRCollider::getDistancesFromPoint(const RRVec3& point, const RRObject* object, bool shadowRays, RRVec2& distanceMinMax, unsigned numRays) const
{
	RRRay ray;
	RRCollisionHandlerFirstVisible collisionHandler(object,shadowRays);
	ray.rayOrigin = point;
	ray.rayLengthMax = 1e12f;
	ray.rayFlags = RRRay::FILL_DISTANCE;
	ray.hitObject = object;
	ray.collisionHandler = &collisionHandler;
	int RAYS = (int)((sqrtf(numRays/6.f)-1)/2); // numRays ~= (2*RAYS+1)^2 * 6
	int nr0 = (2*RAYS+1)*(2*RAYS+1)*6;
	int nr1 = (2*(RAYS+1)+1)*(2*(RAYS+1)+1)*6;
	if (numRays-nr0>nr1-numRays)
		RAYS++;
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			float u = i/(RAYS+0.5f);
			float v = j/(RAYS+0.5f);
			addRay(this,ray,RRVec3(u,v,+1),distanceMinMax);
			addRay(this,ray,RRVec3(u,v,-1),distanceMinMax);
			addRay(this,ray,RRVec3(u,+1,v),distanceMinMax);
			addRay(this,ray,RRVec3(u,-1,v),distanceMinMax);
			addRay(this,ray,RRVec3(+1,u,v),distanceMinMax);
			addRay(this,ray,RRVec3(-1,u,v),distanceMinMax);
		}
	}
}

void RRCollider::getDistancesFromCamera(const RRCamera& camera, const RRObject* object, bool shadowRays, RRVec2& distanceMinMax, unsigned numRays) const
{
	RRRay ray;
	RRCollisionHandlerFirstVisible collisionHandler(object,shadowRays);
	ray.rayLengthMax = 1e12f;
	ray.rayFlags = RRRay::FILL_DISTANCE;
	ray.hitObject = object;
	ray.collisionHandler = &collisionHandler;
	int RAYS = (int)((sqrtf((float)numRays)-1)/2); // numRays ~= (2*RAYS+1)^2
	int nr0 = (2*RAYS+1)*(2*RAYS+1);
	int nr1 = (2*(RAYS+1)+1)*(2*(RAYS+1)+1);
	if (numRays-nr0>nr1-numRays)
		RAYS++;
	for (int i=-RAYS;i<=RAYS;i++)
	{
		for (int j=-RAYS;j<=RAYS;j++)
		{
			RRVec2 posInWindow(i/float(RAYS),j/float(RAYS));
			rr::RRVec3 dir;
			camera.getRay(posInWindow,ray.rayOrigin,dir);
			addRay(this,ray,dir,distanceMinMax);
		}
	}
}


} //namespace
