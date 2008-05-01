// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
		data = new TriangleData[numTriangles];
		for(unsigned t=0;t<numTriangles;t++)
		{
			_inherited->getTriangleBody(t,data[t].body);
			_inherited->getTriangleNormals(t,data[t].normals);
		}
	}
	virtual ~RRMeshAccelerated()
	{
		delete[] data;
	}
	virtual void         getTriangleBody(unsigned t, TriangleBody& out) const
	{
		if(t<numTriangles)
			out = data[t].body;
	}
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if(t<numTriangles)
			out = data[t].normals;
	}

protected:
	struct TriangleData
	{
		TriangleBody body;
		TriangleNormals normals;
	};
	unsigned        numTriangles;
	TriangleData*   data;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

const RRMesh* RRMesh::createAccelerated() const
{
	try
	{
		return this ? new RRMeshAccelerated(this) : NULL;
	}
	catch(std::bad_alloc e)
	{
		RRReporter::report(ERRO,"Not enough memory, mesh not accelerated.\n");
		return this;
	}
}

}; // namespace
