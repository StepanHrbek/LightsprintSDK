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

real size2(Vec2 a)
{
	return a.x*a.x+a.y*a.y;
}

Vec2 normalized(Vec2 a)
{
	return a/size(a);
}

real dot(Vec2 a,Vec2 b)
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
	real r=size2(a-b);
	return acos(1-r/2);
}

Angle angleBetween(Vec2 a,Vec2 b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

Vec3 Vec3::transformed(const Matrix *m) const
{
	return Vec3(
	  x*(*m)[0][0] + y*(*m)[1][0] + z*(*m)[2][0] + (*m)[3][0],
	  x*(*m)[0][1] + y*(*m)[1][1] + z*(*m)[2][1] + (*m)[3][1],
	  x*(*m)[0][2] + y*(*m)[1][2] + z*(*m)[2][2] + (*m)[3][2]);
}

Vec3 Vec3::rotated(const Matrix *m) const
{
	return Vec3(
		x*(*m)[0][0] + y*(*m)[1][0] + z*(*m)[2][0],
		x*(*m)[0][1] + y*(*m)[1][1] + z*(*m)[2][1],
		x*(*m)[0][2] + y*(*m)[1][2] + z*(*m)[2][2]);
}

Vec3 Vec3::transform(const Matrix *m)
{
	real _x=x,_y=y,_z=z;

	x = _x*(*m)[0][0] + _y*(*m)[1][0] + _z*(*m)[2][0] + (*m)[3][0];
	y = _x*(*m)[0][1] + _y*(*m)[1][1] + _z*(*m)[2][1] + (*m)[3][1];
	z = _x*(*m)[0][2] + _y*(*m)[1][2] + _z*(*m)[2][2] + (*m)[3][2];

	return *this;
}

Vec3 Vec3::rotate(const Matrix *m)
{
	real _x=x,_y=y,_z=z;

	x = _x*(*m)[0][0] + _y*(*m)[1][0] + _z*(*m)[2][0];
	y = _x*(*m)[0][1] + _y*(*m)[1][1] + _z*(*m)[2][1];
	z = _x*(*m)[0][2] + _y*(*m)[1][2] + _z*(*m)[2][2];

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

real size2(Vec3 a)
{
	return a.x*a.x+a.y*a.y+a.z*a.z;
}

real abs(real a)
{
	return fabs(a);
}

Vec3 abs(Vec3 a)
{
	return Vec3(fabs(a.x),fabs(a.y),fabs(a.z));
}

real sum(real a)
{
	return a;
}

real sum(Vec3 a)
{
	return a.x+a.y+a.z;
}

real avg(real a)
{
	return a;
}

real avg(Vec3 a)
{
	return (a.x+a.y+a.z)/3;
}

void clampToZero(real& a)
{
	if(a<0) a=0;
}

void clampToZero(Vec3& a)
{
	if(a.x<0) a.x=0;
	if(a.y<0) a.y=0;
	if(a.z<0) a.z=0;
}

Vec3 normalized(Vec3 a)
{
	return a/size(a);
}

real dot(Vec3 a,Vec3 b)
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
	real r=size2(a-b);
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

void Bound::detect(const Vec3 *vertex,unsigned vertices)
{
	Point3 sum=Point3(0,0,0);
	for(unsigned i=0;i<vertices;i++) sum+=vertex[i];
	center=sum/vertices;
	radius2=0;
	for(unsigned i=0;i<vertices;i++) 
	{
		real tmp=size2(vertex[i]-center);
		if(tmp>radius2) radius2=tmp;
	}
	radius=sqrt(radius2);
	//...najit presnejsi bound
	centerBeforeTransformation=center;
}

// does ray intersect sphere in distance < maxDistance?

bool Bound::intersect(Point3 eye,Vec3 direction,real maxDistance)
{
	Vec3 toCenter=center-eye;
	real distEyeCenter2=size2(toCenter);            //3*mul

	// eye inside sphere
	if(distEyeCenter2-radius2<0) return true;

	real distEyeCenter=sqrt(distEyeCenter2);             //1*sqrt

	// sphere too far
	if(distEyeCenter-radius>=maxDistance) return false;

	assert(fabs(size2(direction)-1)<0.001);

#ifdef FAST_BOUND
	Point3 nearCenter=eye+direction*distEyeCenter;            //3*mul
	real distNrcntrCenter2=size2(nearCenter-center);//3*mul

	// probably no intersection
	if(distNrcntrCenter2>=radius2) return false;
#else
	real psqr=size2(toCenter/distEyeCenter-direction);
	real cosa=1-psqr/2;
	real sina2=psqr*(1-psqr/4);//=sin(acos(cosa));
	real distNrstCenter2=distEyeCenter2*sina2;

	// no intersection
	if(distNrstCenter2>=radius2) return false;

	real distNrstEye=distEyeCenter*cosa;
	real distNrstIntrsct=sqrt(radius*radius-distNrstCenter2);
	real distEyeIntrsct=distNrstEye-distNrstIntrsct;

	// intersection too far
	if(distEyeIntrsct>=maxDistance) return false;

	// intersection beyond eye
	if(size2(toCenter+direction*distEyeCenter)<size2(toCenter-direction*distEyeCenter)) return false;
#endif
	return true;
}

bool Bound::visible(const Matrix *camera)
{
	//...
	return true;
}

} // namespace
