#pragma once

#include "RRMeshFilter.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h> // qsort


namespace rrCollider
{

//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessVerticesFilter<INDEX> - importer slow-filter that removes duplicate vertices

int compareXyz(const void* elem1, const void* elem2);

template <class INDEX>
class RRLessVerticesFilter : public RRMeshFilter
{
public:
	RRLessVerticesFilter(const RRMesh* original, float MAX_STITCH_DISTANCE)
		: RRMeshFilter(original)
	{
		assert(MAX_STITCH_DISTANCE>=0); // negative value would remove no vertices -> no improvement
		// prepare translation arrays
		unsigned numVertices = importer->getNumVertices();
		INDEX tmp = numVertices;
		assert(tmp==numVertices);
		Dupl2Unique = new INDEX[numVertices];
		Unique2Dupl = new INDEX[numVertices];
		UniqueVertices = 0;

		// build temporary x-sorted array of vertices
		RRMesh::Vertex* vertices = new RRMesh::Vertex[numVertices];
		RRMesh::Vertex** sortedVertices = new RRMesh::Vertex*[numVertices];
		for(unsigned i=0;i<numVertices;i++)
		{
			importer->getVertex(i,vertices[i]);
			sortedVertices[i] = &vertices[i];
		}
		qsort(sortedVertices,numVertices,sizeof(RRMesh::Vertex*),compareXyz);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for(unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			assert(d<numVertices);
			RRMesh::Vertex& dfl = vertices[d];
			// test his distance against all already found unique vertices
			for(unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				RRMesh::Vertex& ufl = vertices[Unique2Dupl[u]];
				// stop when testing too x-distant vertex (all close vertices were already tested)
				//#define CLOSE(a,b) ((a)==(b))
				#define CLOSE(a,b) (fabs((a)-(b))<=MAX_STITCH_DISTANCE)
				if(!CLOSE(dfl[0],ufl[0])) break;
				// this is candidate for stitching, do final test
				if(CLOSE(dfl[0],ufl[0]) && CLOSE(dfl[1],ufl[1]) && CLOSE(dfl[2],ufl[2])) 
				{
					Dupl2Unique[d] = u;
					goto dupl;
				}
			}
			Unique2Dupl[UniqueVertices] = d;
			Dupl2Unique[d] = UniqueVertices++;
dupl:;
		}

		// delete temporaries
		delete[] vertices;
		delete[] sortedVertices;
	}
	~RRLessVerticesFilter()
	{
		delete[] Unique2Dupl;
		delete[] Dupl2Unique;
	}

