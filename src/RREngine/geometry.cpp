#include <assert.h>
#include <math.h>
#include <memory.h>
#include "geometry.h"

namespace rrEngine
{

//#define FAST_BOUND                // fast approximate bounding sphere intersection
//...naky intersecty pritom zahazuje, je treba proverit skodlivost

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

real sizeSquare(Vec2 a)
{
	return a.x*a.x+a.y*a.y;
}

Vec2 normalized(Vec2 a)
{
	return a/size(a);
}

real scalarMul(Vec2 a,Vec2 b)
{
	return a.x*b.x+a.y*b.y;
}

Vec2 ortogonalToRight(Vec2 a)
{
	return Vec2(a.y,-a.x);
}

Vec2 ortogonalToLeft(Vec2 a)
{
	return Vec2(-a.y,a.x);
}

Angle angleBetweenNormalized(Vec2 a,Vec2 b)
{
	assert(fabs(size(a)-1)<0.001);
	assert(fabs(size(b)-1)<0.001);
	real r=sizeSquare(a-b);
	return acos(1-r/2);
}

Angle angleBetween(Vec2 a,Vec2 b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

Vec3 Vec3::transformed(Matrix *m)
{
	return Vec3(
	  x*(*m)[0][0] + y*(*m)[1][0] + z*(*m)[2][0] + (*m)[3][0],
	  x*(*m)[0][1] + y*(*m)[1][1] + z*(*m)[2][1] + (*m)[3][1],
	  x*(*m)[0][2] + y*(*m)[1][2] + z*(*m)[2][2] + (*m)[3][2]);
}

Vec3 Vec3::transform(Matrix *m)
{
	real _x=x,_y=y,_z=z;

	x = _x*(*m)[0][0] + _y*(*m)[1][0] + _z*(*m)[2][0] + (*m)[3][0];
	y = _x*(*m)[0][1] + _y*(*m)[1][1] + _z*(*m)[2][1] + (*m)[3][1];
	z = _x*(*m)[0][2] + _y*(*m)[1][2] + _z*(*m)[2][2] + (*m)[3][2];

	return *this;
}

Vec3 operator -(Vec3 a)
{
	return Vec3(-a.x,-a.y,-a.z);
}

real size(Vec3 a)
{
	return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
}

real sizeSquare(Vec3 a)
{
	return a.x*a.x+a.y*a.y+a.z*a.z;
}

Vec3 normalized(Vec3 a)
{
	return a/size(a);
}

real scalarMul(Vec3 a,Vec3 b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

Vec3 ortogonalTo(Vec3 a)
{
	return Vec3(0,a.z,-a.y);
}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

Vec3 ortogonalTo(Vec3 a,Vec3 b)
{
	return Vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

Angle angleBetweenNormalized(Vec3 a,Vec3 b)
{
	real r=sizeSquare(a-b);
	return acos(1-r/2);
}

Angle angleBetween(Vec3 a,Vec3 b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}

//////////////////////////////////////////////////////////////////////////////
//
// normal in 3d

real normalValueIn(Normal n,Point3 a)
{
	return a.x*n.x+a.y*n.y+a.z*n.z+n.d;
}

//////////////////////////////////////////////////////////////////////////////
//
// bounding object (sphere)

void Bound::detect(Vec3 *vertex,unsigned vertices)
{
	Point3 sum=Point3(0,0,0);
	for(unsigned i=0;i<vertices;i++) sum+=vertex[i];
	center=sum/vertices;
	radiusSquare=0;
	for(unsigned i=0;i<vertices;i++) 
	{
		real tmp=sizeSquare(vertex[i]-center);
		if(tmp>radiusSquare) radiusSquare=tmp;
	}
	radius=sqrt(radiusSquare);
	//...najit presnejsi bound
	centerBeforeTransformation=center;
}

// does ray intersect sphere in distance < maxDistance?

bool Bound::intersect(Point3 eye,Vec3 direction,real maxDistance)
{
	Vec3 toCenter=center-eye;
	real distEyeCenterSquare=sizeSquare(toCenter);            //3*mul

	// eye inside sphere
	if(distEyeCenterSquare-radiusSquare<0) return true;

	real distEyeCenter=sqrt(distEyeCenterSquare);             //1*sqrt

	// sphere too far
	if(distEyeCenter-radius>=maxDistance) return false;

	assert(fabs(sizeSquare(direction)-1)<0.001);

#ifdef FAST_BOUND
	Point3 nearCenter=eye+direction*distEyeCenter;            //3*mul
	real distNrcntrCenterSquare=sizeSquare(nearCenter-center);//3*mul

	// probably no intersection
	if(distNrcntrCenterSquare>=radiusSquare) return false;
#else
	real psqr=sizeSquare(toCenter/distEyeCenter-direction);
	real cosa=1-psqr/2;
	real sinaSquare=psqr*(1-psqr/4);//=sin(acos(cosa));
	real distNrstCenterSquare=distEyeCenterSquare*sinaSquare;

	// no intersection
	if(distNrstCenterSquare>=radiusSquare) return false;

	real distNrstEye=distEyeCenter*cosa;
	real distNrstIntrsct=sqrt(radius*radius-distNrstCenterSquare);
	real distEyeIntrsct=distNrstEye-distNrstIntrsct;

	// intersection too far
	if(distEyeIntrsct>=maxDistance) return false;

	// intersection beyond eye
	if(sizeSquare(toCenter+direction*distEyeCenter)<sizeSquare(toCenter-direction*distEyeCenter)) return false;
#endif
	return true;
}

bool Bound::visible(Matrix *camera)
{
	//...
	return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// matrix in 3d

void matrix_Copy(Matrix s, Matrix d)
{
 memcpy(d,s,sizeof(Matrix));
}

void matrix_Invert(Matrix s, Matrix d)
{
 int i,j; float det;

 d[0][0] =   s[1][1] * s[2][2] - s[1][2] * s[2][1];
 d[0][1] = - s[0][1] * s[2][2] + s[0][2] * s[2][1];
 d[0][2] =   s[0][1] * s[1][2] - s[0][2] * s[1][1];

 det = d[0][0] * s[0][0] + d[0][1] * s[1][0] + d[0][2] * s[2][0];

 d[1][0] = - s[1][0] * s[2][2] + s[1][2] * s[2][0];
 d[1][1] =   s[0][0] * s[2][2] - s[0][2] * s[2][0];
 d[1][2] = - s[0][0] * s[1][2] + s[0][2] * s[1][0];

 d[2][0] =   s[1][0] * s[2][1] - s[1][1] * s[2][0];
 d[2][1] = - s[0][0] * s[2][1] + s[0][1] * s[2][0];
 d[2][2] =   s[0][0] * s[1][1] - s[0][1] * s[1][0];

 for (j=0;j<3;j++) for (i=0;i<3;i++) d[j][i]/=det;

 d[3][0] = - s[3][0]*d[0][0] - s[3][1]*d[1][0] - s[3][2]*d[2][0];
 d[3][1] = - s[3][0]*d[0][1] - s[3][1]*d[1][1] - s[3][2]*d[2][1];
 d[3][2] = - s[3][0]*d[0][2] - s[3][1]*d[1][2] - s[3][2]*d[2][2];

 d[0][3] = 0;
 d[1][3] = 0;
 d[2][3] = 0;
 d[3][3] = 1;
}

void matrix_Mul(Matrix a, Matrix b)
{
 Matrix t; int i, j;

 for (i=0;i<4;i++) for (j=0;j<4;j++)
     t[i][j]=a[0][j]*b[i][0]+a[1][j]*b[i][1]+
             a[2][j]*b[i][2]+a[3][j]*b[i][3];

 for (i=0;i<4; i++) for (j=0;j<4;j++) a[i][j]=t[i][j];
}

void matrix_Init(Matrix a)
{
 int i,j; for (j=0;j<4;j++) {for (i=0;i<4;i++) a[i][j]=0;a[j][j]=1;}
}

void matrix_Move(Matrix m, float dx, float dy, float dz)
{
 Matrix t; matrix_Init(t);
 t[3][0]=dx; t[3][1]=dy; t[3][2]=dz;
 matrix_Mul(m,t);
}

void matrix_Rotate(Matrix m, float a, int axis)
{
 Matrix t; matrix_Init(t);
 switch (axis) {
  case _X_: t[1][2] = sin(a);
            t[1][1] = cos(a);
            t[2][2] = cos(a);
            t[2][1] =-sin(a);
            break;
  case _Y_: t[0][0] = cos(a);
            t[0][2] =-sin(a);
            t[2][2] = cos(a);
            t[2][0] = sin(a);
            break;
  case _Z_: t[0][0]= cos(a);
            t[0][1]= sin(a);
            t[1][1]= cos(a);
            t[1][0]=-sin(a);
            break;
  }
 matrix_Mul(m,t);
}

} // namespace
