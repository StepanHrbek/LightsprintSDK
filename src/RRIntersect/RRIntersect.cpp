#include "RRIntersect.h"
#include "IntersectBspCompact.h"
#include "IntersectBspFast.h"
#include <math.h>
#include <memory.h>
#include <new> // aligned new
#include <stdio.h>
#include <string.h>


namespace rrIntersect
{

#ifdef USE_SSE
void* AlignedMalloc(size_t size,int byteAlign)
{
	void *mallocPtr = malloc(size + byteAlign + sizeof(void*));
	size_t ptrInt = (size_t)mallocPtr;

	ptrInt = (ptrInt + byteAlign + sizeof(void*)) / byteAlign * byteAlign;
	*(((void**)ptrInt) - 1) = mallocPtr;

	return (void*)ptrInt;
}

void AlignedFree(void *ptr)
{
	free(*(((void**)ptr) - 1));
}
#endif

RRAligned::RRAligned()
{
/* u trid s necim virtualnim je this%16==4
#ifdef USE_SSE
	if(intptr_t(this)%16) 
	{
		printf("You have created unaligned structure, aborting. Try static or heap if it's on stack now.");
		assert(!(intptr_t(this)%16));
		exit(1);
	}
#endif*/
};

void* RRAligned::operator new(std::size_t n)
{
#ifdef USE_SSE
	return AlignedMalloc(n,16);
#else
	return ::operator new(n);
#endif
};

void RRAligned::operator delete(void* p, std::size_t n)
{
#ifdef USE_SSE
	AlignedFree(p);
#else
	::delete(p);
#endif
};

RRRay::RRRay()
{
	memset(this,0,sizeof(RRRay)); // no virtuals in RRRay -> no pointer to virtual function table overwritten
	flags = FILL_DISTANCE | FILL_POINT3D | FILL_POINT2D | FILL_PLANE | FILL_TRIANGLE | FILL_SIDE;
}

RRRay* RRRay::create()
{
	return new RRRay();
}

void RRMeshImporter::getTriangleSRL(unsigned i, TriangleSRL* t) const
{
	unsigned v0,v1,v2;
	getTriangle(i,v0,v1,v2);
	real* v[3];
	v[0] = getVertex(v0);
	v[1] = getVertex(v1);
	v[2] = getVertex(v2);
	t->s[0]=v[0][0];
	t->s[1]=v[0][1];
	t->s[2]=v[0][2];
	t->r[0]=v[1][0]-v[0][0];
	t->r[1]=v[1][1]-v[0][1];
	t->r[2]=v[1][2]-v[0][2];
	t->l[0]=v[2][0]-v[0][0];
	t->l[1]=v[2][1]-v[0][1];
	t->l[2]=v[2][2]-v[0][2];
}

RRCollider* RRCollider::create(RRMeshImporter* importer, IntersectTechnique intersectTechnique, void* buildParams)
{
	if(!importer) return NULL;
	BuildParams bp(intersectTechnique);
	if(!buildParams || ((BuildParams*)buildParams)->size<sizeof(BuildParams)) buildParams = &bp;
	switch(intersectTechnique)
	{
		// needs explicit instantiation at the end of IntersectBspFast.cpp and IntersectBspCompact.cpp and bsp.cpp
		case IT_BSP_COMPACT:
			if(importer->getNumTriangles()<=256)
			{
				typedef IntersectBspCompact<CBspTree21> T;
				T* in = T::create(importer,intersectTechnique,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			if(importer->getNumTriangles()<=65536)
			{
				typedef IntersectBspCompact<CBspTree42> T;
				T* in = T::create(importer,intersectTechnique,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
			{
				typedef IntersectBspCompact<CBspTree44> T;
				T* in = T::create(importer,intersectTechnique,".compact",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
			}
		case IT_BSP_FASTEST:
		case IT_BSP_FAST:
			{
				typedef IntersectBspFast<BspTree44> T;
				T* in = T::create(importer,intersectTechnique,(intersectTechnique==IT_BSP_FAST)?".fast":".fastest",(BuildParams*)buildParams);
				if(in->getMemoryOccupied()>sizeof(T)) return in;
				delete in;
				goto linear;
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

} //namespace
