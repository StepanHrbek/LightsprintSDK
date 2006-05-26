#pragma once

#include <assert.h>
#include "RRVision.h"

namespace rrVision
{

//////////////////////////////////////////////////////////////////////////////
//
// Transformed object importer has all vertices transformed by matrix.

class RRTransformedObjectFilter : public RRObjectFilter
{
public:
	RRTransformedObjectFilter(RRObjectImporter* aobject, rrCollider::RRCollider::IntersectTechnique intersectTechnique)
		: RRObjectFilter(aobject)
	{
		collider = NULL;
		const RRMatrix3x4* m = base->getWorldMatrix();
		assert(m);
		rrCollider::RRMeshImporter* mesh = new RRTransformedMeshFilter(base->getCollider()->getImporter(),m);
		collider = rrCollider::RRCollider::create(mesh,intersectTechnique);
	}
	virtual ~RRTransformedObjectFilter()
	{
		delete collider->getImporter();
		delete collider;
	}
	// We want to compensate possible negative scale in transformation.
	// way 1
	//  hook in getTriangle and getTriangleBody and possibly also in pre-post conversions
	//  and return triangle vertices in opposite order
	// way 2 YES
	//  hook in getSurface and return sideBits in opposite order
	virtual const RRSurface* getSurface(unsigned s) const
	{
		const RRSurface* surf = base->getSurface(s);
		if(!surf) return surf;
		const RRMatrix3x4* m = base->getWorldMatrix();
		if(!m) return surf;
		bool negScale = m->determinant3x3()<0;
		if(!negScale) return surf;
		//!!! neni thread safe
		static RRSurface surf2 = *surf;
		surf2.sideBits[0] = surf->sideBits[1];
		surf2.sideBits[1] = surf->sideBits[0];
		return &surf2;
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		base->getTriangleNormals(t,out);
		const RRMatrix3x4* m = base->getWorldMatrix();
		if(m)
			for(unsigned i=0;i<3;i++)
			{
				m->transformDirection(out.norm[i]);
			}
	}
private:
	rrCollider::RRCollider* collider;
};

}; // namespace
