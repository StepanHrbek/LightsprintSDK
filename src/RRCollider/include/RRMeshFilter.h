#ifndef COLLIDER_RRFILTEREDMESHIMPORTER_H
#define COLLIDER_RRFILTEREDMESHIMPORTER_H

#include "RRCollider.h"

namespace rrCollider
{

//////////////////////////////////////////////////////////////////////////////
//
// Base class for mesh import filters.

class RRFilteredMeshImporter : public rrCollider::RRMeshImporter
{
public:
	RRFilteredMeshImporter(const RRMeshImporter* mesh)
	{
		importer = mesh;
		numVertices = importer ? importer->getNumVertices() : 0;
		numTriangles = importer ? importer->getNumTriangles() : 0;
	}
	virtual ~RRFilteredMeshImporter()
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
	const RRMeshImporter* importer;
	unsigned        numVertices;
	unsigned        numTriangles;
};

}; // namespace

#endif
