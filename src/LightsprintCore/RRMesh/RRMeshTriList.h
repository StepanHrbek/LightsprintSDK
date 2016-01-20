// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Mesh adapter for tristrips.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRMesh.h"

namespace rr
{

class RRMeshTriList : public RRMeshTriStrip
{
public:
	RRMeshTriList(char* vbuffer, unsigned vertices, unsigned stride)
		: RRMeshTriStrip(vbuffer,vertices,stride)
	{
	}
	virtual unsigned getNumTriangles() const
	{
		return Vertices/3;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		RR_ASSERT(t*3<Vertices);
		out[0] = t*3+0;
		out[1] = t*3+1;
		out[2] = t*3+2;
	}
	virtual void getTriangleBody(unsigned t, TriangleBody& out) const
	{
		RR_ASSERT(t*3<Vertices);
		RR_ASSERT(VBuffer);
		unsigned v0,v1,v2;
		v0 = t*3+0;
		v1 = t*3+1;
		v2 = t*3+2;
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
};

} //namespace
