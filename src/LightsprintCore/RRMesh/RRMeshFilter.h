// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Mesh filter.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRMesh.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Base class for mesh import filters.

class RRMeshFilter : public RRMesh
{
public:
	RRMeshFilter(const RRMesh* original)
	{
		inherited = original;
		RR_ASSERT(inherited);
		numVertices = inherited ? inherited->getNumVertices() : 0;
		numTriangles = inherited ? inherited->getNumTriangles() : 0;
	}
	virtual ~RRMeshFilter()
	{
		// Delete only what was created by us, not params.
		// So don't delete importer.
	}

	// vertices
	virtual unsigned     getNumVertices() const override
	{
		return numVertices;
	}
	virtual void         getVertex(unsigned v, Vertex& out) const override
	{
		inherited->getVertex(v,out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const override
	{
		return numTriangles;
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const override
	{
		inherited->getTriangle(t,out);
	}
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const override
	{
		inherited->getTriangleNormals(t,out);
	}
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const override
	{
		return inherited->getTriangleMapping(t,out,channel);
	}

	// preimport/postimport conversions
	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const override
	{
		return inherited->getPreImportVertex(postImportVertex, postImportTriangle);
	}
	virtual unsigned     getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const override
	{
		return inherited->getPostImportVertex(preImportVertex, preImportTriangle);
	}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const override
	{
		return inherited->getPreImportTriangle(postImportTriangle);
	}
	virtual unsigned     getPostImportTriangle(PreImportNumber preImportTriangle) const override
	{
		return inherited->getPostImportTriangle(preImportTriangle);
	}

	// tools
	virtual void         getUvChannels(RRVector<unsigned>& out) const override
	{
		inherited->getUvChannels(out);
	}

protected:
	const RRMesh*   inherited;
	unsigned        numVertices;
	unsigned        numTriangles;
};

}; // namespace

