// --------------------------------------------------------------------------
// RRCollider class - generic code for ray-mesh intersections.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRCollider.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
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
		collider = create(mesh,IT_LINEAR,aborting);
	}
	virtual ~IntersectWrapper()
	{
		delete collider;
	}
	virtual bool intersect(RRRay* ray) const
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
			RRCollider* newCollider = create(getMesh(),technique,aborting);
			if (aborting)
				delete newCollider;
			else
			{
				delete collider;
				collider = newCollider;
			}
		}
	}
	virtual unsigned getMemoryOccupied() const
	{
		return collider->getMemoryOccupied();
	}
protected:
	RRCollider* collider;
};

////////////////////////////////////////////////////////////////////////////
//
// RRCollider


void RRCollider::setTechnique(IntersectTechnique technique, bool& aborting)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"setTechnique() ignored, collider was not created with IT_CHANGEABLE.\n"));
}

RRCollider* RRCollider::create(const RRMesh* mesh, IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation, void* buildParams)
{
	try {

	if (!mesh) return NULL;
	BuildParams bp(intersectTechnique);
	if (!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case IT_BSP_COMPACT:
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
				unsigned size1 = in->getMemoryOccupied();
				if (size1>=10000000)
					RRReporter::report(INF1,"Memory taken by collider(compact): %dMB\n",size1/1024/1024);
				if (size1>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_BSP_FASTEST:
		case IT_BSP_FASTER:
		case IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(mesh,intersectTechnique,aborting,cacheLocation,(intersectTechnique==IT_BSP_FAST)?".fast":((intersectTechnique==IT_BSP_FASTER)?".faster":".fastest"),(BuildParams*)buildParams);
				unsigned size1 = in->getMemoryOccupied();
				if (size1>=10000000)
					RRReporter::report(INF1,"Memory taken by collider(fast*): %dMB\n",size1/1024/1024);
				if (size1>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_VERIFICATION:
			{
				return IntersectVerification::create(mesh,aborting);
			}
		case IT_LINEAR:
			{
				return IntersectLinear::create(mesh);
			}
		case IT_CHANGEABLE:
		default:
		linear:
			{
				return new IntersectWrapper(mesh,aborting);
			}
	}

	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, collider not created.\n");
		return NULL;
	}
}

void RRCollider::intersectBatch(RRRay* ray, unsigned numRays) const
{
	#pragma omp parallel for schedule(static,1)
	for (int i=0;i<(int)numRays;i++)
	{
		if (!intersect(ray+i)) ray[i].hitDistance = -1;
	}
}





const char* loadLicense(const char* filename)
{
	return NULL;
}


} //namespace
