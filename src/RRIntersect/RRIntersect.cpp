#include "RRIntersect.h"
#include "IntersectLinear.h"
#include "IntersectBsp.h"
#include "IntersectKd.h"

RRIntersect* newIntersect(RRObjectImporter* importer)
{
#ifdef USE_KD
	return new IntersectKd(importer);
#else
#ifdef USE_BSP
	return new IntersectBsp(importer);
#else
	return new IntersectLinear(importer);
#endif
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// RRIntersectStats - statistics for library calls

RRIntersectStats intersectStats;

RRIntersectStats::RRIntersectStats() 
{
	//!!!memset(this,0,sizeof(*this));
}

unsigned RRIntersectStats::shots;
unsigned RRIntersectStats::bsp;
unsigned RRIntersectStats::kd;
unsigned RRIntersectStats::tri;

