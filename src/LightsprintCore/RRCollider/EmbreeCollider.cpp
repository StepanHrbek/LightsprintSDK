//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// EmbreeCollider class - Embree based colliders.
// --------------------------------------------------------------------------

#if !defined(_M_X64) && !defined(_LP64)
// We don't provide 32bit embree build.
// If you use SDK in 32bit, consider building embree and removing this undef to speed up ray scene intersections.
#undef SUPPORT_EMBREE
#endif

#ifdef SUPPORT_EMBREE

#include "Lightsprint/RRObject.h"
#include "EmbreeCollider.h"
#include "embree3/rtcore.h"
#include "embree3/rtcore_ray.h"
#pragma comment(lib,"embree3.lib")

namespace rr
{

// we use statics to init and shutdown embree automatically
// embree shuts down when we delete last collider
static unsigned s_numEmbreeColliders = 0;
static RTCDevice s_embreeDevice = nullptr;

////////////////////////////////////////////////////////////////////////////
//
// RTCIntersectContextEx
//
// as a ray context, we pass this extended structure

struct RTCIntersectContextEx : public RTCIntersectContext
{
	RRRay* rrRay;
	const RRObjects* objects;
};

////////////////////////////////////////////////////////////////////////////
//
// EmbreeCollider
//
// Embree based collider that supports both single mesh and set of objects.

class EmbreeCollider : public RRCollider
{
	const RRMesh* mesh;
	unsigned meshVersion;
	const RRObjects* objects;
	RRCollider::IntersectTechnique technique;

	RTCScene rtcScene;

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

