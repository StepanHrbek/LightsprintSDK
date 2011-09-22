// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

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
	RRTransformedMeshFilter(const RRMesh* mesh, const RRMatrix3x4* matrix)
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
		if (m)
		{
			m->transformPosition(out);
		}
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		inherited->getTriangleNormals(t,out);
		if (m)
		{
			for (unsigned v=0;v<3;v++)
			{
				// nonuniform scale breaks orthogonality
				out.vertex[v].normal = m->getTransformedDirection(out.vertex[v].normal).normalized();
				out.vertex[v].tangent = m->getTransformedDirection(out.vertex[v].tangent).normalized();
				out.vertex[v].bitangent = m->getTransformedDirection(out.vertex[v].bitangent).normalized();
			}
		}
	}
private:
	const RRMatrix3x4* m;
};

}; // namespace
