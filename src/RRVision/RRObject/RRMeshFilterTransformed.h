#pragma once

#include "RRVision.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshFilter : public rr::RRMeshFilter
{
public:
	RRTransformedMeshFilter(RRMesh* mesh, const RRMatrix3x4* matrix)
		: RRMeshFilter(mesh)
	{
		m = matrix;
	}
	/*RRTransformedMeshFilter(RRObject* object)
	: RRMeshFilter(object->getCollider()->getMesh())
	{
	m = object->getWorldMatrix();
	}*/

	virtual void getVertex(unsigned v, Vertex& out) const
	{
		inherited->getVertex(v,out);
		if(m)
		{
			m->transformPosition(out);
		}
	}
private:
	const RRMatrix3x4* m;
};

}; // namespace
