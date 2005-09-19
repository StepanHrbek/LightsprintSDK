#ifndef COLLIDER_GEOMETRY_H
#define COLLIDER_GEOMETRY_H

#include "RRCollider.h"
#include "config.h"
#include <float.h> // _finite

#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

namespace rrCollider
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

	//////////////////////////////////////////////////////////////////////////////
	//
	// 3d vector

	struct Vec3
	{
		real    x;
		real    y;
		real    z;

		Vec3()                                {}
		Vec3(real ax,real ay,real az)         {x=ax;y=ay;z=az;}
		Vec3 operator + (const Vec3& a) const {return Vec3(x+a.x,y+a.y,z+a.z);}
		Vec3 operator - (const Vec3& a) const {return Vec3(x-a.x,y-a.y,z-a.z);}
		Vec3 operator * (real f)        const {return Vec3(x*f,y*f,z*f);}
		Vec3 operator * (Vec3 a)        const {return Vec3(x*a.x,y*a.y,z*a.z);}
		Vec3 operator / (real f)        const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator / (int f)         const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator / (unsigned f)    const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator +=(const Vec3& a)       {x+=a.x;y+=a.y;z+=a.z;return *this;}
		Vec3 operator -=(const Vec3& a)       {x-=a.x;y-=a.y;z-=a.z;return *this;}
		Vec3 operator *=(real f)              {x*=f;y*=f;z*=f;return *this;}
		Vec3 operator /=(real f)              {x/=f;y/=f;z/=f;return *this;}
		bool operator ==(const Vec3& a) const {return a.x==x && a.y==y && a.z==z;}
		real operator [](int i)         const {return ((real*)this)[i];}
	};

	Vec3 operator -(const Vec3& a);
	real size(const Vec3& a);
	real size2(Vec3 a);
	Vec3 normalized(Vec3 a);
	real dot(const Vec3& a,const Vec3& b);
	Vec3 ortogonalTo(const Vec3& a,const Vec3& b);

	//////////////////////////////////////////////////////////////////////////////
	//
	// plane in 3d

	struct Plane : public Vec3
	{
		real    d;

		void operator =(const Vec3 a)   {x=a.x;y=a.y;z=a.z;}
	};

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
