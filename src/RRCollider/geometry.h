#ifndef COLLIDER_GEOMETRY_H
#define COLLIDER_GEOMETRY_H

#include "RRCollider.h"
#include "RRMath.h"
#include "config.h"
#include <float.h> // _finite

#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

namespace rr
{
	typedef RRReal real;

	#define MAX(a,b) (((a)>(b))?(a):(b))
	#define MIN(a,b) (((a)<(b))?(a):(b))
	#define MAX3(a,b,c) MAX(a,MAX(b,c))
	#define MIN3(a,b,c) MIN(a,MIN(b,c))

	#define ABS(A)       fabs(A) //((A)>0?(A):-(A)) ReDoxovi pomaha toto, u me je rychlejsi fabs
	#define IS_NUMBER(n) _finite(n)//((n)>-BIG_REAL && (n)<BIG_REAL)
	#define IS_VEC2(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]))
	#define IS_VEC3(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]))
	#define IS_VEC4(v)   (IS_NUMBER(v[0]) && IS_NUMBER(v[1]) && IS_NUMBER(v[2]) && IS_NUMBER(v[3]))

	//////////////////////////////////////////////////////////////////////////////
	//
	// 3d vector

	typedef RRVec3 Vec3;

	//////////////////////////////////////////////////////////////////////////////
	//
	// plane in 3d

	typedef RRVec4 Plane;

	real normalValueIn(const Plane& n,const Vec3& a);

	//////////////////////////////////////////////////////////////////////////////
	//
	// sphere in 3d

#ifdef USE_SPHERE
	struct Sphere
	{
		Vec3    center;
		real    radius;
		real    radius2;
		void    detect(const Vec3 *vertex,unsigned vertices);
		bool    intersect(RRRay* ray) const;
	};
#endif

	//////////////////////////////////////////////////////////////////////////////
	//
	// box in 3d

	struct Box
	{
		Vec3    min;
		real    pad;
		Vec3    max;
		void    detect(const Vec3 *vertex,unsigned vertices);
		bool    intersect(RRRay* ray) const;
		bool    intersectFast(RRRay* ray) const;
	};
}

#endif
