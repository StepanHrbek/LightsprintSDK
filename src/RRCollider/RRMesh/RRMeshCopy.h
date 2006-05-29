#pragma once

#include "RRMeshFilter.h"
#include "../geometry.h" // MAX

#include <assert.h>
#include <vector>


namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCopy
//
// For creating fully owned copies of importer,
// saving to file and loading from file.

class RRMeshCopy : public RRMesh
{
public:
	RRMeshCopy()
	{
	}

	// Save importer to file.
	// Check importer consistency during save so we don't have to store redundant data.
	bool save(const char* filename)
	{
		//!!!
		return false;
	}

	// Load importer from file.
	bool load(const char* filename)
	{
		//!!!
		return false;
	}

	// Load importer from another importer (create copy).
	bool load(RRMesh* importer)
	{
		if(!importer) return false;

		// copy vertices
		unsigned numVertices = importer->getNumVertices();
		postImportVertices.resize(numVertices);
		for(unsigned i=0;i<numVertices;i++)
		{
			Vertex v;
			importer->getVertex(i,v);
			postImportVertices[i] = v;
		}

		// copy triangles
		unsigned numPreImportTriangles = 0;
		unsigned numTriangles = importer->getNumTriangles();
		postImportTriangles.resize(numTriangles);
		for(unsigned postImportTriangle=0;postImportTriangle<numTriangles;postImportTriangle++)
		{
			PostImportTriangle t;
			// copy getPreImportTriangle
			t.preImportTriangle = importer->getPreImportTriangle(postImportTriangle);
			assert(t.preImportTriangle!=UNDEFINED);
			// copy getTriangle
			importer->getTriangle(postImportTriangle,t.postImportTriangleVertices);
			// copy getPreImportVertex
			for(unsigned j=0;j<3;j++)
			{
				assert(t.postImportTriangleVertices[j]!=UNDEFINED);
				t.preImportTriangleVertices[j] = importer->getPreImportVertex(t.postImportTriangleVertices[j],postImportTriangle);
			}
			numPreImportTriangles = MAX(numPreImportTriangles,t.preImportTriangle+1);
			postImportTriangles[postImportTriangle] = t;
		}

		// handle sparse PreImport numbering
		if(numPreImportTriangles>=0x100000 && numPreImportTriangles>10*numTriangles) //!!! fudge numbers
		{
			// CopyMesh is very inefficient copying MultiMesh 
			// and other meshes with extremely sparse PreImport numbering
			// let's rather fail
			return false;
		}

		// copy triangleBodies
		for(unsigned i=0;i<numTriangles;i++)
		{
			//TriangleBody t;
			//importer->getTriangleBody(i,t);
			//!!!... check that getTriangleBody returns numbers consistent with getVertex/getTriangle
		}

		// copy getPostImportTriangle
		pre2postImportTriangles.resize(numPreImportTriangles);
		for(unsigned preImportTriangle=0;preImportTriangle<numPreImportTriangles;preImportTriangle++)
		{
			unsigned postImportTriangle = importer->getPostImportTriangle(preImportTriangle);
			assert(postImportTriangle==UNDEFINED || postImportTriangle<postImportTriangles.size());
			pre2postImportTriangles[preImportTriangle] = postImportTriangle;
		}

		// check that importer equals this in all queries
		//!!!...

		return true;
	}

	virtual ~RRMeshCopy()
	{
	}

	// vertices
	virtual unsigned     getNumVertices() const
	{
		return static_cast<unsigned>(postImportVertices.size());
	}
	virtual void         getVertex(unsigned v, Vertex& out) const
	{
		assert(v<postImportVertices.size());
		out = postImportVertices[v];
	}

	// triangles
	virtual unsigned     getNumTriangles() const
	{
		return static_cast<unsigned>(postImportTriangles.size());
	}
	virtual void         getTriangle(unsigned t, Triangle& out) const
	{
		assert(t<postImportTriangles.size());
		out = postImportTriangles[t].postImportTriangleVertices;
	}
	virtual void         getTriangleBody(unsigned i, TriangleBody& out) const
	{
		Triangle t;
		RRMeshCopy::getTriangle(i,t);
		Vertex v[3];
		RRMeshCopy::getVertex(t[0],v[0]);
		RRMeshCopy::getVertex(t[1],v[1]);
		RRMeshCopy::getVertex(t[2],v[2]);
		out.vertex0[0]=v[0][0];
		out.vertex0[1]=v[0][1];
		out.vertex0[2]=v[0][2];
		out.side1[0]=v[1][0]-v[0][0];
		out.side1[1]=v[1][1]-v[0][1];
		out.side1[2]=v[1][2]-v[0][2];
		out.side2[0]=v[2][0]-v[0][0];
		out.side2[1]=v[2][1]-v[0][1];
		out.side2[2]=v[2][2]-v[0][2];
	}

	// preimport/postimport conversions
	virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if(postImportVertex>=postImportVertices.size())
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		if(postImportTriangle>=postImportTriangles.size())
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[0]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[0];
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[1]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[1];
		if(postImportTriangles[postImportTriangle].postImportTriangleVertices[2]==postImportVertex) return postImportTriangles[postImportTriangle].preImportTriangleVertices[2];
		assert(0);
		return UNDEFINED;
	}
	virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		unsigned postImportTriangle = getPostImportTriangle(preImportTriangle);
		if(postImportTriangle==UNDEFINED) 
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[0]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[0];
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[1]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[1];
		if(postImportTriangles[postImportTriangle].preImportTriangleVertices[2]==preImportVertex) return postImportTriangles[postImportTriangle].postImportTriangleVertices[2];
		assert(0);
		return UNDEFINED;
	}
	virtual unsigned     getPreImportTriangle(unsigned postImportTriangle) const
	{
		if(postImportTriangle>=postImportTriangles.size())
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		return postImportTriangles[postImportTriangle].preImportTriangle;
	}
	virtual unsigned     getPostImportTriangle(unsigned preImportTriangle) const 
	{
		if(preImportTriangle>=pre2postImportTriangles.size())
		{
			// it is allowed by rules
			// it happens with copy of multimesh and possibly with other sparse preimport numberings
			return UNDEFINED;
		}
		return pre2postImportTriangles[preImportTriangle];
	}

protected:
	std::vector<Vertex>   postImportVertices;
	struct PostImportTriangle
	{
		unsigned preImportTriangle;
		Triangle postImportTriangleVertices;
		Triangle preImportTriangleVertices;
	};
	std::vector<PostImportTriangle> postImportTriangles;
	std::vector<unsigned> pre2postImportTriangles; // sparse(inverse of preImportTriangles) -> vector is mostly ok, but multimesh probably needs map
};


} //namespace
