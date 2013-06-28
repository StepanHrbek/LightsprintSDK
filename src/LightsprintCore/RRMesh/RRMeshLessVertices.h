// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#pragma once

#include "RRMeshFilter.h"
#include <cmath>
#include <cstdlib> // qsort

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Importer filters
//
// RRLessVerticesFilter<INDEX> - importer slow-filter that removes unused and duplicated vertices
//
// Differences in positions, normals and selected uv channels are limited by parameters.
// Uvs in not selected channels may differ arbitrarily.

int __cdecl comparePositionNormal(const void* elem1, const void* elem2);
int __cdecl comparePositionX(const void* elem1, const void* elem2);

template <class INDEX>
class RRLessVerticesFilter : public RRMeshFilter
{
public:
	RRLessVerticesFilter(const RRMesh* original, float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch, float maxDistanceBetweenUvsToStitch, const RRVector<unsigned>* texcoords)
		: RRMeshFilter(original)
	{
		// negative values are legal, filter can still remove unused vertices
		//RR_ASSERT(maxDistanceBetweenVerticesToStitch>=0);
		bool mergingPossible = maxDistanceBetweenVerticesToStitch>=0 && maxRadiansBetweenNormalsToStitch>=0;

		// prepare translation arrays
		unsigned numVertices = inherited->getNumVertices();
		unsigned numTriangles = inherited->getNumTriangles();
		INDEX tmp = numVertices;
		RR_ASSERT(tmp==numVertices);
		Dupl2Unique = new (std::nothrow) INDEX[numVertices]; // if it fails, allocating "vertices" will fail too, it is handled below
		Unique2Dupl = new (std::nothrow) INDEX[numVertices];
		UniqueVertices = 0;

		bool preserveUvs = texcoords && texcoords->size();

		// build temporary position.x sorted array of vertices
		enum {MAX_UVS=4};
		struct Vertex
		{
			RRVec3 position;
			RRVec3 normal;
			RRReal area;  // if (area<0) {found different n: normal=sum(n*area);} else {all n identical: normal=n; area=sum of areas;}
			RRVec2 uv[MAX_UVS];
			bool used;
		};
		Vertex* vertices = new (std::nothrow) Vertex[numVertices];
		if (!vertices)
		{
			RR_LIMITED_TIMES(10,RRReporter::report(WARN,"Mesh not processed, allocating %s failed(1).\n",RRReporter::bytesToString(sizeof(Vertex)*numVertices)));
			UniqueVertices = numVertices; // pretend that no vertices were removed, RRMesh::createOptimizedVertices() will delete us immediately
			return;
		}
		memset(vertices,0,sizeof(Vertex)*numVertices);
		Vertex** sortedVertices = new (std::nothrow) Vertex*[numVertices];
		if (!sortedVertices)
		{
			delete[] vertices;
			RR_LIMITED_TIMES(10,RRReporter::report(WARN,"Mesh not processed, allocating %s failed(2).\n",RRReporter::bytesToString(sizeof(Vertex*)*numVertices)));
			UniqueVertices = numVertices; // pretend that no vertices were removed, RRMesh::createOptimizedVertices() will delete us immediately
			return;
		}
		for (unsigned i=0;i<numVertices;i++)
		{
			sortedVertices[i] = &vertices[i];
			// load position
			inherited->getVertex(i,vertices[i].position);
			// mark vertex as unused
			vertices[i].used = false;
		}
		for (unsigned i=0;i<numTriangles;i++)
		{
			// load normal
			RRMesh::Triangle t;
			RRMesh::TriangleNormals tn;
			inherited->getTriangle(i,t);
			inherited->getTriangleNormals(i,tn);
			// Two triangles may share vertex, yet have different normals.
			// Simply using one of them fails in wop_padattic where tiny triangle has opposite normal, stitching fails if we pick the wrong normal.
			// Therefore we weight normals by triangle area.
			RRReal area = inherited->getTriangleArea(i);
			for (unsigned j=0;j<3;j++)
			{
				// old code: accumulate normal*area
				//vertices[t[j]].normal += tn.vertex[0].normal*area;
				// new code: prevent the most common rounding error by storing unmodified normal and accumulating area
				//           when different normals are encountered, revert to original less accurate approach
				//           this fixes error where some vertices are not stitched because of false difference in normals
				Vertex& v = vertices[t[j]];
				const RRVec3& normal = tn.vertex[j].normal;
				if (v.area>=0)
				{
					if (!v.area || normal==v.normal)
					{
						// all normals still the same
						v.normal = normal;
						v.area += area;
					}
					else
					{
						// just found first different normal
						v.normal = v.normal*v.area + normal*area;
						v.area = -1;
					}
				}
				else
				{
					// we already had different normal before
					v.normal += normal*area;
				}
				// mark vertex as used
				v.used = true;
			}
			
			// load uvs
			if (preserveUvs)
			{
				TriangleMapping tm;
				unsigned uvs = 0;
				for (unsigned j=0;j<texcoords->size();j++)
				{
					inherited->getTriangleMapping(i,tm,(*texcoords)[j]);
					vertices[t[0]].uv[uvs] = tm.uv[0];
					vertices[t[1]].uv[uvs] = tm.uv[1];
					vertices[t[2]].uv[uvs] = tm.uv[2];
					uvs++;
					if (uvs==MAX_UVS) break;
				}
			}
		}
		for (unsigned i=0;i<numVertices;i++)
		{
			vertices[i].normal.normalizeSafe(); // normalize normals
		}
		// sorting by both position and normal is bit slower, but it makes vertices nicely ordered,
		// which is good for reproducibility and debugging (we can log vertices and then compare logs,
		// there will be a few insertions and deletions instead of tons of swaps)
		// sorting by position.x should give the same results, with different vertex order
		if (mergingPossible)
			qsort(sortedVertices,numVertices,sizeof(Vertex*),comparePositionNormal);
		// expensive expressions moved out of for cycle
		bool stitchOnlyIdenticalNormals = maxRadiansBetweenNormalsToStitch==0;
		float minNormalDotNormalToStitch = cos(maxRadiansBetweenNormalsToStitch);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for (unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			RR_ASSERT(d<numVertices);
			Vertex& dfl = vertices[d];
			// skip unused vertices
			if (!dfl.used)
			{
				Dupl2Unique[d] = UINT_MAX; // probably can be left uninitialized, should never be accessed
				goto dupl;
			}
			// test his distance against all already found unique vertices
			if (mergingPossible)
			for (unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				Vertex& ufl = vertices[Unique2Dupl[u]];
				// skip unused vertices
				if (!ufl.used)
					continue;
				// stop when testing too x-distant vertex (all close vertices were already tested)
				//#define CLOSE(a,b) ((a)==(b))
				#define CLOSEPOS(i) (fabs((dfl.position[i])-(ufl.position[i]))<=maxDistanceBetweenVerticesToStitch)
				if (!CLOSEPOS(0)) break;
				// this is candidate for stitching, do final test
				if (CLOSEPOS(0) && CLOSEPOS(1) && CLOSEPOS(2))
				{
					if ( (stitchOnlyIdenticalNormals && dfl.normal==ufl.normal)
						|| (!stitchOnlyIdenticalNormals && dfl.normal.dot(ufl.normal)>=minNormalDotNormalToStitch) ) // normals must be normalized here
					{
						if (!preserveUvs
							|| (maxDistanceBetweenUvsToStitch==0 && !memcmp(dfl.uv,ufl.uv,sizeof(ufl.uv)))
							#define CLOSEU(i,j) (fabs((dfl.uv[i][j])-(ufl.uv[i][j]))<=maxDistanceBetweenUvsToStitch)
							#define CLOSEUV(i) (CLOSEU(i,0) && CLOSEU(i,1))
							//#define CLOSEUV(i) ((dfl.uv[i]-ufl.uv[i]).length2()<=maxDistanceBetweenUvsToStitch*maxDistanceBetweenUvsToStitch)
							|| (CLOSEUV(0) && CLOSEUV(1) && CLOSEUV(2) && CLOSEUV(3))) // MAX_UVS=4
						{
							Dupl2Unique[d] = u;
							goto dupl;
						}
					}
				}
				#undef CLOSEUV
				#undef CLOSEU
				#undef CLOSEPOS
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
		RR_ASSERT(postImportVertex<UniqueVertices);
		unsigned midImportVertex = Unique2Dupl[postImportVertex];
		inherited->getVertex(midImportVertex,out);
	}
	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if (postImportVertex>=UniqueVertices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return PreImportNumber(UNDEFINED,UNDEFINED);
		}

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned midImportTriangle = postImportTriangle; // triangle numbering is not changed by us
		if (midImportTriangle==UNDEFINED)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return PreImportNumber(UNDEFINED,UNDEFINED);
		}
		RRMesh::Triangle midImportVertices;
		inherited->getTriangle(midImportTriangle,midImportVertices);
		for (unsigned v=0;v<3;v++)
		{
			if (Dupl2Unique[midImportVertices[v]]==postImportVertex)
				return inherited->getPreImportVertex(midImportVertices[v],midImportTriangle);
		}

		// invalid inputs (nastalo kdyz byla chyba v LessTrianglesFilter a postNumber se nezkonvertilo na midNumber)
		RR_ASSERT(0);
		return PreImportNumber(UNDEFINED,UNDEFINED);
	}
	virtual unsigned getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const
	{
		unsigned midImportVertex = inherited->getPostImportVertex(preImportVertex,preImportTriangle);
		if (midImportVertex>=inherited->getNumVertices()) 
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return UNDEFINED;
		}
		return Dupl2Unique[midImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		inherited->getTriangle(t,out);
		out[0] = Dupl2Unique[out[0]]; RR_ASSERT(out[0]<UniqueVertices);
		out[1] = Dupl2Unique[out[1]]; RR_ASSERT(out[1]<UniqueVertices);
		out[2] = Dupl2Unique[out[2]]; RR_ASSERT(out[2]<UniqueVertices);
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
		RR_ASSERT(tmp==avertices);
		// prepare translation arrays
		unsigned numVertices = avertices;
		Dupl2Unique = new INDEX[numVertices];
		Unique2Dupl = new INDEX[numVertices];
		UniqueVertices = 0;

		// build temporary x-sorted array of vertices
		RRMesh::Vertex* vertices = new RRMesh::Vertex[numVertices];
		RRMesh::Vertex** sortedVertices = new RRMesh::Vertex*[numVertices];
		for (unsigned i=0;i<numVertices;i++)
		{
			INHERITED::getVertex(i,vertices[i]);
			sortedVertices[i] = &vertices[i];
		}
		qsort(sortedVertices,numVertices,sizeof(RRMesh::Vertex*),comparePositionX);

		// find duplicates and stitch, fill translation arrays
		// for each vertex
		for (unsigned ds=0;ds<numVertices;ds++) // ds=index into sortedVertices
		{
			unsigned d = (unsigned)(sortedVertices[ds]-vertices); // d=prefiltered/importer vertex, index into Dupl2Unique
			RR_ASSERT(d<numVertices);
			RRMesh::Vertex& dfl = vertices[d];
			// test his distance against all already found unique vertices
			for (unsigned u=UniqueVertices;u--;) // u=filtered/our vertex, index into Unique2Dupl
			{
				RRMesh::Vertex& ufl = vertices[Unique2Dupl[u]];
				// stop when testing too x-distant vertex (all close vertices were already tested)
				//#define CLOSE(a,b) ((a)==(b))
				#define CLOSE(a,b) (fabs((a)-(b))<=MAX_STITCH_DISTANCE)
				if (!CLOSE(dfl[0],ufl[0])) break;
				// this is candidate for stitching, do final test
				if (CLOSE(dfl[0],ufl[0]) && CLOSE(dfl[1],ufl[1]) && CLOSE(dfl[2],ufl[2])) 
				{
					Dupl2Unique[d] = u;
					goto dupl;
				}
				#undef CLOSE
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
		RR_ASSERT(postImportVertex<UniqueVertices);
		unsigned midImportVertex = Unique2Dupl[postImportVertex];
		//RR_ASSERT(midImportVertex<INHERITED::Vertices);
		//RR_ASSERT(INHERITED::VBuffer);
		INHERITED::getVertex(midImportVertex,out);
		//out = *(RRMesh::Vertex*)(INHERITED::VBuffer+midImportVertex*INHERITED::Stride);
	}
	virtual RRMesh::PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
	{
		if (postImportVertex>=UniqueVertices)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}

		// exact version
		// postImportVertex is not full information, because one postImportVertex translates to many preImportVertex
		// use postImportTriangle to fully specify which one preImportVertex to return
		unsigned midImportTriangle = postImportTriangle; // triangle numbering is not changed by us
		if (midImportTriangle==RRMesh::UNDEFINED)
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
		}
		RRMesh::Triangle midImportVertices;
		INHERITED::getTriangle(midImportTriangle,midImportVertices);
		for (unsigned v=0;v<3;v++)
		{
			if (Dupl2Unique[midImportVertices[v]]==postImportVertex)
				return INHERITED::getPreImportVertex(midImportVertices[v],midImportTriangle);
		}

		// invalid inputs
		RR_ASSERT(0);
		return RRMesh::PreImportNumber(RRMesh::UNDEFINED,RRMesh::UNDEFINED);
	}
	virtual unsigned getPostImportVertex(RRMesh::PreImportNumber preImportVertex, RRMesh::PreImportNumber preImportTriangle) const
	{
		unsigned midImportVertex = INHERITED::getPostImportVertex(preImportVertex,preImportTriangle);
		if (midImportVertex>=INHERITED::getNumVertices()) 
		{
			RR_ASSERT(0); // it is allowed by rules, but also interesting to know when it happens
			return RRMesh::UNDEFINED;
		}
		return Dupl2Unique[midImportVertex];
	}
	virtual void getTriangle(unsigned t, RRMesh::Triangle& out) const
	{
		INHERITED::getTriangle(t,out);
		out[0] = Dupl2Unique[out[0]]; RR_ASSERT(out[0]<UniqueVertices);
		out[1] = Dupl2Unique[out[1]]; RR_ASSERT(out[1]<UniqueVertices);
		out[2] = Dupl2Unique[out[2]]; RR_ASSERT(out[2]<UniqueVertices);
	}

protected:
	INDEX*               Unique2Dupl;    // small -> big number translation, one small number translates to one of many suitable big numbers
	INDEX*               Dupl2Unique;    // big -> small number translation
	unsigned             UniqueVertices; // small number of unique vertices, UniqueVertices<=INHERITED.getNumVertices()
};

} //namespace
