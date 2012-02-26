// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
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
	virtual unsigned     getNumVertices() const
	{
		return numVertices;
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		inherited->getVertex(v,out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return numTriangles;
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		inherited->getTriangle(t,out);
	}
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		inherited->getTriangleNormals(t,out);
	}
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
	{
		return inherited->getTriangleMapping(t,out,channel);
	}

	// preimport/postimport conversions
	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		return inherited->getPreImportVertex(postImportVertex, postImportTriangle);
	}
	virtual unsigned     getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const 
	{
		return inherited->getPostImportVertex(preImportVertex, preImportTriangle);
	}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const 
	{
		return inherited->getPreImportTriangle(postImportTriangle);
	}
	virtual unsigned     getPostImportTriangle(PreImportNumber preImportTriangle) const 
	{
		return inherited->getPostImportTriangle(preImportTriangle);
	}

	// tools
	virtual void         getUvChannels(RRVector<unsigned>& out) const
	{
		inherited->getUvChannels(out);
	}

protected:
	const RRMesh*   inherited;
	unsigned        numVertices;
	unsigned        numTriangles;
};

}; // namespace

