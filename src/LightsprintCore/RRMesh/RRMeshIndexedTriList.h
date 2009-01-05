// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRMesh.h"

#include <cassert>


namespace rr
{


template <class INDEX>
#define INHERITED RRMeshIndexedTriStrip<INDEX>
class RRMeshIndexedTriList : public INHERITED
{
public:
	RRMeshIndexedTriList(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: RRMeshIndexedTriStrip<INDEX>(vbuffer,vertices,stride,ibuffer,indices)
	{
		INDEX tmp = vertices;
		tmp = tmp;
		RR_ASSERT(tmp==vertices);
	}
	virtual unsigned getNumTriangles() const
	{
		return INHERITED::Indices/3;
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		RR_ASSERT(t*3<INHERITED::Indices);
		RR_ASSERT(INHERITED::IBuffer);
		out[0] = INHERITED::IBuffer[t*3+0]; RR_ASSERT(out[0]<INHERITED::Vertices);
		out[1] = INHERITED::IBuffer[t*3+1]; RR_ASSERT(out[1]<INHERITED::Vertices);
		out[2] = INHERITED::IBuffer[t*3+2]; RR_ASSERT(out[2]<INHERITED::Vertices);
	}
	virtual void getTriangleBody(unsigned t, RRMesh::TriangleBody& out) const
	{
		RR_ASSERT(t*3<INHERITED::Indices);
		RR_ASSERT(INHERITED::VBuffer);
		RR_ASSERT(INHERITED::IBuffer);
		unsigned v0,v1,v2;
		v0 = INHERITED::IBuffer[t*3+0]; RR_ASSERT(v0<INHERITED::Vertices);
		v1 = INHERITED::IBuffer[t*3+1]; RR_ASSERT(v1<INHERITED::Vertices);
		v2 = INHERITED::IBuffer[t*3+2]; RR_ASSERT(v2<INHERITED::Vertices);
#define VERTEX(v) ((RRReal*)(INHERITED::VBuffer+v*INHERITED::Stride))
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
};
#undef INHERITED


} //namespace
