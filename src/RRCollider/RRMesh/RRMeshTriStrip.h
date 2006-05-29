#pragma once

#include "RRCollider.h"

#include <assert.h>


namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// Importers from vertex/index buffers
//
// support indices of any size, vertex positions float[3]
//
// RRMeshTriStrip               - triangle strip
// RRMeshTriList                - triangle list
// RRMeshIndexedTriStrip<INDEX> - indexed triangle strip 
// RRMeshIndexedTriList<INDEX>  - indexed triangle list

class RRMeshTriStrip : public RRMesh
{
public:
	RRMeshTriStrip(char* vbuffer, unsigned vertices, unsigned stride)
		: VBuffer(vbuffer), Vertices(vertices), Stride(stride)
	{
	}

	virtual unsigned getNumVertices() const
	{
		return Vertices;
	}
	virtual void getVertex(unsigned v, Vertex& out) const
	{
		assert(v<Vertices);
		assert(VBuffer);
		out = *(Vertex*)(VBuffer+v*Stride);
	}
	virtual unsigned getNumTriangles() const
	{
		return Vertices-2;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<Vertices-2);
		out[0] = t+0;
		out[1] = t+1;
		out[2] = t+2;
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		assert(t<Vertices-2);
		assert(VBuffer);
		unsigned v0,v1,v2;
		v0 = t+0;
		v1 = t+1;
		v2 = t+2;
#define VERTEX(v) ((RRReal*)(VBuffer+v*Stride))
		out.vertex0[0] = VERTEX(v0)[0];
		out.vertex0[1] = VERTEX(v0)[1];
		out.vertex0[2] = VERTEX(v0)[2];
		out.side1[0] = VERTEX(v1)[0]-out.vertex0[0];
		out.side1[1] = VERTEX(v1)[1]-out.vertex0[1];
		out.side1[2] = VERTEX(v1)[2]-out.vertex0[2];
		out.side2[0] = VERTEX(v2)[0]-out.vertex0[0];
		out.side2[1] = VERTEX(v2)[1]-out.vertex0[1];
		out.side2[2] = VERTEX(v2)[2]-out.vertex0[2];
#undef VERTEX
	}

protected:
	char*                VBuffer;
	unsigned             Vertices;
	unsigned             Stride;
};

} //namespace
