#pragma once

#include <cassert>
#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Transformed object importer has all vertices transformed by matrix.

class RRTransformedObjectFilter : public RRObjectFilter
{
public:
	RRTransformedObjectFilter(RRObject* aobject, bool anegScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation)
		: RRObjectFilter(aobject)
	{
		negScaleMakesOuterInner = anegScaleMakesOuterInner;
		collider = NULL;
		const RRMatrix3x4* m = inherited->getWorldMatrix();
		RR_ASSERT(m);
		RRMesh* mesh = new RRTransformedMeshFilter(inherited->getCollider()->getMesh(),m);
		// it would be possible to reuse collider of aobject, our collider would transform
		//  both inputs and outputs and call aobject's collider with complicated collisionHandler
		// quite complicated and slower, let's rather create new collider
		collider = RRCollider::create(mesh,intersectTechnique,cacheLocation);
	}
	virtual ~RRTransformedObjectFilter()
	{
		delete collider->getMesh();
		delete collider;
	}
	// We want to compensate possible negative scale in transformation.
	// way 1
	//  hook in getTriangle and getTriangleBody and possibly also in pre-post conversions
	//  and return triangle vertices in opposite order
	// way 2 YES
	//  hook in getTriangleMaterial and return sideBits in opposite order
	virtual const RRMaterial* getTriangleMaterial(unsigned t) const
	{
		const RRMaterial* surf = inherited->getTriangleMaterial(t);
		if(!surf || negScaleMakesOuterInner) return surf;
		const RRMatrix3x4* m = inherited->getWorldMatrix();
		if(!m) return surf;
		bool negScale = m->determinant3x3()<0;
		if(!negScale) return surf;
		//!!! neni thread safe
		static RRMaterial surf2 = *surf;
		surf2.sideBits[0] = surf->sideBits[1];
		surf2.sideBits[1] = surf->sideBits[0];
		return &surf2;
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		inherited->getChannelSize(channelId,numItems,itemSize);
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		return inherited->getChannelData(channelId,itemIndex,itemData,itemSize);
	}
private:
	RRCollider* collider;
	bool negScaleMakesOuterInner;
};

}; // namespace
