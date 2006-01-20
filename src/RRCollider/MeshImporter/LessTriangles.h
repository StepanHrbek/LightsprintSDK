#pragma once

#include "RRCollider.h"

#include <assert.h>


namespace rrCollider
{


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
			RRMeshImporter::Triangle t;
			INHERITED::getTriangle(i,t);
			if(!(t[0]==t[1] || t[0]==t[2] || t[1]==t[2])) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for(unsigned i=0;i<numAllTriangles;i++)
		{
			RRMeshImporter::Triangle t;
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
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangle(ValidIndex[t],out);
	}
	virtual unsigned getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if(postImportTriangle>=ValidIndices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMeshImporter::UNDEFINED;
		}
		return ValidIndex[postImportTriangle];
	}
	virtual unsigned getPostImportTriangle(unsigned preImportTriangle) const 
	{
		// efficient implementation would require another translation array
		for(unsigned post=0;post<ValidIndices;post++)
			if(ValidIndex[post]==preImportTriangle)
				return post;
		return RRMeshImporter::UNDEFINED;
	}
	virtual void getTriangleBody(unsigned t, RRMeshImporter::TriangleBody& out) const
	{
		assert(t<ValidIndices);
		INHERITED::getTriangleBody(ValidIndex[t],out);
	}

protected:
	unsigned*            ValidIndex; // may be uint16_t when indices<64k
	unsigned             ValidIndices;
};

} //namespace
