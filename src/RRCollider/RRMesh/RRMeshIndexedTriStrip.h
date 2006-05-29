#pragma once

#include "RRMeshTriStrip.h"

#include <assert.h>


namespace rr
{


template <class INDEX>
class RRMeshIndexedTriStrip : public RRMeshTriStrip
{
public:
	RRMeshIndexedTriStrip(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: RRMeshTriStrip(vbuffer,vertices,stride), IBuffer(ibuffer), Indices(indices)
	{
		INDEX tmp = vertices;
		tmp = tmp;
		assert(tmp==vertices);
	}

	virtual unsigned getNumTriangles() const
	{
		return Indices-2;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<Indices-2);
		assert(IBuffer);
		out[0] = IBuffer[t];         assert(out[0]<Vertices);
		out[1] = IBuffer[t+1+(t%2)]; assert(out[1]<Vertices);
		out[2] = IBuffer[t+2-(t%2)]; assert(out[2]<Vertices);
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		assert(t<Indices-2);
		assert(VBuffer);
		assert(IBuffer);
		unsigned v0,v1,v2;
		v0 = IBuffer[t];         assert(v0<Vertices);
		v1 = IBuffer[t+1+(t%2)]; assert(v1<Vertices);
		v2 = IBuffer[t+2-(t%2)]; assert(v2<Vertices);
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
	INDEX*               IBuffer;
	unsigned             Indices;
};

} //namespace
