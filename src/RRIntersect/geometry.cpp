#include <assert.h>
#include <math.h>
#include <memory.h>
#include "geometry.h"

namespace rrIntersect
{

//////////////////////////////////////////////////////////////////////////////
//
// 3d vector

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

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

Vec3 ortogonalTo(Vec3 a,Vec3 b)
{
	return Vec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

//////////////////////////////////////////////////////////////////////////////
//
// normal in 3d

real normalValueIn(Plane n,Vec3 a)
{
	return a.x*n.x+a.y*n.y+a.z*n.z+n.d;
}

} // namespace
