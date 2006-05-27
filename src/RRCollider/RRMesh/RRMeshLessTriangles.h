#pragma once

#include "RRCollider.h"

#include <assert.h>


namespace rrCollider
{

//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessTrianglesFilter - importer slow-filter that removes degenerate triangles

class RRLessTrianglesFilter : public RRMeshFilter
{
public:
	RRLessTrianglesFilter(const RRMesh* original)
		: RRMeshFilter(original)
	{
		ValidIndices = 0;
		unsigned numAllTriangles = inherited->getNumTriangles();
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::Triangle t;
			inherited->getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::Triangle t;
			inherited->getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndex[ValidIndices++] = i;
		}
	};
	~RRLessTrianglesFilter()
	{
		delete[] ValidIndex;
	}

	virtual unsigned getNumTriangles() const
	{
		return ValidIndices;
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		assert(t<ValidIndices);
		inherited->getTriangle(ValidIndex[t],out);
	}
	virtual unsigned getPreImportTriangle(unsigned postImportTriangle) const
	{
		if(postImportTriangle>=ValidIndices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return inherited->getPreImportTriangle(midImportTriangle);
	}
	virtual unsigned getPostImportTriangle(unsigned preImportTriangle) const 
	{
		// check that this slow code is not called often
		assert(0);
		// efficient implementation would require another translation array
		unsigned midImportTriangle = inherited->getPostImportTriangle(preImportTriangle);
		for(unsigned post=0;post<ValidIndices;post++)
			if(ValidIndex[post]==midImportTriangle)
				return post;
		return RRMesh::UNDEFINED;
	}
	virtual unsigned  getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	// getPreImportVertex: postImportTriangle must be converted to midImportTriangle before calling inherited importer
	// getPostImportVertex:  no conversion needed
	{
		if(postImportTriangle>=ValidIndices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return inherited->getPreImportVertex(postImportVertex, midImportTriangle);
	}
	virtual void getTriangleBody(unsigned t, RRMesh::TriangleBody& out) const
	{
		assert(t<ValidIndices);
		inherited->getTriangleBody(ValidIndex[t],out);
	}

protected:
	unsigned*            ValidIndex; // may be uint16_t when indices<64k
	unsigned             ValidIndices;
};

//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessTrianglesImporter<IMPORTER,INDEX> - importer fast-filter that removes degenerate triangles

template <class INHERITED, class INDEX>
class RRLessTrianglesImporter : public INHERITED
{
public:
	RRLessTrianglesImporter(char* vbuffer, unsigned vertices, unsigned stride, INDEX* ibuffer, unsigned indices)
		: INHERITED(vbuffer,vertices,stride,ibuffer,indices) 
	{
		ValidIndices = 0;
		unsigned numAllTriangles = INHERITED::getNumTriangles();
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::Triangle t;
			INHERITED::getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::Triangle t;
			INHERITED::getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndex[ValidIndices++] = i;
		}
	};
	~RRLessTrianglesImporter()
	{
		delete[] ValidIndex;
	}

	virtual unsigned getNumTriangles() const
	{
		return ValidIndices;
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangle(ValidIndex[t],out);
	}
	virtual unsigned getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle>=ValidIndices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		//return ValidIndex[postImportTriangle];
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return INHERITED::getPreImportTriangle(midImportTriangle);
	}
	virtual unsigned getPostImportTriangle(unsigned preImportTriangle) const 
	{
		// check that this slow code is not called often
		assert(0);
		// efficient implementation would require another translation array
		unsigned midImportTriangle = INHERITED::getPostImportTriangle(preImportTriangle);
		for(unsigned post=0;post<ValidIndices;post++)
			if(ValidIndex[post]==midImportTriangle)
				return post;
		return RRMesh::UNDEFINED;
	}
	virtual unsigned  getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	// getPreImportVertex: postImportTriangle must be converted to midImportTriangle before calling inherited importer
	// getPostImportVertex:  no conversion needed
	{
		if(postImportTriangle>=ValidIndices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return INHERITED::getPreImportVertex(postImportVertex, midImportTriangle);
	}
	virtual void getTriangleBody(unsigned t, RRMesh::TriangleBody& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangleBody(ValidIndex[t],out);
	}

protected:
	unsigned*            ValidIndex; // may be uint16_t when indices<64k
	unsigned             ValidIndices;
};

} //namespace
