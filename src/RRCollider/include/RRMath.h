#ifndef COLLIDER_RRMATH_H
#define COLLIDER_RRMATH_H

#include "RRCollider.h"

namespace rrCollider
{

	RRVec3 RRCOLLIDER_API operator -(const RRVec3& a);
	RRReal RRCOLLIDER_API size(const RRVec3& a);
	RRReal RRCOLLIDER_API size2(const RRVec3& a);
	RRVec3 RRCOLLIDER_API normalized(const RRVec3& a);
	RRReal RRCOLLIDER_API dot(const RRVec3& a,const RRVec3& b);
	RRVec3 RRCOLLIDER_API ortogonalTo(const RRVec3& a);
	RRVec3 RRCOLLIDER_API ortogonalTo(const RRVec3& a,const RRVec3& b);

} // namespace

#endif
