#ifndef RRENGINE_GEOMETRY_H
#define RRENGINE_GEOMETRY_H

#include "RREngine.h" // RRReal
#include <float.h> // _finite

namespace rrEngine
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
#define ABS(A)       fabs(A) //((A)>0?(A):-(A)) ReDoxovi pomaha toto, u me je rychlejsi fabs
#define IS_NUMBER(n) _finite(n)//((n)>-BIG_REAL && (n)<BIG_REAL)
#define IS_0(n)      (ABS(n)<0.001)
#define IS_1(n)      (fabs(n-1)<0.001)
#define IS_EQ(a,b)   (fabs(a-b)<0.001)
#define IS_VEC2(v)   (IS_NUMBER(v.x) && IS_NUMBER(v.y))
#define IS_VEC3(v)   (IS_NUMBER((v).x) && IS_NUMBER((v).y) && IS_NUMBER((v).z))
#define IS_SIZE1(v)  IS_1(size2(v))
#define IS_SIZE0(v)  IS_0(size2(v))

#if CHANNELS == 1
 #define IS_CHANNELS(n) IS_NUMBER(n)
#else
 #define IS_CHANNELS(n) (IS_NUMBER(n.x) && IS_NUMBER(n.y) && IS_NUMBER(n.z))
#endif

#ifndef MAX
 #define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
 #define MIN(a,b) ((a)<(b)?(a):(b))
#endif

real abs(real a);
real sum(real a);
real avg(real a);
void clampToZero(real& a);

//////////////////////////////////////////////////////////////////////////////
//
// angle in rad

typedef real Angle;

//////////////////////////////////////////////////////////////////////////////
//
// matrix 4x4

typedef real Matrix[4][4];

//////////////////////////////////////////////////////////////////////////////
//
// 2d vector

struct Vec2
{
	real    x;
	real    y;

	Vec2()                         {}
	Vec2(real ax,real ay)          {x=ax;y=ay;}
	Vec2 operator + (Vec2 a) const {return Vec2(x+a.x,y+a.y);}
	Vec2 operator - (Vec2 a) const {return Vec2(x-a.x,y-a.y);}
	Vec2 operator * (real f) const {return Vec2(x*f,y*f);}
	Vec2 operator / (real f) const {return Vec2(x/f,y/f);}
	Vec2 operator +=(Vec2 a)       {x+=a.x;y+=a.y;return *this;}
	Vec2 operator -=(Vec2 a)       {x-=a.x;y-=a.y;return *this;}
	Vec2 operator *=(real f)       {x*=f;y*=f;return *this;}
	Vec2 operator /=(real f)       {x/=f;y/=f;return *this;}
	bool operator ==(Vec2 a) const {return a.x==x && a.y==y;}
};

Vec2 operator -(Vec2 a);
real size(Vec2 a);
real size2(Vec2 a);
Vec2 normalized(Vec2 a);
real dot(Vec2 a,Vec2 b);
Vec2 ortogonalToRight(Vec2 a);
Vec2 ortogonalToLeft(Vec2 a);
Angle angleBetween(Vec2 a,Vec2 b);
Angle angleBetweenNormalized(Vec2 a,Vec2 b);

//////////////////////////////////////////////////////////////////////////////
//
// 2d point

#define Point2 Vec2

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

struct Vec3
{
	real    x;
	real    y;
	real    z;

	Vec3()                            {}
	explicit Vec3(real a)             {x=a;y=a;z=a;}
	Vec3(real ax,real ay,real az)     {x=ax;y=ay;z=az;}
	Vec3 operator +(Vec3 a)     const {return Vec3(x+a.x,y+a.y,z+a.z);}
	Vec3 operator -(Vec3 a)     const {return Vec3(x-a.x,y-a.y,z-a.z);}
	Vec3 operator *(real f)     const {return Vec3(x*f,y*f,z*f);}
	Vec3 operator *(Vec3 a)     const {return Vec3(x*a.x,y*a.y,z*a.z);}
	Vec3 operator /(real f)     const {return Vec3(x/f,y/f,z/f);}
	Vec3 operator /(Vec3 a)     const {return Vec3(x/a.x,y/a.y,z/a.z);}
	Vec3 operator /(int f)      const {return Vec3(x/f,y/f,z/f);}
	Vec3 operator /(unsigned f) const {return Vec3(x/f,y/f,z/f);}
	Vec3 operator +=(Vec3 a)          {x+=a.x;y+=a.y;z+=a.z;return *this;}
	Vec3 operator -=(Vec3 a)          {x-=a.x;y-=a.y;z-=a.z;return *this;}
	Vec3 operator *=(real f)          {x*=f;y*=f;z*=f;return *this;}
	Vec3 operator *=(Vec3 a)          {x*=a.x;y*=a.y;z*=a.z;return *this;}
	Vec3 operator /=(real f)          {x/=f;y/=f;z/=f;return *this;}
	bool operator ==(Vec3 a)    const {return a.x==x && a.y==y && a.z==z;}
	bool operator !=(Vec3 a)    const {return a.x!=x || a.y!=y || a.z!=z;}
	Vec3 transformed(const Matrix *m) const;
	Vec3 transform(const Matrix *m);
	Vec3 rotated(const Matrix *m) const;
	Vec3 rotate(const Matrix *m);
	real operator [](int i)     const {return ((real*)this)[i];}
};

Vec3 operator -(Vec3 a);
real size(Vec3 a);
real size2(Vec3 a);
Vec3 abs(Vec3 a);
real sum(Vec3 a);
real avg(Vec3 a);
void clampToZero(Vec3& a);
Vec3 normalized(Vec3 a);
real dot(Vec3 a,Vec3 b);
Vec3 ortogonalTo(Vec3 a);
Vec3 ortogonalTo(Vec3 a,Vec3 b);
Angle angleBetween(Vec3 a,Vec3 b);
Angle angleBetweenNormalized(Vec3 a,Vec3 b);

//////////////////////////////////////////////////////////////////////////////
//
// 3d point

#define Point3 Vec3

//////////////////////////////////////////////////////////////////////////////
//
// normal in 3d

struct Normal : public Vec3
{
	real    d;

	void operator =(Vec3 a)   {x=a.x;y=a.y;z=a.z;}
};

real normalValueIn(Normal n,Point3 a);

//////////////////////////////////////////////////////////////////////////////
//
// bounding object (sphere)

struct Bound
{
	Point3  centerBeforeTransformation;
	Point3  center;
	real    radius;
	real    radius2;
	void    detect(const Vec3 *vertex,unsigned vertices);
	bool    intersect(Point3 eye,Vec3 direction,real maxDistance);
	bool    visible(const Matrix *camera);
};

} // namespace

#endif
