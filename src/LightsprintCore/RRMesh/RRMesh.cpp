// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRMesh.h"
#include "RRMeshTriStrip.h"
#include "RRMeshTriList.h"
#include "RRMeshIndexedTriStrip.h"
#include "RRMeshIndexedTriList.h"
#include "RRMeshLessVertices.h"
#include "RRMeshLessTriangles.h"
#include "RRMeshFilterTransformed.h"
#include "RRMeshMulti.h"
#include "../RRMathPrivate.h"
#include <cassert>
#include <cstdio>

#ifndef _MSC_VER
	#include <stdint.h> // standard C99 header
#else
	typedef unsigned char uint8_t;
	typedef unsigned short uint16_t;
	typedef unsigned uint32_t;
	typedef unsigned long long uint64_t;
	static union
	{
		char uint8_t_incorrect  [sizeof( uint8_t) == 1];
		char uint16_t_incorrect [sizeof(uint16_t) == 2];
		char uint32_t_incorrect [sizeof(uint32_t) == 4];
		char uint64_t_incorrect [sizeof(uint64_t) == 8];
	};
#endif


namespace rr
{

////////////////////////////////////////////////////////////////////////////
//
// RRMesh::TriangleBody

bool RRMesh::TriangleBody::isNotDegenerated() const
{
	return IS_VEC3(vertex0) && IS_VEC3(side1) && IS_VEC3(side2) // NaN or INF
		&& side1.abs().sum() && side2.abs().sum() && side1!=side2; // degenerated
}


////////////////////////////////////////////////////////////////////////////
//
// RRMesh

void RRMesh::getTriangleBody(unsigned i, TriangleBody& out) const
{
	Triangle t;
	getTriangle(i,t);
	Vertex v[3];
	getVertex(t[0],v[0]);
	getVertex(t[1],v[1]);
	getVertex(t[2],v[2]);
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

void RRMesh::TangentBasis::buildBasisFromNormal()
{
	tangent = orthogonalTo(normal).normalized();
	bitangent = orthogonalTo(normal,tangent);
}

void RRMesh::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	unsigned numTriangles = getNumTriangles();
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return;
	}
	RRMesh::TriangleBody tb;
	getTriangleBody(t,tb);
	out.vertex[0].normal = orthogonalTo(tb.side1,tb.side2).normalized();
	out.vertex[0].buildBasisFromNormal();
	out.vertex[1] = out.vertex[0];
	out.vertex[2] = out.vertex[0];
}

bool RRMesh::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (channel!=0)
	{
		return false;
	}
	unsigned numTriangles = getNumTriangles();
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return false;
	}

	// row with 50% fill |/|/|/|/ 1234
	//out.uv[0][0] = 1.0f*t/numTriangles;
	//out.uv[0][1] = 0;
	//out.uv[1][0] = 1.0f*(t+1)/numTriangles;
	//out.uv[1][1] = 0;
	//out.uv[2][0] = 1.0f*t/numTriangles;
	//out.uv[2][1] = 1;

	// matrix with 50% fill  |/|/|/|/  1234
	//                       |/|/|/|/  5678
	//unsigned w = (unsigned)sqrtf((float)(numTriangles-1))+1;
	//unsigned h = (numTriangles-1)/w+1;
	//unsigned x = t%w;
	//unsigned y = t/w;
	//out.uv[0][0] = 1.0f*x/w;
	//out.uv[0][1] = 1.0f*y/h;
	//out.uv[1][0] = 1.0f*(x+1)/w;
	//out.uv[1][1] = 1.0f*y/h;
	//out.uv[2][0] = 1.0f*x/w;
	//out.uv[2][1] = 1.0f*(y+1)/h;

	// matrix with 90% fill |//||//||//||//| 12345678
	//                      |//||//||//||//| 9abcdefg
	unsigned spin = t&1;
	t /= 2;
	unsigned w = (unsigned)sqrtf((float)((numTriangles+1)/2-1))+1;
	unsigned h = ((numTriangles+1)/2-1)/w+1;
	unsigned x = t%w;
	unsigned y = t/w;
	static const float border = 0.1f;
	if (!spin)
	{
		out.uv[0][0] = (x+border)/w;
		out.uv[0][1] = (y+border)/h;
		out.uv[1][0] = (x+1-2.4f*border)/w;
		out.uv[1][1] = (y+border)/h;
		out.uv[2][0] = (x+border)/w;
		out.uv[2][1] = (y+1-2.4f*border)/h;
	}
	else
	{
		out.uv[0][0] = (x+1-border)/w;
		out.uv[0][1] = (y+1-border)/h;
		out.uv[1][0] = (x+2.4f*border)/w;
		out.uv[1][1] = (y+1-border)/h;
		out.uv[2][0] = (x+1-border)/w;
		out.uv[2][1] = (y+2.4f*border)/h;
	}
	return true;
}

