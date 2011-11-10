// --------------------------------------------------------------------------
// Ray for ray-mesh intersections.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRCollider.h"
#include "config.h" // USE_SSE

#include <cstring>

namespace rr
{

RRRay::RRRay()
{
	memset(this,0,sizeof(RRRay)); // no virtuals in RRRay -> no pointer to virtual function table overwritten
	//memset(((char*)this)+4,0,sizeof(RRRay)-4); // virtuals in RRRay -> avoid overwriting pointer to virtual function table
	rayOrigin[3] = 1;
	rayFlags = FILL_DISTANCE | FILL_POINT3D | FILL_POINT2D | FILL_PLANE | FILL_TRIANGLE | FILL_SIDE;
}

RRRay* RRRay::create()
{
	return new RRRay();
}

RRRay* RRRay::create(unsigned n)
{
#ifdef USE_SSE
	RR_ASSERT(!(sizeof(RRRay)%16));
#endif
	return new RRRay[n]();
}

} //namespace
