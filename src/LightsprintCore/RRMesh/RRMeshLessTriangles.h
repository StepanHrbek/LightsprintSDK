// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include "Lightsprint/RRMesh.h"

#include <cassert>


namespace rr
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
		for (unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::TriangleBody tb;
			inherited->getTriangleBody(i,tb);
			if (tb.isNotDegenerated()) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for (unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::TriangleBody tb;
			inherited->getTriangleBody(i,tb);
			if (tb.isNotDegenerated()) ValidIndex[ValidIndices++] = i;
		}
	};
	~RRLessTrianglesFilter()
	{
		delete[] ValidIndex;
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		inherited->getChannelSize(channelId,numItems,itemSize);
		if (numItems && *numItems && (channelId&0x7ffff000)==INDEXED_BY_TRIANGLE)
		{
			*numItems = ValidIndices;
		}
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		if ((channelId&0x7ffff000) == INDEXED_BY_TRIANGLE)
		{
			if (itemIndex<ValidIndices)
			{
				itemIndex = ValidIndex[itemIndex];
			}
			else
			{
				RR_ASSERT(0);
				return false;
			}
		}
		return inherited->getChannelData(channelId,itemIndex,itemData,itemSize);
	}

	virtual unsigned getNumTriangles() const
	{
		return ValidIndices;
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		RR_ASSERT(t<ValidIndices);
		inherited->getTriangle(ValidIndex[t],out);
	}
	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if (t>=ValidIndices)
		{
			RR_ASSERT(0); // legal but bad practise, good to be warned when it happens
			return;
		}
		t = ValidIndex[t];
		inherited->getTriangleNormals(t,out);
	}
	virtual void getTriangleMapping(unsigned t, TriangleMapping& out) const
	{
		if (t>=ValidIndices)
		{
			RR_ASSERT(0); // legal but bad practise, good to be warned when it happens
			return;
		}
		t = ValidIndex[t];
		inherited->getTriangleMapping(t,out);
	}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const
	{
		if (postImportTriangle>=ValidIndices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return inherited->getPreImportTriangle(midImportTriangle);
	}
	virtual unsigned getPostImportTriangle(PreImportNumber preImportTriangle) const 
	{
		// check that this slow code is not called often
//!!! is called when RRDynamicSolver works with single object
//		RR_ASSERT(0);
		// efficient implementation would require another translation array
		unsigned midImportTriangle = inherited->getPostImportTriangle(preImportTriangle);
		for (unsigned post=0;post<ValidIndices;post++)
			if (ValidIndex[post]==midImportTriangle)
				return post;
		return RRMesh::UNDEFINED;
	}
	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	// getPreImportVertex: postImportTriangle must be converted to midImportTriangle before calling inherited importer
	// getPostImportVertex:  no conversion needed
	{
		if (postImportTriangle>=ValidIndices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return inherited->getPreImportVertex(postImportVertex, midImportTriangle);
	}
	virtual void getTriangleBody(unsigned t, RRMesh::TriangleBody& out) const
	{
		RR_ASSERT(t<ValidIndices);
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
		for (unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::TriangleBody tb;
			INHERITED::getTriangleBody(i,tb);
			if (tb.isNotDegenerated()) ValidIndices++;
		}
		ValidIndex = new unsigned[ValidIndices];
		ValidIndices = 0;
		for (unsigned i=0;i<numAllTriangles;i++)
		{
			RRMesh::TriangleBody tb;
			INHERITED::getTriangleBody(i,tb);
			if (tb.isNotDegenerated()) ValidIndex[ValidIndices++] = i;
		}
	};
	~RRLessTrianglesImporter()
	{
		delete[] ValidIndex;
	}

	// channels
	virtual void getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
	{
		INHERITED::getChannelSize(channelId,numItems,itemSize);
		if (numItems && *numItems && (channelId&0x7ffff000)==RRMesh::INDEXED_BY_TRIANGLE)
		{
			*numItems = ValidIndices;
		}
	}
	virtual bool getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
	{
		if ((channelId&0x7ffff000) == RRMesh::INDEXED_BY_TRIANGLE)
		{
			if (itemIndex<ValidIndices)
			{
				itemIndex = ValidIndex[itemIndex];
			}
			else
			{
				RR_ASSERT(0);
				return false;
			}
		}
		return INHERITED::getChannelData(channelId,itemIndex,itemData,itemSize);
	}

	virtual unsigned getNumTriangles() const
	{
		return ValidIndices;
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		RR_ASSERT(t<ValidIndices);
		INHERITED::getTriangle(ValidIndex[t],out);
	}
	virtual RRMesh::PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const 
	{
		if (postImportTriangle>=ValidIndices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}
		//return ValidIndex[postImportTriangle];
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return INHERITED::getPreImportTriangle(midImportTriangle);
	}
	virtual unsigned getPostImportTriangle(RRMesh::PreImportNumber preImportTriangle) const 
	{
		// check that this slow code is not called often
		//RR_ASSERT(0); // called from RRMeshCopy
		// efficient implementation would require another translation array
		unsigned midImportTriangle = INHERITED::getPostImportTriangle(preImportTriangle);
		for (unsigned post=0;post<ValidIndices;post++)
			if (ValidIndex[post]==midImportTriangle)
				return post;
		return RRMesh::UNDEFINED;
	}
	virtual RRMesh::PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const 
	// getPreImportVertex: postImportTriangle must be converted to midImportTriangle before calling inherited importer
	// getPostImportVertex:  no conversion needed
	{
		if (postImportTriangle>=ValidIndices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}
		unsigned midImportTriangle = ValidIndex[postImportTriangle];
		return INHERITED::getPreImportVertex(postImportVertex, midImportTriangle);
	}
	virtual void getTriangleBody(unsigned t, RRMesh::TriangleBody& out) const
	{
		RR_ASSERT(t<ValidIndices);
		INHERITED::getTriangleBody(ValidIndex[t],out);
	}

protected:
	unsigned*            ValidIndex; // may be uint16_t when indices<64k
	unsigned             ValidIndices;
};

} //namespace