bool RRMesh::getTrianglePlane(unsigned i, RRVec4& out) const
{
	Triangle t;
	getTriangle(i,t);
	Vertex v[3];
	getVertex(t[0],v[0]);
	getVertex(t[1],v[1]);
	getVertex(t[2],v[2]);
	out=normalized(orthogonalTo(v[1]-v[0],v[2]-v[0]));
	out[3]=-dot(v[0],out);
	return IS_VEC4(out);
}

// calculates triangle area from triangle vertices
static RRReal calculateArea(RRVec3 v0, RRVec3 v1, RRVec3 v2)
{
	RRReal a = size2(v1-v0);
	RRReal b = size2(v2-v0);
	RRReal c = size2(v2-v1);
	//return sqrt(2*b*c+2*c*a+2*a*b-a*a-b*b-c*c)/4;
	RRReal d = 2*b*c+2*c*a+2*a*b-a*a-b*b-c*c;
	return (d>0) ? sqrt(d)*0.25f : 0; // protection against evil rounding error
}

RRReal RRMesh::getTriangleArea(unsigned i) const
{
	Triangle t;
	getTriangle(i,t);
	Vertex v[3];
	getVertex(t[0],v[0]);
	getVertex(t[1],v[1]);
	getVertex(t[2],v[2]);
	RRReal area = calculateArea(v[0],v[1],v[2]);
	return area;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh tools

RRMesh::RRMesh()
{
	aabbCache = NULL;
}

RRMesh::~RRMesh()
{
	delete aabbCache;
}

void RRMesh::getAABB(RRVec3* _mini, RRVec3* _maxi, RRVec3* _center) const
{
	if (!aabbCache)
	#pragma omp critical
	{
		const_cast<RRMesh*>(this)->aabbCache = new RRVec3[3]; // hack: we write to const mesh. critical section makes it safe
		unsigned numVertices = getNumVertices();
		if (numVertices)
		{
			RRVec3 center = RRVec3(0);
			RRVec3 mini = RRVec3(1e37f); // with FLT_MAX/FLT_MIN, vs2008 produces wrong result
			RRVec3 maxi = RRVec3(-1e37f);
			for (unsigned i=0;i<numVertices;i++)
			{
				RRMesh::Vertex v;
				getVertex(i,v);
				for (unsigned j=0;j<3;j++)
					if (_finite(v[j])) // filter out INF/NaN
					{
						center[j] += v[j];
						mini[j] = RR_MIN(mini[j],v[j]);
						maxi[j] = RR_MAX(maxi[j],v[j]);
					}
			}

			// fix negative size
			for (unsigned j=0;j<3;j++)
				if (mini[j]>maxi[j]) mini[j] = maxi[j] = 0;

			aabbCache[0] = mini;
			aabbCache[1] = maxi;
			aabbCache[2] = center/numVertices;
		}
		else
		{
			aabbCache[0] = RRVec3(0);
			aabbCache[1] = RRVec3(0);
			aabbCache[2] = RRVec3(0);
		}
		RR_ASSERT(IS_VEC3(aabbCache[0]));
		RR_ASSERT(IS_VEC3(aabbCache[1]));
		RR_ASSERT(IS_VEC3(aabbCache[2]));
	}
	if (_mini) *_mini = aabbCache[0];
	if (_maxi) *_maxi = aabbCache[1];
	if (_center) *_center = aabbCache[2];
}

RRReal RRMesh::getAverageVertexDistance() const
{
	unsigned numVertices = getNumVertices();
	if (numVertices<2)
	{
		// no distances available, returning 1 probably won't hurt while 0 could
		return 1;
	}
	RRVec3 v1;
	getVertex(0,v1);
	RRReal distanceSum = 0;
	enum {TESTS=1000};
	for (unsigned i=0;i<TESTS;i++)
	{
		RRVec3 v2;
		getVertex(rand()%numVertices,v2);
		distanceSum += (v2-v1).length();
		v1 = v2;
	}
	return distanceSum/TESTS;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh instance factory

RRMesh* RRMesh::create(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride)
{
	// each case instantiates new importer class
	// delete cases you don't need to save several kB of memory
	flags &= ~(OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES); // optimizations are not implemented for non-indexed
	switch(vertexFormat)
	{
		case FLOAT32:
			switch(flags)
			{
				case TRI_LIST: return new RRMeshTriList((char*)vertexBuffer,vertexCount,vertexStride);
				case TRI_STRIP: return new RRMeshTriStrip((char*)vertexBuffer,vertexCount,vertexStride);
			}
			break;
		default:
			RRReporter::report(WARN,"Support for this vertex format was not implemented yet, please let us know you want it.\n");
			break;
	}
	return NULL;
}

RRMesh* RRMesh::createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance)
{
	// each case instantiates new importer class
	// delete cases you don't need to save several kB of memory
	switch(vertexFormat)
	{
	case FLOAT32:
		switch(flags)
		{
			case TRI_LIST:
				switch(indexFormat)
				{
					case UINT8: return new RRMeshIndexedTriList<uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRMeshIndexedTriList<uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRMeshIndexedTriList<uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_STRIP:
				switch(indexFormat)
				{
					case UINT8: return new RRMeshIndexedTriStrip<uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRMeshIndexedTriStrip<uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRMeshIndexedTriStrip<uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_LIST|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_STRIP|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					default:
					    RRReporter::report(WARN,"Support for this index format was not implemented yet, please let us know you want it.\n");
					    break;
				}
				break;
			default:
			    RRReporter::report(WARN,"Support for this combination of flags was not implemented yet, please let us know you want it.\n");
			    break;
		}
		break;
	default:
		RRReporter::report(WARN,"Support for this vertex format was not implemented yet, please let us know you want it.\n");
		break;
	}
	return NULL;
}

RRMesh* RRMesh::createTransformed(const RRMatrix3x4* transform) const
{
	if (!this) return NULL;
	//!!! az bude refcounting, muzu pri identite vracet this
	//return transform ? new RRTransformedMeshFilter(this,transform) : this;
	return new RRTransformedMeshFilter(this,transform);
}

const RRMesh* RRMesh::createMultiMesh(const RRMesh* const* meshes, unsigned numMeshes, bool fast)
{
	return fast ? RRMeshMultiFast::create(meshes,numMeshes) : RRMeshMultiSmall::create(meshes,numMeshes);
}

const RRMesh* RRMesh::createOptimizedVertices(float maxDistanceBetweenVerticesToStitch, float maxRadiansBetweenNormalsToStitch) const
{
	if (!this) return NULL;
	if (maxDistanceBetweenVerticesToStitch<0 || maxRadiansBetweenNormalsToStitch<0)
		return this;
	RRMesh* tmp = new RRLessVerticesFilter<unsigned>(this,maxDistanceBetweenVerticesToStitch,maxRadiansBetweenNormalsToStitch);
	if (tmp->getNumVertices()<getNumVertices())
		return tmp;
	delete tmp;
	return this;
}

const RRMesh* RRMesh::createOptimizedTriangles() const
{
	if (!this) return NULL;
	RRMesh* tmp = new RRLessTrianglesFilter(this);
	if (tmp->getNumTriangles()<getNumTriangles())
		return tmp;
	delete tmp;
	return this;
}

class RRHidePreImportFilter : public RRMeshFilter
{
public:
	RRHidePreImportFilter(const RRMesh* _original) : RRMeshFilter(_original) {}
	virtual PreImportNumber getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const {return PreImportNumber(0,postImportVertex);}
	virtual unsigned getPostImportVertex(PreImportNumber preImportVertex, PreImportNumber preImportTriangle) const {return preImportVertex.index;}
	virtual PreImportNumber getPreImportTriangle(unsigned postImportTriangle) const {return PreImportNumber(0,postImportTriangle);}
	virtual unsigned getPostImportTriangle(PreImportNumber preImportTriangle) const {return preImportTriangle.index;}
};

RRMesh* RRMesh::createVertexBufferRuler() const
{
	if (!this) return NULL;
	return new RRHidePreImportFilter(this);
}

// helper for checkConsistency(), similar to unsigned, but reports object number
class NumReports
{
public:
	NumReports(unsigned _meshNumber)
	{
		numReports = 0;
		meshNumber = _meshNumber;
		indented = false;
	}
	~NumReports()
	{
		if (indented)
			RRReporter::indent(-2);
	}
	void operator ++(int i)
	{
		if (!numReports && meshNumber!=UINT_MAX)
		{
			RRReporter::report(WARN,"Inconsistency found in mesh %d:\n",meshNumber);
			RRReporter::indent(2);
			indented = true;
		}
		numReports++;
	}
	operator unsigned() const
	{
		return numReports;
	}
private:
	unsigned numReports;
	unsigned meshNumber;
	bool indented;
};

unsigned RRMesh::checkConsistency(unsigned lightmapTexcoord, unsigned meshNumber) const
{
	NumReports numReports(meshNumber); // unsigned numReports = 0; would work too, although without reporting meshNumber
	// numVertices
	unsigned numVertices = getNumVertices();
	if (numVertices==0 || numVertices>=10000000)
	{
		numReports++;
		RRReporter::report(WARN,"getNumVertices()==%d.\n",numVertices);
	}
	// numTriangles
	unsigned numTriangles = getNumTriangles();
	if (numTriangles==0 || numTriangles>=10000000)
	{
		numReports++;
		RRReporter::report(WARN,"getNumTriangles()==%d.\n",numTriangles);
	}
	// vertices
	for (unsigned i=0;i<numVertices;i++)
	{
		Vertex vertex;
		getVertex(i,vertex);
		if (!IS_VEC3(vertex))
		{
			numReports++;
			RRReporter::report(ERRO,"getVertex(%d)==%f %f %f.\n",i,vertex[0],vertex[1],vertex[2]);
		}
	}
	// triangles
	for (unsigned i=0;i<numTriangles;i++)
	{
		// triangle
		Triangle triangle;
		getTriangle(i,triangle);
		if (triangle.m[0]>=numVertices || triangle.m[1]>=numVertices || triangle.m[2]>=numVertices)
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangle(%d)==%d %d %d, getNumVertices()==%d.\n",i,triangle.m[0],triangle.m[1],triangle.m[2],numVertices);
		}
		if (triangle.m[0]==triangle.m[1] || triangle.m[0]==triangle.m[2] || triangle.m[1]==triangle.m[2])
		{
			numReports++;
			RRReporter::report(ERRO,"degen: getTriangle(%d)==%d %d %d\n",i,triangle.m[0],triangle.m[1],triangle.m[2]);
		}

		// triangleBody
		TriangleBody triangleBody;
		getTriangleBody(i,triangleBody);
		if (!IS_VEC3(triangleBody.vertex0))
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangleBody(%d).vertex0==%f %f %f.\n",i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2]);
		}
		if (!IS_VEC3(triangleBody.side1))
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangleBody(%d).side1==%f %f %f.\n",i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2]);
		}
		if (!IS_VEC3(triangleBody.side2))
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangleBody(%d).side2==%f %f %f.\n",i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2]);
		}
		if (!triangleBody.side1.length2())
		{
			numReports++;
			RRReporter::report(WARN,"degen: getTriangleBody(%d).side1==0\n",i);
		}
		if (!triangleBody.side2.length2())
		{
			numReports++;
			RRReporter::report(WARN,"degen: getTriangleBody(%d).side2==0\n",i);
		}

		// triangleBody equals triangle
		Vertex vertex[3];
		getVertex(triangle.m[0],vertex[0]);
		getVertex(triangle.m[1],vertex[1]);
		getVertex(triangle.m[2],vertex[2]);
		if (triangleBody.vertex0[0]!=vertex[0][0] || triangleBody.vertex0[1]!=vertex[0][1] || triangleBody.vertex0[2]!=vertex[0][2])
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).vertex0==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangleBody.vertex0[0]-triangle[0],triangleBody.vertex0[1]-triangle[1],triangleBody.vertex0[2]-triangle[2]);
		}
		float scale =
			fabs(vertex[1][0]-vertex[0][0])+fabs(triangleBody.side1[0])+
			fabs(vertex[1][1]-vertex[0][1])+fabs(triangleBody.side1[1])+
			fabs(vertex[1][2]-vertex[0][2])+fabs(triangleBody.side1[2]);
		float dif =
			fabs(vertex[1][0]-vertex[0][0]-triangleBody.side1[0])+
			fabs(vertex[1][1]-vertex[0][1]-triangleBody.side1[1])+
			fabs(vertex[1][2]-vertex[0][2]-triangleBody.side1[2]);
		if (dif>scale*1e-5)
		{
			numReports++;
			RRReporter::report((dif>scale*0.01)?ERRO:WARN,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[1],vertex[1][0],vertex[1][1],vertex[1][2],
				vertex[1][0]-vertex[0][0]-triangleBody.side1[0],vertex[1][1]-vertex[0][1]-triangleBody.side1[1],vertex[1][2]-vertex[0][2]-triangleBody.side1[2]);
		}
		scale =
			fabs(vertex[2][0]-vertex[0][0])+fabs(triangleBody.side2[0])+
			fabs(vertex[2][1]-vertex[0][1])+fabs(triangleBody.side2[1])+
			fabs(vertex[2][2]-vertex[0][2])+fabs(triangleBody.side2[2]);
		dif =
			fabs(vertex[2][0]-vertex[0][0]-triangleBody.side2[0])+
			fabs(vertex[2][1]-vertex[0][1]-triangleBody.side2[1])+
			fabs(vertex[2][2]-vertex[0][2]-triangleBody.side2[2]);
		if (dif>scale*1e-5)
		{
			numReports++;
			RRReporter::report((dif>scale*0.01)?ERRO:WARN,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[2],vertex[2][0],vertex[2][1],vertex[2][2],
				vertex[2][0]-vertex[0][0]-triangleBody.side2[0],vertex[2][1]-vertex[0][1]-triangleBody.side2[1],vertex[2][2]-vertex[0][2]-triangleBody.side2[2]);
		}

		//!!! pre/post import

		// triangleNormals
		TriangleNormals triangleNormals;
		TriangleNormals triangleNormalsFlat;
		getTriangleNormals(i,triangleNormals);
		RRMesh::getTriangleNormals(i,triangleNormalsFlat);
		bool nanOrInf = false;
		bool denormalized = false;
		bool badDirection = false;
		bool notOrthogonal = false;
		for (unsigned j=0;j<3;j++)
		{
			if (!IS_VEC3(triangleNormals.vertex[j].normal)) nanOrInf = true;
			if (!IS_VEC3(triangleNormals.vertex[j].tangent)) nanOrInf = true;
			if (!IS_VEC3(triangleNormals.vertex[j].bitangent)) nanOrInf = true;
			if (fabs(size2(triangleNormals.vertex[j].normal)-1)>0.1f) denormalized = true;
			if (fabs(size2(triangleNormals.vertex[j].tangent)-1)>0.1f) denormalized = true;
			if (fabs(size2(triangleNormals.vertex[j].bitangent)-1)>0.1f) denormalized = true;
			if (size2(triangleNormals.vertex[j].normal-triangleNormalsFlat.vertex[0].normal)>2) badDirection = true;
			if (fabs(dot(triangleNormals.vertex[j].normal,triangleNormals.vertex[j].tangent))>0.01f) notOrthogonal = true;
			if (fabs(dot(triangleNormals.vertex[j].normal,triangleNormals.vertex[j].bitangent))>0.01f) notOrthogonal = true;
			if (fabs(dot(triangleNormals.vertex[j].tangent,triangleNormals.vertex[j].bitangent))>0.01f) notOrthogonal = true;
		}
		if (nanOrInf)
		{
			numReports++;
			RRReporter::report(ERRO,"getTriangleNormals(%d) are invalid, lengths of v0 norm/tang/bitang: %f %f %f, v1 %f %f %f, v2 %f %f %f.\n",i,
				size(triangleNormals.vertex[0].normal),size(triangleNormals.vertex[0].tangent),size(triangleNormals.vertex[0].bitangent),
				size(triangleNormals.vertex[1].normal),size(triangleNormals.vertex[1].tangent),size(triangleNormals.vertex[1].bitangent),
				size(triangleNormals.vertex[2].normal),size(triangleNormals.vertex[2].tangent),size(triangleNormals.vertex[2].bitangent));
		} 
		else
		if (denormalized)
		{
			numReports++;
			RRReporter::report(WARN,"getTriangleNormals(%d) are denormalized, lengths of v0 norm/tang/bitang: %f %f %f, v1 %f %f %f, v2 %f %f %f.\n",i,
				size(triangleNormals.vertex[0].normal),size(triangleNormals.vertex[0].tangent),size(triangleNormals.vertex[0].bitangent),
				size(triangleNormals.vertex[1].normal),size(triangleNormals.vertex[1].tangent),size(triangleNormals.vertex[1].bitangent),
				size(triangleNormals.vertex[2].normal),size(triangleNormals.vertex[2].tangent),size(triangleNormals.vertex[2].bitangent));
		}
		else
		if (badDirection)
		{
			numReports++;
			RRReporter::report(WARN,"getTriangleNormals(%d) point to back side.\n",i);
		}
		else
		if (notOrthogonal)
		{
			// they are orthogonal in UE3, but they are not in Gamebryo
			// RRReporter::report(WARN,"getTriangleNormals(%d) are not orthogonal (normal,tangent,bitangent).\n",i);
		}

		// triangleMapping
		if (lightmapTexcoord!=UINT_MAX)
		{
			TriangleMapping triangleMapping;
			getTriangleMapping(i,triangleMapping,lightmapTexcoord);
			bool outOfRange = false;
			for (unsigned j=0;j<3;j++)
			{
				for (unsigned k=0;k<2;k++)
					if (triangleMapping.uv[j][k]<-0.0f || triangleMapping.uv[j][k]>1)
						outOfRange = true;
			}
			if (outOfRange)
			{
				numReports++;
				RRReporter::report(WARN,"Unwrap getTriangleMapping(%d,,%d) out of range, %f %f  %f %f  %f %f.\n",
					i,lightmapTexcoord,
					triangleMapping.uv[0][0],triangleMapping.uv[0][1],
					triangleMapping.uv[1][0],triangleMapping.uv[1][1],
					triangleMapping.uv[2][0],triangleMapping.uv[2][1]
					);
			}
		}
		
	}
	return numReports;
}

unsigned RRMesh::getNumPreImportVertices() const 
{
	unsigned numPreImportVertices = 0;
	unsigned numPostTriangles = getNumTriangles();
	for (unsigned postTriangle=0;postTriangle<numPostTriangles;postTriangle++)
	{
		Triangle postVertices;
		getTriangle(postTriangle,postVertices);
		for (unsigned i=0;i<3;i++)
		{
			PreImportNumber preVertex = getPreImportVertex(postVertices[i],postTriangle);
			if (preVertex.index!=UINT_MAX)
				numPreImportVertices = RR_MAX(numPreImportVertices,preVertex.index+1);
		}
	}
	return numPreImportVertices;
}


// Moved to file with exceptions enabled:
// RRReal RRMesh::findGroundLevel() const

} //namespace
