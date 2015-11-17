//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
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
	#include "embree2/rtcore.h"
	#include "embree2/rtcore_ray.h"
	#pragma comment(lib,"embree.lib")
#endif

namespace rr
{

#ifdef SUPPORT_EMBREE

////////////////////////////////////////////////////////////////////////////
//
// Embree

static unsigned s_numEmbreeColliders = 0;

class EmbreeCollider : public RRCollider
{
	const RRMesh* rrMesh;
	RTCScene rtcScene;
	RRCollider::IntersectTechnique technique;

	struct Ray : public RTCRay
	{
		RRRay& rrRay;
		Ray(RRRay& _rrRay) : rrRay(_rrRay) {}
	};

	template <class C, class D>
	static void copyVec3(const C& src, D& dst)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}

	static void copyRrRayToRtc(const RRRay& rrRay, RTCRay& rtcRay)
	{
		copyVec3(rrRay.rayOrigin,rtcRay.org);
		copyVec3(rrRay.rayDir,rtcRay.dir);
		rtcRay.tnear = rrRay.rayLengthMin;
		rtcRay.tfar = rrRay.rayLengthMax;
		rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
		rtcRay.primID = RTC_INVALID_GEOMETRY_ID;
		rtcRay.instID = RTC_INVALID_GEOMETRY_ID;
		rtcRay.mask = 0xffffffff;
		rtcRay.time = 0;
	}

	static void copyRtcHitToRr(const RTCRay& rtcRay, RRRay& rrRay)
	{
		if (rrRay.rayFlags&RRRay::FILL_DISTANCE)
			rrRay.hitDistance = rtcRay.tfar;
		if (rrRay.rayFlags&RRRay::FILL_TRIANGLE)
			rrRay.hitTriangle = rtcRay.primID;
		if (rrRay.rayFlags&RRRay::FILL_POINT2D)
			rrRay.hitPoint2d = RRVec2(rtcRay.u,rtcRay.v);
		if (rrRay.rayFlags&RRRay::FILL_POINT3D)
			rrRay.hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay.tfar;
		if (rrRay.rayFlags&RRRay::FILL_PLANE)
		{
			// embree Ng is unnormalized, goes from back side
			// Lightsprint RRVec3(hitPlane) is normalized, goes from front side
			RRVec3 hitPoint3d = rrRay.rayOrigin + rrRay.rayDir*rtcRay.tfar;
			RRVec3 n(-rtcRay.Ng[0],-rtcRay.Ng[1],-rtcRay.Ng[2]);
			float siz = n.length();
			rrRay.hitPlane[0] = n.x/siz;
			rrRay.hitPlane[1] = n.y/siz;
			rrRay.hitPlane[2] = n.z/siz;
			rrRay.hitPlane[3] = -(hitPoint3d[0] * rrRay.hitPlane[0] + hitPoint3d[1] * rrRay.hitPlane[1] + hitPoint3d[2] * rrRay.hitPlane[2]);
		}
		if (rrRay.rayFlags&RRRay::FILL_SIDE)
		{
			RRVec3 n(-rtcRay.Ng[0],-rtcRay.Ng[1],-rtcRay.Ng[2]);
			rrRay.hitFrontSide = rrRay.rayDir.dot(n.normalized())<0;
		}
	}

	static void callback(void* _userPtr, RTCRay& _rtcRay)
	{
		Ray& rtcRay = *(Ray*)&_rtcRay;

		// copy rtcRay to rrRay
		copyRtcHitToRr(rtcRay,rtcRay.rrRay);

		if (rtcRay.rrRay.collisionHandler)
		{
			if (!rtcRay.rrRay.collisionHandler->collides(rtcRay.rrRay))
				rtcRay.geomID = RTC_INVALID_GEOMETRY_ID;
		}
	}

