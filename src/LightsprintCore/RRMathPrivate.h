// --------------------------------------------------------------------------
// Math for local use.
// Copyright (c) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRMATHPRIVATE_H
#define RRMATHPRIVATE_H

#include "Lightsprint/RRMath.h"

#include <float.h> // _finite
#include <math.h>  // sqrt

namespace rr
{
	#define ABS(A)       fabs(A)
	#define IS_NUMBER(n) _finite(n)
	#define IS_VEC2(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]))
	#define IS_VEC3(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]))
	#define IS_VEC4(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]) && IS_NUMBER(v[3]))
	#define IS_NORMALIZED(v) (fabs((v).length2()-1)<0.01f)

	inline RRReal size(const RRVec3& a)
	{
		return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
	}

	inline RRReal size2(const RRVec3& a)
	{
		return a.x*a.x+a.y*a.y+a.z*a.z;
	}

	inline RRVec3 normalized(const RRVec3& a)
	{
		return a/size(a);
	}

	inline RRReal dot(const RRVec3& a,const RRVec3& b)
	{
		return a.x*b.x+a.y*b.y+a.z*b.z;
	}

	// doesn't preserve length (would be slower)
	// is not continuous (such function doesn't exist)
	inline RRVec3 orthogonalTo(const RRVec3& a)
	{
		//if (!a.x) return RRVec3(0,a.z,-a.y); else return RRVec3(-a.y,a.x,0); // plane of discontinuity
		if (!a.x && !a.y) return RRVec3(0,1,0); else return RRVec3(-a.y,a.x,0); // line of discontinuity
	}

	inline RRVec3 orthogonalTo(const RRVec3& a,const RRVec3& b)
	{
		return RRVec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
	}


} // namespace

#endif
