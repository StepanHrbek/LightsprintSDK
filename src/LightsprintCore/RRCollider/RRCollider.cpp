// --------------------------------------------------------------------------
// RRCollider class - generic code for ray-mesh intersections.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRCollider.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"

#include <assert.h>
#include <memory.h>
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif


namespace rr
{


const RRCollider* RRCollider::create(const RRMesh* importer, IntersectTechnique intersectTechnique, const char* cacheLocation, void* buildParams)
{
	try {

	if (!importer) return NULL;
	BuildParams bp(intersectTechnique);
	if (!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case IT_BSP_COMPACT:
			if (importer->getNumTriangles()<=256)
			{
				// we expect that mesh with <256 triangles won't produce >64k tree, CBspTree21 has 16bit offsets
				// this is satisfied only with kdleaves enabled
				typedef IntersectBspCompact<CBspTree21> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if (in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			if (importer->getNumTriangles()<=65536)
			{
				typedef IntersectBspCompact<CBspTree42> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if (in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			{
				typedef IntersectBspCompact<CBspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
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
				T* in = T::create(importer,intersectTechnique,cacheLocation,(intersectTechnique==IT_BSP_FAST)?".fast":((intersectTechnique==IT_BSP_FASTER)?".faster":".fastest"),(BuildParams*)buildParams);
				unsigned size1 = in->getMemoryOccupied();
				if (size1>=10000000)
					RRReporter::report(INF1,"Memory taken by collider(fast*): %dMB\n",size1/1024/1024);
				if (size1>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_VERIFICATION:
			{
				return IntersectVerification::create(importer);
			}
		case IT_LINEAR: 
		default:
		linear:
			RR_ASSERT(importer);
			if (!importer) return NULL;
			return IntersectLinear::create(importer);
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
