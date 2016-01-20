// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Mesh transformer.
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
	RRTransformedMeshFilter(const RRMesh* mesh, const RRMatrix3x4Ex* matrix)
		: RRMeshFilter(mesh)
	{
		m = matrix;
	}
	/*RRTransformedMeshFilter(RRObject* object)
		: RRMeshFilter(object->getCollider()->getMesh())
	{
		m = object->getWorldMatrix();
	}*/

	virtual void getVertex(unsigned v, Vertex& out) const override
	{
		inherited->getVertex(v,out);
		if (m)
		{
			m->transformPosition(out);
		}
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const override
	{
		inherited->getTriangleNormals(t,out);
		if (m)
		{
			for (unsigned v=0;v<3;v++)
			{
				// nonuniform scale breaks orthogonality
				out.vertex[v].normal = m->getTransformedNormal(out.vertex[v].normal).normalized();
				out.vertex[v].tangent = m->getTransformedNormal(out.vertex[v].tangent).normalized();
				out.vertex[v].bitangent = m->getTransformedNormal(out.vertex[v].bitangent).normalized();
			}
		}
	}
private:
	const RRMatrix3x4Ex* m;
};

}; // namespace