	static unsigned loadMesh(RTCScene rtcScene, const RRMesh* _mesh)
	{
		unsigned numTriangles = _mesh->getNumTriangles();
		unsigned numVertices = _mesh->getNumVertices();
		unsigned geomId = rtcNewTriangleMesh(rtcScene,RTC_GEOMETRY_STATIC,numTriangles,numVertices);

		// indices
		{
			//rtcSetBuffer(rtcScene,geomId,RTC_INDEX_BUFFER,rrMesh->triangle,0,sizeof(RRMesh::Triangle));
			RRMesh::Triangle* buffer = (RRMesh::Triangle*)rtcMapBuffer(rtcScene,geomId,RTC_INDEX_BUFFER);
			for (unsigned i=0;i<numTriangles;i++)
				_mesh->getTriangle(i,buffer[i]);
			rtcUnmapBuffer(rtcScene,geomId,RTC_INDEX_BUFFER);
		}
		
		// positions
		{
			//rtcSetBuffer(rtcScene,geomId,RTC_VERTEX_BUFFER,rrMesh->position,0,sizeof(RRMesh::Vertex));
			RRVec3p* buffer = (RRVec3p*)rtcMapBuffer(rtcScene,geomId,RTC_VERTEX_BUFFER);
			for (unsigned i=0;i<numVertices;i++)
				_mesh->getVertex(i,buffer[i]);
			rtcUnmapBuffer(rtcScene,geomId,RTC_VERTEX_BUFFER);
		}

		rtcSetIntersectionFilterFunction(rtcScene,geomId,callback);

		return geomId;
	}

public:

	EmbreeCollider(RRCollider::IntersectTechnique _technique, const RRMesh* _mesh)
	{
		if (!s_numEmbreeColliders++)
			rtcInit(nullptr);

		rrMesh = _mesh;
		rtcScene = rtcNewScene( RTC_SCENE_DYNAMIC | ((_technique==IT_BVH_COMPACT) ? (RTC_SCENE_INCOHERENT|RTC_SCENE_COMPACT) : RTC_SCENE_INCOHERENT), RTC_INTERSECT1);
		technique = _technique;
		loadMesh(rtcScene,_mesh);
		rtcCommit(rtcScene);
		// rtcCommit used to be very slow for tiny meshes
		// it's fast now. making all 23k objects dynamic in diacor takes 8.8s
		// we can avoid threading overhead completely with rtcCommitThread(rtcScene,0,1), diacor would improve to 7.7s
		// but building single 8M mesh would be probably much slower, so not worth it
	}

	virtual bool intersect(RRRay& rrRay) const
	{
		if (rrRay.collisionHandler)
			rrRay.collisionHandler->init(rrRay);
		Ray rtcRay(rrRay);
		copyRrRayToRtc(rrRay,rtcRay);
		
		// backup rrRay.hitXxx (callback overwrites it, but all intersections can be rejected)
		RRReal          hitDistance  = rrRay.hitDistance;
		unsigned        hitTriangle  = rrRay.hitTriangle;
		RRVec2          hitPoint2d   = rrRay.hitPoint2d;
		RRVec4          hitPlane     = rrRay.hitPlane;
		RRVec3          hitPoint3d   = rrRay.hitPoint3d;
		bool            hitFrontSide = rrRay.hitFrontSide;

		rtcIntersect(rtcScene,rtcRay);
		bool result = rtcRay.primID!=RTC_INVALID_GEOMETRY_ID;
		if (rrRay.collisionHandler)
		{
			// rrRay.hitXxx is overwritten, here we fix it
			// (when custom handler rejects collisions, wrong data are left in rrRay.hitXxx)
			if (result)
				copyRtcHitToRr(rtcRay,rtcRay.rrRay);
			else
			{
				// restore rrRay.hitXxx (callback overwrites it, but all intersections can be rejected)
				rrRay.hitDistance = hitDistance;
				rrRay.hitTriangle = hitTriangle;
				rrRay.hitPoint2d = hitPoint2d;
				rrRay.hitPlane = hitPlane;
				rrRay.hitPoint3d = hitPoint3d;
				rrRay.hitFrontSide = hitFrontSide;
			}
			result = rrRay.collisionHandler->done();
		}
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
			rtcExit();
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
