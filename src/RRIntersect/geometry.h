#ifndef RRINTERSECT_GEOMETRY_H
#define RRINTERSECT_GEOMETRY_H

#include "RRIntersect.h"

namespace rrIntersect
{
	#define S8           signed char //__int8
	#define U8           unsigned char //unsigned __int8
	#define S16          __int16
	#define U16          unsigned __int16
	#define S32          __int32
	#define U32          unsigned __int32
	#define S64          __int64
	#define U64          unsigned __int64
	#define real         RRreal
	#define BIG_REAL     1e20
	#define SMALL_REAL   1e-10
	#define ABS(A)       fabs(A) //((A)>0?(A):-(A)) ReDoxovi pomaha toto, u me je rychlejsi fabs
	#define IS_NUMBER(n) ((n)>-BIG_REAL && (n)<BIG_REAL)
	#define IS_0(n)      (ABS(n)<0.001)
	#define IS_1(n)      (fabs(n-1)<0.001)
	#define IS_EQ(a,b)   (fabs(a-b)<0.001)
	#define IS_VEC2(v)   (IS_NUMBER(v.x) && IS_NUMBER(v.y))
	#define IS_VEC3(v)   (IS_NUMBER(v.x) && IS_NUMBER(v.y) && IS_NUMBER(v.z))
	#define IS_SIZE1(v)  IS_1(sizeSquare(v))
	#define IS_SIZE0(v)  IS_0(sizeSquare(v))

	//////////////////////////////////////////////////////////////////////////////
	//
	// 3d vector

	struct Vec3
	{
		real    x;
		real    y;
		real    z;

		Vec3()                               {}
		Vec3(real ax,real ay,real az)        {x=ax;y=ay;z=az;}
		Vec3 operator + (const Vec3 a) const {return Vec3(x+a.x,y+a.y,z+a.z);}
		Vec3 operator - (const Vec3 a) const {return Vec3(x-a.x,y-a.y,z-a.z);}
		Vec3 operator * (real f)       const {return Vec3(x*f,y*f,z*f);}
		Vec3 operator / (real f)       const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator / (int f)        const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator / (unsigned f)   const {return Vec3(x/f,y/f,z/f);}
		Vec3 operator +=(const Vec3 a)       {x+=a.x;y+=a.y;z+=a.z;return *this;}
		Vec3 operator -=(const Vec3 a)       {x-=a.x;y-=a.y;z-=a.z;return *this;}
		Vec3 operator *=(real f)             {x*=f;y*=f;z*=f;return *this;}
		Vec3 operator /=(real f)             {x/=f;y/=f;z/=f;return *this;}
		bool operator ==(const Vec3 a) const {return a.x==x && a.y==y && a.z==z;}
		real operator [](int i)        const {return ((real*)this)[i];}
	};

	Vec3 operator -(Vec3 a);
	real size(Vec3 a);
	real sizeSquare(Vec3 a);
	Vec3 normalized(Vec3 a);
	real scalarMul(Vec3 a,Vec3 b);
	Vec3 ortogonalTo(Vec3 a,Vec3 b);

	//////////////////////////////////////////////////////////////////////////////
	//
	// normal in 3d

	struct Normal : public Vec3
	{
		real    d;

		void operator =(const Vec3 a)   {x=a.x;y=a.y;z=a.z;}
	};

	real normalValueIn(const Normal n,const Vec3 a);
}

#endif
