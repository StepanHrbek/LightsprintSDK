// --------------------------------------------------------------------------
// Trivial math for local use.
// Copyright (c) 2000-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRVISION_GEOMETRY_H
#define RRVISION_GEOMETRY_H

#include "Lightsprint/RRMath.h" // RRReal
#include "../RRMathPrivate.h" // vec3 support in rrCollider
#include <float.h> // _finite

namespace rr
{

#define S8           signed char
#define U8           unsigned char
#define S16          short
#define U16          unsigned short
#define S32          int
#define U32          unsigned
#define S64          __int64 //long long
#define U64          unsigned __int64 //unsigned long long
#define real         RRReal
#define BIG_REAL     1e20f
#define SMALL_REAL   1e-10f
#define SHOT_OFFSET  1e-4f  //!!! offset 0.1mm resi situaci kdy jsou 2 facy ve stejne poloze, jen obracene zady k sobe. bez offsetu se vzajemne zasahuji.
#define ABS(A)       fabs(A)
#define IS_NUMBER(n) _finite(n)
#define IS_PTR(p)    ((long long)(p)>0x10000)
#define IS_0(n)      (ABS(n)<0.001)
#define IS_1(n)      (fabs(n-1)<0.001)
#define IS_EQ(a,b)   (fabs(a-b)<0.001)
#define IS_VEC2(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]))
#define IS_VEC3(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]))
#define IS_SIZE1(v)  IS_1(size2(v))
#define IS_SIZE0(v)  IS_0(size2(v))

#if CHANNELS == 1
 #define IS_CHANNELS(n) IS_NUMBER(n)
#else
 #define IS_CHANNELS(n) (IS_NUMBER(n.x) && IS_NUMBER(n.y) && IS_NUMBER(n.z))
#endif

//////////////////////////////////////////////////////////////////////////////
//
// angle in rad

typedef real Angle;

//////////////////////////////////////////////////////////////////////////////
//
// 2d vector

typedef RRVec2 Vec2;

Vec2 operator -(Vec2 a);
real size(Vec2 a);
real size2(Vec2 a);
Vec2 normalized(Vec2 a);
real dot(Vec2 a,Vec2 b);
Vec2 orthogonalToRight(Vec2 a);
Vec2 orthogonalToLeft(Vec2 a);
Angle angleBetween(Vec2 a,Vec2 b);
Angle angleBetweenNormalized(Vec2 a,Vec2 b);

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

typedef RRVec3 Vec3;

// pri zinlinovani i techto msvc o 5% zpomali
RRReal abs(RRReal a);
RRReal sum(RRReal a);
RRReal avg(RRReal a);
void   clampToZero(RRReal& a);

inline RRVec3 abs(const RRVec3& a)
{
	return RRVec3(fabs(a.x),fabs(a.y),fabs(a.z));
}
inline RRReal sum(const RRVec3& a)
{
	return a.x+a.y+a.z;
}
inline RRReal avg(const RRVec3& a)
{
	return (a.x+a.y+a.z)/3;
}
inline void clampToZero(RRVec3& a)
{
	if (a.x<0) a.x=0;
	if (a.y<0) a.y=0;
	if (a.z<0) a.z=0;
}

RRReal angleBetween(const RRVec3& a,const RRVec3& b);
RRReal angleBetweenNormalized(const RRVec3& a,const RRVec3& b);

//////////////////////////////////////////////////////////////////////////////
//
// normal in 3d

typedef RRVec3p Normal;

//////////////////////////////////////////////////////////////////////////////
//
// trigonometry

inline RRReal fast_acos(RRReal fValue)
{
#if 1
	RRReal fRoot = sqrtf(1.0f-fValue);
	RRReal fResult = -0.0187293f;
	fResult *= fValue;
	fResult += 0.0742610f;
	fResult *= fValue;
	fResult -= 0.2121144f;
	fResult *= fValue;
	fResult += 1.5707288f;
	fResult *= fRoot;
	return fResult; 
#else
	return acos(fValue);
#endif
}

} // namespace

#endif