	virtual unsigned getNumVertices() const
	{
		return UniqueVertices;
	}
	virtual void getVertex(unsigned postImportVertex, RRMesh::Vertex& out) const
	{
		assert(postImportVertex<UniqueVertices);
		unsigned midImportVertex = Unique2Dupl[postImportVertex];
		importer->getVertex(midImportVertex,out);
	}
	virtual unsigned getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if(postImportVertex>=UniqueVertices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned midImportTriangle = postImportTriangle; // triangle numbering is not changed by us
		if(midImportTriangle==UNDEFINED)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		RRMesh::Triangle midImportVertices;
		importer->getTriangle(midImportTriangle,midImportVertices);
		for(unsigned v=0;v<3;v++)
		{
			if(Dupl2Unique[midImportVertices[v]]==postImportVertex)
				return importer->getPreImportVertex(midImportVertices[v],midImportTriangle);
		}
		assert(0); // nastalo kdyz byla chyba v LessTrianglesFilter a postNumber se nezkonvertilo na midNumber

		// fast version
		return Unique2Dupl[postImportVertex];
	}
	virtual unsigned getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		unsigned midImportVertex = importer->getPostImportVertex(preImportVertex,preImportTriangle);
		if(midImportVertex>=importer->getNumVertices()) 
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		return Dupl2Unique[midImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		importer->getTriangle(t,out);
		out[0] = Dupl2Unique[out[0]]; assert(out[0]<UniqueVertices);
		out[1] = Dupl2Unique[out[1]]; assert(out[1]<UniqueVertices);
		out[2] = Dupl2Unique[out[2]]; assert(out[2]<UniqueVertices);
	}

protected:
	INDEX*               Unique2Dupl;    // small -> big number translation, one small number translates to one of many suitable big numbers
	INDEX*               Dupl2Unique;    // big -> small number translation
	unsigned             UniqueVertices; // small number of unique vertices, UniqueVertices<=INHERITED.getNumVertices()
};

//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessVerticesImporter<IMPORTER,INDEX> - importer fast-filter that removes duplicate vertices

template <class INHERITED, class INDEX>
class RRLessVerticesImporter : public INHERITED
{
public:
	RRLessVerticesImporter(char* vbuffer, unsigned avertices, unsigned stride, INDEX* ibuffer, unsigned indices, float MAX_STITCH_DISTANCE)
		: INHERITED(vbuffer,avertices,stride,ibuffer,indices)
	{
		INDEX tmp = avertices;
		assert(tmp==avertices);
		// prepare translation arrays
		unsigned numVertices = avertices;
		Dupl2Unique = new INDEX[numVertices];
		Unique2Dupl = new INDEX[numVertices];
		UniqueVertices = 0;

		// build temporary x-sorted array of vertices
		RRMesh::Vertex* vertices = new RRMesh::Vertex[numVertices];
		RRMesh::Vertex** sortedVertices = new RRMesh::Vertex*[numVertices];
		for(unsigned i=0;i<numVertices;i++)
		{
			INHERITED::getVertex(i,vertices[i]);
			sortedVertices[i] = &vertices[i];
		}
		qsort(sortedVertices,numVertices,sizeof(RRMesh::Vertex*),compareXyz);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for(unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			assert(d<numVertices);
			RRMesh::Vertex& dfl = vertices[d];
			// test his distance against all already found unique vertices
			for(unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				RRMesh::Vertex& ufl = vertices[Unique2Dupl[u]];
				// stop when testing too x-distant vertex (all close vertices were already tested)
				//#define CLOSE(a,b) ((a)==(b))
#define CLOSE(a,b) (fabs((a)-(b))<=MAX_STITCH_DISTANCE)
				if(!CLOSE(dfl[0],ufl[0])) break;
				// this is candidate for stitching, do final test
				if(CLOSE(dfl[0],ufl[0]) && CLOSE(dfl[1],ufl[1]) && CLOSE(dfl[2],ufl[2])) 
				{
					Dupl2Unique[d] = u;
					goto dupl;
				}
			}
			Unique2Dupl[UniqueVertices] = d;
			Dupl2Unique[d] = UniqueVertices++;
dupl:;
		}

		// delete temporaries
		delete[] vertices;
		delete[] sortedVertices;
	}
	~RRLessVerticesImporter()
	{
		delete[] Unique2Dupl;
		delete[] Dupl2Unique;
	}

	virtual unsigned getNumVertices() const
	{
		return UniqueVertices;
	}
	virtual void getVertex(unsigned postImportVertex, RRMesh::Vertex& out) const
	{
		assert(postImportVertex<UniqueVertices);
		unsigned midImportVertex = Unique2Dupl[postImportVertex];
		//assert(midImportVertex<INHERITED::Vertices);
		//assert(INHERITED::VBuffer);
		INHERITED::getVertex(midImportVertex,out);
		//out = *(RRMesh::Vertex*)(INHERITED::VBuffer+midImportVertex*INHERITED::Stride);
	}
	virtual unsigned getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if(postImportVertex>=UniqueVertices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned midImportTriangle = postImportTriangle; // triangle numbering is not changed by us
		if(midImportTriangle==RRMesh::UNDEFINED)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		RRMesh::Triangle midImportVertices;
		INHERITED::getTriangle(midImportTriangle,midImportVertices);
		for(unsigned v=0;v<3;v++)
		{
			if(Dupl2Unique[midImportVertices[v]]==postImportVertex)
				return INHERITED::getPreImportVertex(midImportVertices[v],midImportTriangle);
		}
		assert(0);

		// fast version
		return Unique2Dupl[postImportVertex];
	}
	virtual unsigned getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		unsigned midImportVertex = INHERITED::getPostImportVertex(preImportVertex,preImportTriangle);
		if(midImportVertex>=INHERITED::getNumVertices()) 
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		return Dupl2Unique[midImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		INHERITED::getTriangle(t,out);
		out[0] = Dupl2Unique[out[0]]; assert(out[0]<UniqueVertices);
		out[1] = Dupl2Unique[out[1]]; assert(out[1]<UniqueVertices);
		out[2] = Dupl2Unique[out[2]]; assert(out[2]<UniqueVertices);
	}

protected:
	INDEX*               Unique2Dupl;    // small -> big number translation, one small number translates to one of many suitable big numbers
	INDEX*               Dupl2Unique;    // big -> small number translation
	unsigned             UniqueVertices; // small number of unique vertices, UniqueVertices<=INHERITED.getNumVertices()
};

} //namespace
