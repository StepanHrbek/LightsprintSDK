#pragma once

#include "Filter.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h> // qsort


namespace rrCollider
{


//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessVerticesFilter<INDEX>             - importer slow-filter that removes duplicate vertices
// RRLessVerticesImporter<IMPORTER,INDEX>  - importer fast-filter that removes duplicate vertices

int compareXyz(const void* elem1, const void* elem2);

template <class INDEX>
class RRLessVerticesFilter : public RRMeshFilter
{
public:
	RRLessVerticesFilter(const RRMeshImporter* original, float MAX_STITCH_DISTANCE)
		: RRMeshFilter(original)
	{
		// prepare translation arrays
		unsigned numVertices = importer->getNumVertices();
		INDEX tmp = numVertices;
		assert(tmp==numVertices);
		Dupl2Unique = new INDEX[numVertices];
		Unique2Dupl = new INDEX[numVertices];
		UniqueVertices = 0;

		// build temporary x-sorted array of vertices
		RRMeshImporter::Vertex* vertices = new RRMeshImporter::Vertex[numVertices];
		RRMeshImporter::Vertex** sortedVertices = new RRMeshImporter::Vertex*[numVertices];
		for(unsigned i=0;i<numVertices;i++)
		{
			importer->getVertex(i,vertices[i]);
			sortedVertices[i] = &vertices[i];
		}
		qsort(sortedVertices,numVertices,sizeof(RRMeshImporter::Vertex*),compareXyz);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for(unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			assert(d<numVertices);
			RRMeshImporter::Vertex& dfl = vertices[d];
			// test his distance against all already found unique vertices
			for(unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				RRMeshImporter::Vertex& ufl = vertices[Unique2Dupl[u]];
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
	virtual void getVertex(unsigned v, RRMeshImporter::Vertex& out) const
	{
		assert(v<UniqueVertices);
		importer->getVertex(Unique2Dupl[v],out);
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
		unsigned preImportTriangle = RRMeshImporter::getPreImportTriangle(postImportTriangle);
		if(preImportTriangle==UNDEFINED)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		RRMeshImporter::Triangle preImportVertices;
		importer->getTriangle(preImportTriangle,preImportVertices);
		if(Dupl2Unique[preImportVertices[0]]==postImportVertex) return preImportVertices[0];
		if(Dupl2Unique[preImportVertices[1]]==postImportVertex) return preImportVertices[1];
		if(Dupl2Unique[preImportVertices[2]]==postImportVertex) return preImportVertices[2];
		assert(0);

		// fast version
		return Unique2Dupl[postImportVertex];
	}
	virtual unsigned getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		if(preImportVertex>=importer->getNumVertices()) 
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		return Dupl2Unique[preImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
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
		RRMeshImporter::Vertex* vertices = new RRMeshImporter::Vertex[numVertices];
		RRMeshImporter::Vertex** sortedVertices = new RRMeshImporter::Vertex*[numVertices];
		for(unsigned i=0;i<numVertices;i++)
		{
			INHERITED::getVertex(i,vertices[i]);
			sortedVertices[i] = &vertices[i];
		}
		qsort(sortedVertices,numVertices,sizeof(RRMeshImporter::Vertex*),compareXyz);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for(unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			assert(d<numVertices);
			RRMeshImporter::Vertex& dfl = vertices[d];
			// test his distance against all already found unique vertices
			for(unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				RRMeshImporter::Vertex& ufl = vertices[Unique2Dupl[u]];
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
	virtual void getVertex(unsigned v, RRMeshImporter::Vertex& out) const
	{
		assert(v<UniqueVertices);
		assert(Unique2Dupl[v]<INHERITED::Vertices);
		assert(INHERITED::VBuffer);
		out = *(RRMeshImporter::Vertex*)(INHERITED::VBuffer+Unique2Dupl[v]*INHERITED::Stride);
	}
	virtual unsigned getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if(postImportVertex>=UniqueVertices)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMeshImporter::UNDEFINED;
		}

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned preImportTriangle = RRMeshImporter::getPreImportTriangle(postImportTriangle);
		if(preImportTriangle==RRMeshImporter::UNDEFINED)
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMeshImporter::UNDEFINED;
		}
		RRMeshImporter::Triangle preImportVertices;
		INHERITED::getTriangle(preImportTriangle,preImportVertices);
		if(Dupl2Unique[preImportVertices[0]]==postImportVertex) return preImportVertices[0];
		if(Dupl2Unique[preImportVertices[1]]==postImportVertex) return preImportVertices[1];
		if(Dupl2Unique[preImportVertices[2]]==postImportVertex) return preImportVertices[2];
		assert(0);

		// fast version
		return Unique2Dupl[postImportVertex];
	}
	virtual unsigned getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
	{
		if(preImportVertex>=INHERITED::Vertices) 
		{
			assert(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMeshImporter::UNDEFINED;
		}
		return Dupl2Unique[preImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMeshImporter::Triangle& out) const
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
