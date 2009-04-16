// --------------------------------------------------------------------------
// Trivial math for local use.
// Copyright (c) 2000-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include <memory.h>
#include "geometry_v.h"
#include "Lightsprint/RRMath.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// 2d vector

Vec2 operator -(Vec2 a)
{
	return Vec2(-a.x,-a.y);
}

real size(Vec2 a)
{
	return sqrt(a.x*a.x+a.y*a.y);
}

real size2(Vec2 a)
{
	return a.x*a.x+a.y*a.y;
}

Vec2 normalized(Vec2 a)
{
	return a/size(a);
}

real dot(Vec2 a,Vec2 b)
{
	return a.x*b.x+a.y*b.y;
}

Vec2 orthogonalToRight(Vec2 a)
{
	return Vec2(a.y,-a.x);
}

Vec2 orthogonalToLeft(Vec2 a)
{
	return Vec2(-a.y,a.x);
}

Angle angleBetweenNormalized(Vec2 a,Vec2 b)
{
	RR_ASSERT(fabs(size(a)-1)<0.001);
	RR_ASSERT(fabs(size(b)-1)<0.001);
	real r=size2(a-b);
	return fast_acos(1-r/2);
}

Angle angleBetween(Vec2 a,Vec2 b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

RRReal abs(RRReal a)
{
	return fabs(a);
}

RRReal sum(RRReal a)
{
	return a;
}

RRReal avg(RRReal a)
{
	return a;
}

void clampToZero(RRReal& a)
{
	if (a<0) a=0;
}

RRReal angleBetweenNormalized(const RRVec3& a,const RRVec3& b)
{
	RRReal d = dot(a,b);
	RRReal angle = fast_acos(RR_MAX(RR_MIN(d,1),-1));
	RR_ASSERT(IS_NUMBER(angle));
	return angle;
}

RRReal angleBetween(const RRVec3& a,const RRVec3& b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}


} // namespace
