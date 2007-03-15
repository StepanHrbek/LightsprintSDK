#include "RRCollider.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"

#include <assert.h>
#include <memory.h>
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <windows.h> // jen kvuli GetTempPath


namespace rr
{


RRCollider* RRCollider::create(RRMesh* importer, IntersectTechnique intersectTechnique, const char* cacheLocation, void* buildParams)
{
	if(!importer) return NULL;
	BuildParams bp(intersectTechnique);
	if(!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	char tmpPath[_MAX_PATH+1];
	if(!cacheLocation)
	{
		GetTempPath(_MAX_PATH, tmpPath);
		#define IS_PATHSEP(x) (((x) == '\\') || ((x) == '/'))
		if(!IS_PATHSEP(tmpPath[strlen(tmpPath)-1])) strcat(tmpPath, "\\");
		cacheLocation = tmpPath;
	}
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case IT_BSP_COMPACT:
			if(importer->getNumTriangles()<=256)
			{
				typedef IntersectBspCompact<CBspTree21> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			if(importer->getNumTriangles()<=65536)
			{
				typedef IntersectBspCompact<CBspTree42> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			{
				typedef IntersectBspCompact<CBspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_BSP_FASTEST:
		case IT_BSP_FASTER:
		case IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,(intersectTechnique==IT_BSP_FAST)?".fast":((intersectTechnique==IT_BSP_FASTER)?".faster":".fastest"),(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
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
			assert(importer);
			if(!importer) return NULL;
			return IntersectLinear::create(importer);
	}
}

void RRCollider::intersectBatch(RRRay* ray, unsigned numRays)
{
	#pragma omp parallel for schedule(static,1)
	for(int i=0;i<(int)numRays;i++)
	{
		if(!intersect(ray+i)) ray[i].hitDistance = -1;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls



//////////////////////////////////////////////////////////////////////////////
//
// License


static void registerLicense(const char* licenseOwner, const char* licenseNumber)
{
}

void RRLicenseCollider::loadLicense(const char* filename)
{
}

} //namespace
