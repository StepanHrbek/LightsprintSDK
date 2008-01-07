#pragma once

#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Transformed mesh importer has all vertices transformed by matrix.

class RRTransformedMeshFilter : public RRMeshFilter
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
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		inherited->getTriangleNormals(t,out);
		if(m)
		{
			m->transformDirection(out.norm[0]); out.norm[0].normalize();
			m->transformDirection(out.norm[1]); out.norm[1].normalize();
			m->transformDirection(out.norm[2]); out.norm[2].normalize();
		}
	}
private:
	const RRMatrix3x4* m;
};

}; // namespace
