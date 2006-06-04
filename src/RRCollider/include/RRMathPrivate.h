#ifndef COLLIDER_RRMATH_H
#define COLLIDER_RRMATH_H

#include "RRCollider.h"

namespace rr
{

	RRVec3 RR_API operator -(const RRVec3& a);
	RRReal RR_API size(const RRVec3& a);
	RRReal RR_API size2(const RRVec3& a);
	RRVec3 RR_API normalized(const RRVec3& a);
	RRReal RR_API dot(const RRVec3& a,const RRVec3& b);
	RRVec3 RR_API ortogonalTo(const RRVec3& a);
	RRVec3 RR_API ortogonalTo(const RRVec3& a,const RRVec3& b);

} // namespace

#endif
