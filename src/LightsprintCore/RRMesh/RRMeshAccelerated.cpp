// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRMeshFilter.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Mesh with local copy of TrangleBase and TriangleNormals for faster access

class RRMeshAccelerated : public RRMeshFilter
{
public:
	RRMeshAccelerated(const RRMesh* _inherited)
		: RRMeshFilter(_inherited)
	{
		RR_ASSERT(_inherited);
		numTriangles = _inherited ? _inherited->getNumTriangles() : 0;
		data = new (std::nothrow) TriangleData[numTriangles];
		if (data)
		{
			for (unsigned t=0;t<numTriangles;t++)
			{
				_inherited->getTriangleBody(t,data[t].body);
				_inherited->getTriangleNormals(t,data[t].normals);
			}
		}
	}
	virtual ~RRMeshAccelerated()
	{
		delete[] data;
	}
	virtual void         getTriangleBody(unsigned t, TriangleBody& out) const
	{
		if (t<numTriangles)
			out = data[t].body;
	}
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if (t<numTriangles)
			out = data[t].normals;
	}

	struct TriangleData
	{
		TriangleBody body;
		TriangleNormals normals;
	};
	TriangleData*   data; // public only for createAccelerated()
protected:
	unsigned        numTriangles;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

const RRMesh* RRMesh::createAccelerated() const
{
	RRMeshAccelerated* result = NULL;
	if (this)
	{
		result = new RRMeshAccelerated(this);
		if (!result->data)
		{
			RRReporter::report(ERRO,"Not enough memory, mesh not accelerated.\n");
			RR_SAFE_DELETE(result);
		}
	}
	return result;
}

}; // namespace
