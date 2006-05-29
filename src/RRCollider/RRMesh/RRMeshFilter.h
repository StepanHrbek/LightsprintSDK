#pragma once

#include <assert.h>
#include "RRCollider.h"

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
		assert(inherited);
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

	// preimport/postimport conversions
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		return inherited->getPreImportVertex(postImportVertex, postImportTriangle);
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		return inherited->getPostImportVertex(preImportVertex, preImportTriangle);
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		return inherited->getPreImportTriangle(postImportTriangle);
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		return inherited->getPostImportTriangle(preImportTriangle);
	}

protected:
	const RRMesh*   inherited;
	unsigned        numVertices;
	unsigned        numTriangles;
};

}; // namespace

