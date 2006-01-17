#include "RRCollider.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include "IntersectVerification.h"

#include <assert.h>
#include <memory.h>
#include <windows.h> // jen kvuli GetTempPath


namespace rrCollider
{



RRVec3 operator -(const RRVec3& a)
{
	return RRVec3(-a.x,-a.y,-a.z);
}

RRReal size(const RRVec3& a)
{
	return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
}

RRReal size2(const RRVec3& a)
{
	return a.x*a.x+a.y*a.y+a.z*a.z;
}

RRReal abs(RRReal a)
{
	return fabs(a);
}

RRVec3 abs(const RRVec3& a)
{
	return RRVec3(fabs(a.x),fabs(a.y),fabs(a.z));
}

RRReal sum(RRReal a)
{
	return a;
}

RRReal sum(const RRVec3& a)
{
	return a.x+a.y+a.z;
}

RRReal avg(RRReal a)
{
	return a;
}

RRReal avg(const RRVec3& a)
{
	return (a.x+a.y+a.z)/3;
}

void clampToZero(RRReal& a)
{
	if(a<0) a=0;
}

void clampToZero(RRVec3& a)
{
	if(a.x<0) a.x=0;
	if(a.y<0) a.y=0;
	if(a.z<0) a.z=0;
}

RRVec3 normalized(const RRVec3& a)
{
	return a/size(a);
}

RRReal dot(const RRVec3& a,const RRVec3& b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

RRVec3 ortogonalTo(const RRVec3& a)
{
	return RRVec3(0,a.z,-a.y);
}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

RRVec3 ortogonalTo(const RRVec3& a,const RRVec3& b)
{
	return RRVec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

RRReal angleBetweenNormalized(const RRVec3& a,const RRVec3& b)
{
	RRReal d = dot(a,b);
	RRReal angle = acos(MAX(MIN(d,1),-1));
	assert(IS_NUMBER(angle));
	return angle;
}

RRReal angleBetween(const RRVec3& a,const RRVec3& b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}






RRCollider* RRCollider::create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, const char* cacheLocation, void* buildParams)
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
		case IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(importer,intersectTechnique,cacheLocation,(intersectTechnique==IT_BSP_FAST)?".fast":".fastest",(BuildParams*)buildParams);
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

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	memset(this,0,sizeof(*this));
}

RRIntersectStats* RRIntersectStats::getInstance()
{
	return &intersectStats;
}

void RRIntersectStats::getInfo(char *buf, unsigned len, unsigned level) const
{
	buf[0]=0;
	len--;
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf),"Intersect stats:\n");
	if(intersects>100) {
	if(level>=1) _snprintf(buf+strlen(buf),len-strlen(buf)," rays=%d missed=%d(%d)\n",intersects,intersects-hits,(intersects-hits)/(intersects/100));
	if(level>=1 && (intersect_bspSRLNP || intersect_triangleSRLNP)) _snprintf(buf+strlen(buf),len-strlen(buf)," bspSRLNP=%d(%d) triSRLNP=%d(%d)\n",intersect_bspSRLNP,intersect_bspSRLNP/intersects,intersect_triangleSRLNP,intersect_triangleSRLNP/intersects);
	if(level>=1 && (intersect_bspNP    || intersect_triangleNP   )) _snprintf(buf+strlen(buf),len-strlen(buf)," bspNP=%d(%d) triNP=%d(%d)\n",intersect_bspNP,intersect_bspNP/intersects,intersect_bspNP,intersect_bspNP/intersects);
	}
	if(invalid_triangles) _snprintf(buf+strlen(buf),len-strlen(buf)," invalid_triangles=%d/%d\n",invalid_triangles,loaded_triangles);
	if(intersect_linear) _snprintf(buf+strlen(buf),len-strlen(buf)," intersect_linear=%d\n",intersect_linear);
	buf[len]=0;
}


//////////////////////////////////////////////////////////////////////////////
//
// License


void registerLicense(char* licenseOwner, char* licenseNumber)
{
}

} //namespace
