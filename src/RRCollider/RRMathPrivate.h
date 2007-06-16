#ifndef RRMATHPRIVATE_H
#define RRMATHPRIVATE_H

#include "Lightsprint/RRMath.h"

#include <float.h> // _finite
#include <math.h>  // sqrt

namespace rr
{
	#define MAX(a,b) (((a)>(b))?(a):(b))
	#define MIN(a,b) (((a)<(b))?(a):(b))
	#define MAX3(a,b,c) MAX(a,MAX(b,c))
	#define MIN3(a,b,c) MIN(a,MIN(b,c))

	#define ABS(A)       fabs(A)
	#define IS_NUMBER(n) _finite(n)
	#define IS_VEC2(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]))
	#define IS_VEC3(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]))
	#define IS_VEC4(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]) && IS_NUMBER(v[3]))

	inline RRVec3 operator -(const RRVec3& a)
	{
		return RRVec3(-a.x,-a.y,-a.z);
	}

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

	inline RRVec3 ortogonalTo(const RRVec3& a)
	{
		if(!a.x)
			return RRVec3(0,a.z,-a.y);
		else
			return RRVec3(-a.y,a.x,0);
	}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

	inline RRVec3 ortogonalTo(const RRVec3& a,const RRVec3& b)
	{
		return RRVec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
	}


} // namespace

#endif
