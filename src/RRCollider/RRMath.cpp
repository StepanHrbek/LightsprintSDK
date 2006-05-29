#include "RRMath.h"

#include <math.h> // sqrt

namespace rr
{

RRVec3 operator -(const RRVec3& a)
{
	return RRVec3(-a.x,-a.y,-a.z);
}

RRReal size(const RRVec3& a)
{
	return sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
}

RRReal size2(const RRVec3& a)
{
	return a.x*a.x+a.y*a.y+a.z*a.z;
}

RRVec3 normalized(const RRVec3& a)
{
	return a/size(a);
}

RRReal dot(const RRVec3& a,const RRVec3& b)
{
	return a.x*b.x+a.y*b.y+a.z*b.z;
}

RRVec3 ortogonalTo(const RRVec3& a)
{
	return RRVec3(0,a.z,-a.y);
}

// dohoda:
// predpokladejme triangl s vrcholy 0,a,b.
// pri pohledu na jeho vnejsi stranu (tu s normalou) vidime vrcholy
// serazene proti smeru hodinovych rucicek.
// jinak receno: ortogonalTo(doprava,dopredu)==nahoru

RRVec3 ortogonalTo(const RRVec3& a,const RRVec3& b)
{
	return RRVec3(a.y*b.z-a.z*b.y,-a.x*b.z+a.z*b.x,a.x*b.y-a.y*b.x);
}

} //namespace