	static void copyRtcHitToRr(const RTCRayHit& rtcRay, const RRObjects* objects, RRRay& rrRay)
	{
		const RRMatrix3x4Ex* worldMatrix = nullptr; // first, find out local world matrix, so we can convert hit from local space to world space
		if (objects)
		{
			if (rtcRay.hit.instID[0]<objects->size())
			{
				rrRay.hitObject = (*objects)[rtcRay.hit.instID[0]];
				worldMatrix = rrRay.hitObject->getWorldMatrix();
			}
			else
				RR_ASSERT(0);
		}

		float rtcRay_tfar_fixed = (rrRay.rayLengthMin>=0) ? rtcRay.ray.tfar : (rtcRay.ray.tfar+rrRay.rayLengthMin);
		if (rrRay.rayFlags&RRRay::FILL_DISTANCE)
		{
			rrRay.hitDistance = rtcRay_tfar_fixed; // local space. in callback: "The hit distance is provided as the tfar value of the ray." rtcIntersect1: "When an intersection is found, the hit distance is written into the tfar member of the ray"
			if (worldMatrix)
				; // !!!world space
		}
		if (rrRay.rayFlags&RRRay::FILL_TRIANGLE)
			rrRay.hitTriangle = rtcRay.hit.primID;
		if (rrRay.rayFlags&RRRay::FILL_POINT2D)
			rrRay.hitPoint2d = RRVec2(rtcRay.hit.u,rtcRay.hit.v);
		if (rrRay.rayFlags&RRRay::FILL_POINT3D)
			rrRay.hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay_tfar_fixed; // world space
		if (rrRay.rayFlags&RRRay::FILL_PLANE || rrRay.rayFlags&RRRay::FILL_SIDE)
		{
			// embree Ng is unnormalized, goes from front side
			// Lightsprint RRVec3(hitPlane) is normalized, goes from front side
			RRVec3 hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay_tfar_fixed;
			RRVec3 n(rtcRay.hit.Ng_x,rtcRay.hit.Ng_y,rtcRay.hit.Ng_z); // local space
			if (worldMatrix)
				worldMatrix->transformNormal(n); // world space
			float siz = n.length();
			if (rrRay.rayFlags&RRRay::FILL_PLANE)
			{
				rrRay.hitPlane[0] = n.x/siz;
				rrRay.hitPlane[1] = n.y/siz;
				rrRay.hitPlane[2] = n.z/siz;
				rrRay.hitPlane[3] = -(hitPoint3d[0] * rrRay.hitPlane[0] + hitPoint3d[1] * rrRay.hitPlane[1] + hitPoint3d[2] * rrRay.hitPlane[2]);
			}
			if (rrRay.rayFlags&RRRay::FILL_SIDE)
			{
				rrRay.hitFrontSide = rrRay.rayDir.dot(n.normalized())<0;
			}
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
#if 0
				// shorter source code but slower
				RRRay rrRayLocal(*rrRayGlobal); // create local copy of original ray. possibly unaligned, would be bad for intersections, does not matter for callback
				RTCRayHit rtcRayHit;
				rtcRayHit.ray = rtcGetRayFromRayN(_args->ray,_args->N,i);
				rtcRayHit.hit = rtcGetHitFromHitN(_args->hit,_args->N,i);
				copyRtcHitToRr(rtcRayHit,rrRayLocal); // update local copy of hit from embree
#else
				RRRay rrRayLocal; // possibly unaligned, would be bad for intersections, does not matter for callback
				rrRayLocal.hitObject = rrRayGlobal->hitObject;
				const RRMatrix3x4Ex* worldMatrix = nullptr; // first, find out local world matrix, so we can convert hit from local space to world space
				if (contextEx->objects)
				{
					if (RTCHitN_instID(_args->hit,_args->N,i,0)<contextEx->objects->size())
					{
						rrRayLocal.hitObject = (*contextEx->objects)[RTCHitN_instID(_args->hit,_args->N,i,0)];
						if (!rrRayLocal.hitObject->enabled)
							goto continue_ray;
						worldMatrix = rrRayLocal.hitObject->getWorldMatrix();
					}
					else
						RR_ASSERT(0);
				}
				rrRayLocal.rayOrigin = rrRayGlobal->rayOrigin; // world space
				//rrRayLocal.rayOrigin.x = RTCRayN_org_x(_args->ray,_args->N,i); // the same in local space
				//rrRayLocal.rayOrigin.y = RTCRayN_org_y(_args->ray,_args->N,i);
				//rrRayLocal.rayOrigin.z = RTCRayN_org_z(_args->ray,_args->N,i);
				rrRayLocal.rayDir = rrRayGlobal->rayDir; // world space
				//rrRayLocal.rayDir.x = RTCRayN_dir_x(_args->ray,_args->N,i); // the same in local space
				//rrRayLocal.rayDir.y = RTCRayN_dir_y(_args->ray,_args->N,i);
				//rrRayLocal.rayDir.z = RTCRayN_dir_z(_args->ray,_args->N,i);
				rrRayLocal.rayLengthMin = rrRayGlobal->rayLengthMin; // world space
				//rrRayLocal.rayLengthMin = RTCRayN_tnear(_args->ray,_args->N,i); // the same, might be in local space
				rrRayLocal.rayLengthMax = rrRayGlobal->rayLengthMax; // world space. embree forgets original value, RTCRayN_tfar(_args->ray,_args->N,i) contains hitDistance
				rrRayLocal.rayFlags = rrRayGlobal->rayFlags;
				rrRayLocal.hitDistance = RTCRayN_tfar(_args->ray,_args->N,i); // local space. in callback: "The hit distance is provided as the tfar value of the ray."
				if (worldMatrix)
					; //!!! world space
				rrRayLocal.hitTriangle = RTCHitN_primID(_args->hit,_args->N,i);
				rrRayLocal.hitPoint2d.x = RTCHitN_u(_args->hit,_args->N,i);
				rrRayLocal.hitPoint2d.y = RTCHitN_v(_args->hit,_args->N,i);
				rrRayLocal.hitPoint3d = rrRayLocal.rayOrigin + rrRayLocal.rayDir*rrRayLocal.hitDistance; // world space
				{
					// embree Ng is unnormalized, goes from front side
					// Lightsprint RRVec3(hitPlane) is normalized, goes from front side
					RRVec3 n(RTCHitN_Ng_x(_args->hit,_args->N,i),RTCHitN_Ng_y(_args->hit,_args->N,i),RTCHitN_Ng_z(_args->hit,_args->N,i)); // local space
					if (worldMatrix)
						worldMatrix->transformNormal(n); // world space
					float siz = n.length();
					rrRayLocal.hitPlane[0] = n.x/siz;
					rrRayLocal.hitPlane[1] = n.y/siz;
					rrRayLocal.hitPlane[2] = n.z/siz;
					rrRayLocal.hitPlane[3] = -(rrRayLocal.hitPoint3d[0] * rrRayLocal.hitPlane[0] + rrRayLocal.hitPoint3d[1] * rrRayLocal.hitPlane[1] + rrRayLocal.hitPoint3d[2] * rrRayLocal.hitPlane[2]);
				}
				rrRayLocal.hitFrontSide = rrRayLocal.rayDir.dot(rrRayLocal.hitPlane)<0;
				rrRayLocal.collisionHandler = rrRayGlobal->collisionHandler;
#endif

				if (!rrRayLocal.collisionHandler->collides(rrRayLocal))
				{
				continue_ray:
					_args->valid[i] = 0;
				}

				// in callback: "The filter function is further allowed to change the hit and decrease the tfar value of the ray but it should not modify other ray data nor any inactive components of the ray or hit."
				// so if we ever modify hit or rayLengthMax in our collision handler, we should propagate changese to embree here
				//RTCRayN_tfar(_args->ray,_args->N,i) = rrRayLocal.rayLengthMax;
				//...
			}
		}
	}

