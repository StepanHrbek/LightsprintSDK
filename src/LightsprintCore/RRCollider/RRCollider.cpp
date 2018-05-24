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
	#include "EmbreeCollider.h"
#endif

namespace rr
{

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
	virtual void update()
	{
		collider->update();
	};
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
			return defaultBuilder(mesh,objects,RRCollider::IT_BSP_FAST,aborting,cacheLocation,nullptr);
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
		registerTechnique(IT_VERIFICATION,defaultBuilder);
		registerEmbree(); // when enabled, embree adds its own techniques and overrides IT_LINEAR registered above
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
