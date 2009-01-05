// --------------------------------------------------------------------------
// Matrix for object transformations.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRObject.h"

namespace rr
{

RRVec3 RRMatrix3x4::transformedPosition(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
	  a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2] + (m)[0][3],
	  a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2] + (m)[1][3],
	  a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2] + (m)[2][3]);
}

RRVec3 RRMatrix3x4::transformedDirection(const RRVec3& a) const
{
	RR_ASSERT(m);
	return RRVec3(
		a[0]*(m)[0][0] + a[1]*(m)[0][1] + a[2]*(m)[0][2],
		a[0]*(m)[1][0] + a[1]*(m)[1][1] + a[2]*(m)[1][2],
		a[0]*(m)[2][0] + a[1]*(m)[2][1] + a[2]*(m)[2][2]);
}

RRVec3& RRMatrix3x4::transformPosition(RRVec3& a) const
{
	RR_ASSERT(m);
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2] + (m)[0][3];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2] + (m)[1][3];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2] + (m)[2][3];

	return a;
}

RRVec3& RRMatrix3x4::transformDirection(RRVec3& a) const
{
	RRReal _x=a.x,_y=a.y,_z=a.z;

	a.x = _x*(m)[0][0] + _y*(m)[0][1] + _z*(m)[0][2];
	a.y = _x*(m)[1][0] + _y*(m)[1][1] + _z*(m)[1][2];
	a.z = _x*(m)[2][0] + _y*(m)[2][1] + _z*(m)[2][2];

	return a;
}

RRReal RRMatrix3x4::determinant3x3() const
{
	return m[0][0]*(m[1][1]*m[2][2]-m[2][1]*m[1][2]) + m[0][1]*(m[1][2]*m[2][0]-m[2][2]*m[1][0]) + m[0][2]*(m[1][0]*m[2][1]-m[2][0]*m[1][1]);
}

} // namespace