	static RTCGeometry loadMesh(const RRMesh* _mesh, unsigned& _outMeshVersion)
	{
		unsigned numTriangles = _mesh->getNumTriangles();
		unsigned numVertices = _mesh->getNumVertices();
		RTCGeometry geometry = rtcNewGeometry(s_embreeDevice,RTC_GEOMETRY_TYPE_TRIANGLE);
		RRMeshArrays* meshArrays = const_cast<RRMeshArrays*>(dynamic_cast<const RRMeshArrays*>(_mesh));
		if (meshArrays)
		{
			rtcSetSharedGeometryBuffer(geometry,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,meshArrays->triangle,0,sizeof(rr::RRMesh::Triangle),numTriangles);
			rtcSetSharedGeometryBuffer(geometry,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,meshArrays->position,0,sizeof(rr::RRVec3),numVertices);
			_outMeshVersion = meshArrays->version;
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

			_outMeshVersion = 0;
		}
		rtcSetGeometryIntersectFilterFunction(geometry,intersectFilter);
		rtcCommitGeometry(geometry);
		return geometry;
	}

	static RTCGeometry loadObject(const RRObject* _object)
	{
		RTCGeometry geometry = rtcNewGeometry(s_embreeDevice,RTC_GEOMETRY_TYPE_INSTANCE);
		EmbreeCollider* ec = dynamic_cast<EmbreeCollider*>(_object->getCollider());
		if (ec)
		{
			RR_ASSERT(!ec->objects); // embree natively supports only one level of instancing, so make sure we are instancing single mesh, not objects
			ec->update(); // when building multicollider, update 1colliders that need it (SV allows changing static objects and their coliders don't update. but when they become dynamic, we need them updated)
			rtcSetGeometryInstancedScene(geometry,ec->rtcScene);
			// Please note that rtcCommitScene on the instanced scene should be called first, followed by rtcCommitGeometry on the instance, followed by rtcCommitScene for the top-level scene containing the instance.
			if (_object->getWorldMatrix())
				rtcSetGeometryTransform(geometry,0,RTC_FORMAT_FLOAT3X4_ROW_MAJOR,_object->getWorldMatrix()->m[0]);
			rtcCommitGeometry(geometry);
		}
		else
		{
			RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Non-embree collider passed into embree multicollider, ignored.\n"));
		}
		return geometry;
	}

	static void errorCallback(void* userPtr, RTCError code, const char* str)
	{
		RRReporter::report(ERRO,"Embree: error %d: %hs\n",code,str);
	}

public:

	// creates RRCollider for single RRMesh or for multiple RRObject
	EmbreeCollider(const RRMesh* _mesh, const RRObjects* _objects, RRCollider::IntersectTechnique _technique, bool& aborting)
	{
		if (!s_numEmbreeColliders++)
		{
			s_embreeDevice = rtcNewDevice(nullptr);
			rtcSetDeviceErrorFunction(s_embreeDevice,errorCallback,nullptr);
		}
		mesh = _mesh;
		meshVersion = 0;
		objects = _objects;
		rtcScene = rtcNewScene(s_embreeDevice);
		technique = _technique;
		if (objects)
			for (unsigned i=0;i<objects->size();i++)
				rtcAttachGeometryByID(rtcScene,loadObject((*objects)[i]),i);
		else
		if (mesh)
			rtcAttachGeometryByID(rtcScene,loadMesh(mesh,meshVersion),0);
		rtcCommitScene(rtcScene);
	}

	virtual void update()
	{
		if (objects)
		{
			for (unsigned i=0;i<objects->size();i++)
			{
				RTCGeometry geometry = rtcGetGeometry(rtcScene,i);
				if ((*objects)[i]->getWorldMatrix())
					rtcSetGeometryTransform(geometry,0,RTC_FORMAT_FLOAT3X4_ROW_MAJOR,(*objects)[i]->getWorldMatrix()->m[0]);
				rtcCommitGeometry(geometry);
			}
		}
		else
		{
			RRMeshArrays* meshArrays = const_cast<RRMeshArrays*>(dynamic_cast<const RRMeshArrays*>(mesh));
			if (mesh && (!meshArrays || meshArrays->version!=meshVersion)) // RRMesh that is not RRMeshArrays (legacy meshes, multimesh) gets reloaded even when unchanged
			{
				//??? does this leak, do i have to delete something
				rtcDetachGeometry(rtcScene,0);
				rtcAttachGeometry(rtcScene,loadMesh(mesh,meshVersion));
			}
		}
		rtcCommitScene(rtcScene);
	}

	virtual bool intersect(RRRay& rrRay) const
	{
		if (rrRay.collisionHandler)
			rrRay.collisionHandler->init(rrRay);
		RTCRayHit rtcRayHit;
		copyRrRayToRtc(rrRay,rtcRayHit.ray);
		rtcRayHit.hit.primID = RTC_INVALID_GEOMETRY_ID; // setting is not mentioned in rtcIntersect1 doc, maybe not necessary
		rtcRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
		rtcRayHit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
		RR_ASSERT(RTC_MAX_INSTANCE_LEVEL_COUNT==1); // number of elements in instId array should be 1

		RTCIntersectContextEx rtcIntersectContextEx;
		rtcInitIntersectContext(&rtcIntersectContextEx);
		rtcIntersectContextEx.rrRay = &rrRay;
		rtcIntersectContextEx.objects = objects;
		rtcIntersect1(rtcScene,&rtcIntersectContextEx,&rtcRayHit);
		bool result = rtcRayHit.hit.primID!=RTC_INVALID_GEOMETRY_ID;
		// ? order of next two ifs ?
		if (rrRay.collisionHandler)
			result = rrRay.collisionHandler->done();
		if (result)
			copyRtcHitToRr(rtcRayHit,objects,rrRay);
		return result;
	};

	virtual const RRMesh* getMesh() const
	{
		return mesh;
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

////////////////////////////////////////////////////////////////////////////
//
// embree registration
//

static RRCollider* embreeBuilder(const RRMesh* mesh, const RRObjects* objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
	return new EmbreeCollider(mesh, objects, intersectTechnique, aborting);
}

void registerEmbree()
{
	RRCollider::registerTechnique(RRCollider::IT_BVH_COMPACT,embreeBuilder);
	RRCollider::registerTechnique(RRCollider::IT_BVH_FAST,embreeBuilder);
	
	// All objects start with IT_LINEAR collider, it does not consume any time/memory.
	// Without embree, we might _later_ need collisions, so only then we call setTechnique(something faster).
	// With embree enabled, we tend to need collisions immediately, so we build embree collider immediately, even for IT_LINEAR.
	RRCollider::registerTechnique(RRCollider::IT_LINEAR,embreeBuilder);
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
