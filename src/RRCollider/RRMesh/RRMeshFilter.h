#pragma once

#include <assert.h>
#include "RRCollider.h"

namespace rrCollider
{

//////////////////////////////////////////////////////////////////////////////
//
// Base class for mesh import filters.

class RRMeshFilter : public rrCollider::RRMesh
{
public:
	RRMeshFilter(const RRMesh* original)
	{
		importer = original;
		assert(importer);
		numVertices = importer ? importer->getNumVertices() : 0;
		numTriangles = importer ? importer->getNumTriangles() : 0;
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
		importer->getVertex(v,out);
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return numTriangles;
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		importer->getTriangle(t,out);
	}

	// preimport/postimport conversions
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	{
		return importer->getPreImportVertex(postImportVertex, postImportTriangle);
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const 
	{
		return importer->getPostImportVertex(preImportVertex, preImportTriangle);
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const 
	{
		return importer->getPreImportTriangle(postImportTriangle);
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		return importer->getPostImportTriangle(preImportTriangle);
	}

protected:
	const RRMesh* importer;
	unsigned        numVertices;
	unsigned        numTriangles;
};

}; // namespace

