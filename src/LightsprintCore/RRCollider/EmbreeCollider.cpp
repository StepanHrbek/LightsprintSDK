//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// EmbreeCollider class - Embree based colliders.
// --------------------------------------------------------------------------

#ifdef SUPPORT_EMBREE

#include "Lightsprint/RRObject.h"
#include "EmbreeCollider.h"
#include "embree3/rtcore.h"
#include "embree3/rtcore_ray.h"
#pragma comment(lib,"embree3.lib")

namespace rr
{

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

	virtual void update()
	{
		if (rrMesh)
		{
			//??? does this leak, do i have to delete something
			rtcDetachGeometry(rtcScene,0);
			rtcAttachGeometry(rtcScene,loadMesh(rrMesh));
		}
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

static RRCollider* embreeBuilder(const RRMesh* mesh, const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
	return new EmbreeCollider(intersectTechnique, mesh);
}

void registerEmbree()
{
	RRCollider::registerTechnique(RRCollider::IT_BVH_COMPACT,embreeBuilder);
	RRCollider::registerTechnique(RRCollider::IT_BVH_FAST,embreeBuilder);
}

} //namespace

#else // SUPPORT_EMBREE

namespace rr
{
	void registerEmbree()
	{
	}
}

#endif // SUPPORT_EMBREE
