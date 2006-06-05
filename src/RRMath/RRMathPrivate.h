#ifndef COLLIDER_RRMATH_H
#define COLLIDER_RRMATH_H

#include "RRMath.h"

#include <float.h> // _finite

namespace rr
{
	#define MAX(a,b) (((a)>(b))?(a):(b))
	#define MIN(a,b) (((a)<(b))?(a):(b))
	#define MAX3(a,b,c) MAX(a,MAX(b,c))
	#define MIN3(a,b,c) MIN(a,MIN(b,c))

	#define ABS(A)       fabs(A) //((A)>0?(A):-(A)) ReDoxovi pomaha toto, u me je rychlejsi fabs
	#define IS_NUMBER(n) _finite(n)//((n)>-BIG_REAL && (n)<BIG_REAL)
	#define IS_VEC2(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]))
	#define IS_VEC3(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]))
	#define IS_VEC4(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]) && IS_NUMBER(v[3]))

	RRVec3 operator -(const RRVec3& a);
	RRReal size(const RRVec3& a);
	RRReal size2(const RRVec3& a);
	RRVec3 normalized(const RRVec3& a);
	RRReal dot(const RRVec3& a,const RRVec3& b);
	RRVec3 ortogonalTo(const RRVec3& a);
	RRVec3 ortogonalTo(const RRVec3& a,const RRVec3& b);

} // namespace

#endif
