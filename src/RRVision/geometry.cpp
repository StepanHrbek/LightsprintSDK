#include <assert.h>
#include <math.h>
#include <memory.h>
#include "geometry.h"

namespace rr
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

RRVec3 RRMatrix3x4::transformedPosition(const RRVec3& a) const
{
	assert(m);
	return Vec3(
	  a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2] + (m)[0][3],
	  a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2] + (m)[1][3],
	  a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2] + (m)[2][3]);
}

Vec3 RRMatrix3x4::transformedDirection(const RRVec3& a) const
{
	assert(m);
	return Vec3(
		a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2],
		a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2],
		a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2]);
}

Vec3& RRMatrix3x4::transformPosition(RRVec3& a) const
{
	assert(m);
	real _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2] + (m)[0][3];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2] + (m)[1][3];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2] + (m)[2][3];

	return a;
}

Vec3& RRMatrix3x4::transformDirection(RRVec3& a) const
{
	real _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2];

	return a;
}

RRReal RRMatrix3x4::determinant3x3() const
{
	return m[0][0]*(m[1][1]*m[2][2]-m[2][1]*m[1][2]) + m[0][1]*(m[1][2]*m[2][0]-m[2][2]*m[1][0]) + m[0][2]*(m[1][0]*m[2][1]-m[2][0]*m[1][1]);
}

RRReal abs(RRReal a)
{
	return fabs(a);
}

RRVec3 abs(const RRVec3& a)
{
	return RRVec3(fabs(a.x),fabs(a.y),fabs(a.z));
}

RRReal sum(RRReal a)
{
	return a;
}

RRReal sum(const RRVec3& a)
{
	return a.x+a.y+a.z;
}

RRReal avg(RRReal a)
{
	return a;
}

RRReal avg(const RRVec3& a)
{
	return (a.x+a.y+a.z)/3;
}

void clampToZero(RRReal& a)
{
	if(a<0) a=0;
}

void clampToZero(RRVec3& a)
{
	if(a.x<0) a.x=0;
	if(a.y<0) a.y=0;
	if(a.z<0) a.z=0;
}

RRReal angleBetweenNormalized(const RRVec3& a,const RRVec3& b)
{
	RRReal d = dot(a,b);
	RRReal angle = acos(MAX(MIN(d,1),-1));
	assert(IS_NUMBER(angle));
	return angle;
}

RRReal angleBetween(const RRVec3& a,const RRVec3& b)
{
	return angleBetweenNormalized(normalized(a),normalized(b));
}


//////////////////////////////////////////////////////////////////////////////
//
// normal in 3d

real normalValueIn(Normal n,Point3 a)
{
	return a.x*n.x+a.y*n.y+a.z*n.z+n.w;
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

} // namespace
