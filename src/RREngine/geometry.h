#ifndef _GEOMETRY_H
#define _GEOMETRY_H

#define S8           signed char
#define U8           unsigned char
#define S16          short
#define U16          unsigned short
#define S32          int
#define U32          unsigned
#define S64          __int64 //long long
#define U64          unsigned __int64 //unsigned long long
#define real         float
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
// angle in rad

typedef real Angle;

//////////////////////////////////////////////////////////////////////////////
//
// matrix in 3d

typedef float MATRIX[4][4];

#define _X_ 0
#define _Y_ 1
#define _Z_ 2

void matrix_Invert(MATRIX s, MATRIX d);
void matrix_Mul(MATRIX a, MATRIX b);
void matrix_Init(MATRIX a);
void matrix_Copy(MATRIX s, MATRIX d);
void matrix_Move(MATRIX m, float dx, float dy, float dz);
void matrix_Rotate(MATRIX m, float a, int axis);

//////////////////////////////////////////////////////////////////////////////
//
// 2d vector

struct Vec2
{
	real    x;
	real    y;

	Vec2()                   {}
	Vec2(real ax,real ay)    {x=ax;y=ay;}
	Vec2 operator +(Vec2 a)  {return Vec2(x+a.x,y+a.y);}
	Vec2 operator -(Vec2 a)  {return Vec2(x-a.x,y-a.y);}
	Vec2 operator *(real f)  {return Vec2(x*f,y*f);}
	Vec2 operator /(real f)  {return Vec2(x/f,y/f);}
	Vec2 operator +=(Vec2 a) {x+=a.x;y+=a.y;return *this;}
	Vec2 operator -=(Vec2 a) {x-=a.x;y-=a.y;return *this;}
	Vec2 operator *=(real f) {x*=f;y*=f;return *this;}
	Vec2 operator /=(real f) {x/=f;y/=f;return *this;}
	bool operator ==(Vec2 a) {return a.x==x && a.y==y;}
};

Vec2 operator -(Vec2 a);
real size(Vec2 a);
real sizeSquare(Vec2 a);
Vec2 normalized(Vec2 a);
real scalarMul(Vec2 a,Vec2 b);
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

	Vec3()                        {}
	Vec3(real ax,real ay,real az) {x=ax;y=ay;z=az;}
	Vec3 operator +(Vec3 a)       {return Vec3(x+a.x,y+a.y,z+a.z);}
	Vec3 operator -(Vec3 a)       {return Vec3(x-a.x,y-a.y,z-a.z);}
	Vec3 operator *(real f)       {return Vec3(x*f,y*f,z*f);}
	Vec3 operator /(real f)       {return Vec3(x/f,y/f,z/f);}
	Vec3 operator /(int f)        {return Vec3(x/f,y/f,z/f);}
	Vec3 operator /(unsigned f)   {return Vec3(x/f,y/f,z/f);}
	Vec3 operator +=(Vec3 a)      {x+=a.x;y+=a.y;z+=a.z;return *this;}
	Vec3 operator -=(Vec3 a)      {x-=a.x;y-=a.y;z-=a.z;return *this;}
	Vec3 operator *=(real f)      {x*=f;y*=f;z*=f;return *this;}
	Vec3 operator /=(real f)      {x/=f;y/=f;z/=f;return *this;}
	bool operator ==(Vec3 a)      {return a.x==x && a.y==y && a.z==z;}
	Vec3 transformed(MATRIX *m);
	Vec3 transform(MATRIX *m);
	real operator [](int i)       {return ((real*)this)[i];}
};

Vec3 operator -(Vec3 a);
real size(Vec3 a);
real sizeSquare(Vec3 a);
Vec3 normalized(Vec3 a);
real scalarMul(Vec3 a,Vec3 b);
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
// vertex in 3d

struct Vertex : public Point3
{
	int     id;
	int     t;
	real    u;
	real    v;
	real    tx;
	real    ty;
	real    tz;
	real    sx;
	real    sy;

	void operator =(Point3 a)            {x=a.x;y=a.y;z=a.z;}
	void    transformToCache(MATRIX *m);
	Point3  transformedFromCache()       {return Point3(tx,ty,tz);}
};

//////////////////////////////////////////////////////////////////////////////
//
// bounding object (sphere)

struct Bound
{
	Point3  centerBeforeTransformation;
	Point3  center;
	real    radius;
	real    radiusSquare;
	void    detect(Vertex *vertex,unsigned vertices);
	bool    intersect(Point3 eye,Vec3 direction,real maxDistance);
	bool    visible(MATRIX *camera);
};


#endif
