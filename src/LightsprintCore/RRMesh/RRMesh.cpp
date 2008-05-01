// --------------------------------------------------------------------------
// Mesh adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
		char intptr_t_incorrect [sizeof(intptr_t) == sizeof(void*)];
	};
#endif


namespace rr
{

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
	tangent = ortogonalTo(normal).normalized();
	bitangent = ortogonalTo(normal,tangent);
}

void RRMesh::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	unsigned numTriangles = getNumTriangles();
	if(t>=numTriangles)
	{
		RR_ASSERT(0);
		return;
	}
	RRMesh::TriangleBody tb;
	getTriangleBody(t,tb);
	out.vertex[0].normal = ortogonalTo(tb.side1,tb.side2).normalized();
	out.vertex[0].buildBasisFromNormal();
	out.vertex[1] = out.vertex[0];
	out.vertex[2] = out.vertex[0];
}

void RRMesh::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	unsigned numTriangles = getNumTriangles();
	if(t>=numTriangles)
	{
		RR_ASSERT(0);
		return;
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
	if(!spin)
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
}

bool RRMesh::getTrianglePlane(unsigned i, Plane& out) const
{
	Triangle t;
	getTriangle(i,t);
	Vertex v[3];
	getVertex(t[0],v[0]);
	getVertex(t[1],v[1]);
	getVertex(t[2],v[2]);
	out=normalized(ortogonalTo(v[1]-v[0],v[2]-v[0]));
	out[3]=-dot(v[0],out);
	return IS_VEC4(out);
}

// calculates triangle area from triangle vertices
RRReal calculateArea2(RRVec3 v0, RRVec3 v1, RRVec3 v2)
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
	RRReal area = calculateArea2(v[0],v[1],v[2]);
	return area;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMesh tools

void RRMesh::getAABB(RRVec3* amini, RRVec3* amaxi, RRVec3* acenter) const
{
	RRVec3 center = RRVec3(0);
	RRVec3 mini = RRVec3(1e37f); // with FLT_MAX/FLT_MIN, vs2008 produces wrong result
	RRVec3 maxi = RRVec3(-1e37f);
	unsigned numVertices = getNumVertices();
	for(unsigned i=0;i<numVertices;i++)
	{
		RRMesh::Vertex v;
		getVertex(i,v);
		for(unsigned j=0;j<3;j++)
		{
			center[j] += v[j];
			mini[j] = MIN(mini[j],v[j]);
			maxi[j] = MAX(maxi[j],v[j]);
		}
	}
	if(amini) *amini = mini;
	if(amaxi) *amaxi = maxi;
	if(acenter) *acenter = center/numVertices;
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
	}
	return NULL;
}

RRMesh* RRMesh::createIndexed(unsigned flags, Format vertexFormat, void* vertexBuffer, unsigned vertexCount, unsigned vertexStride, Format indexFormat, void* indexBuffer, unsigned indexCount, float vertexStitchMaxDistance)
{
	// each case instantiates new importer class
	// delete cases you don't need to save several kB of memory
	unsigned triangleCount = (flags&TRI_STRIP)?vertexCount-2:(vertexCount/3);
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
				}
				break;
			case TRI_STRIP:
				switch(indexFormat)
				{
					case UINT8: return new RRMeshIndexedTriStrip<uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRMeshIndexedTriStrip<uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRMeshIndexedTriStrip<uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount);
					case UINT16: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount);
					case UINT32: return new RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount);
				}
				break;
			case TRI_LIST|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_STRIP|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					case UINT8: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_LIST|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriList<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
			case TRI_STRIP|OPTIMIZED_TRIANGLES|OPTIMIZED_VERTICES:
				switch(indexFormat)
				{
					//!!! opacne, chci nejdriv zmergovat vertexy a az pak odstranit nove vznikle degeneraty
					case UINT8: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint8_t>,uint8_t>,uint8_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint8_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT16: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint16_t>,uint16_t>,uint16_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint16_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
					case UINT32: return new RRLessVerticesImporter<RRLessTrianglesImporter<RRMeshIndexedTriStrip<uint32_t>,uint32_t>,uint32_t>((char*)vertexBuffer,vertexCount,vertexStride,(uint32_t*)indexBuffer,indexCount,vertexStitchMaxDistance);
				}
				break;
		}
		break;
	}
	return NULL;
}

RRMesh* RRMesh::createTransformed(const RRMatrix3x4* transform) const
{
	if(!this) return NULL;
	//!!! az bude refcounting, muzu pri identite vracet this
	//return transform ? new RRTransformedMeshFilter(this,transform) : this;
	return new RRTransformedMeshFilter(this,transform);
}

const RRMesh* RRMesh::createMultiMesh(const RRMesh* const* meshes, unsigned numMeshes, bool fast)
{
	return fast ? RRMeshMultiFast::create(meshes,numMeshes) : RRMeshMultiSmall::create(meshes,numMeshes);
}

const RRMesh* RRMesh::createOptimizedVertices(float vertexStitchMaxDistance) const
{
	if(!this) return NULL;
	if(vertexStitchMaxDistance<0)
		return this;
	RRMesh* tmp = new RRLessVerticesFilter<unsigned>(this,vertexStitchMaxDistance);
	if(tmp->getNumVertices()<getNumVertices())
		return tmp;
	delete tmp;
	return this;
}

const RRMesh* RRMesh::createOptimizedTriangles() const
{
	if(!this) return NULL;
	RRMesh* tmp = new RRLessTrianglesFilter(this);
	if(tmp->getNumTriangles()<getNumTriangles())
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
	if(!this) return NULL;
	return new RRHidePreImportFilter(this);
}

unsigned RRMesh::verify() const
{
	unsigned numReports = 0;
	// numVertices
	unsigned numVertices = getNumVertices();
	if(numVertices==0 || numVertices>=10000000)
	{
		RRReporter::report(WARN,"getNumVertices()==%d.\n",numVertices);
		numReports++;
	}
	// numTriangles
	unsigned numTriangles = getNumTriangles();
	if(numTriangles==0 || numTriangles>=10000000)
	{
		RRReporter::report(WARN,"getNumTriangles()==%d.\n",numTriangles);
		numReports++;
	}
	// vertices
	for(unsigned i=0;i<numVertices;i++)
	{
		Vertex vertex;
		getVertex(i,vertex);
		if(!IS_VEC3(vertex))
		{
			RRReporter::report(ERRO,"getVertex(%d)==%f %f %f.\n",i,vertex[0],vertex[1],vertex[2]);
			numReports++;
		}
	}
	// triangles
	for(unsigned i=0;i<numTriangles;i++)
	{
		// triangle
		Triangle triangle;
		getTriangle(i,triangle);
		if(triangle.m[0]>=numVertices || triangle.m[1]>=numVertices || triangle.m[2]>=numVertices)
		{
			RRReporter::report(ERRO,"getTriangle(%d)==%d %d %d, getNumVertices()==%d.\n",i,triangle.m[0],triangle.m[1],triangle.m[2],numVertices);
			numReports++;
		}
		if(triangle.m[0]==triangle.m[1] || triangle.m[0]==triangle.m[2] || triangle.m[1]==triangle.m[2])
		{
			RRReporter::report(ERRO,"degen: getTriangle(%d)==%d %d %d\n",i,triangle.m[0],triangle.m[1],triangle.m[2]);
			numReports++;
		}

		// triangleBody
		TriangleBody triangleBody;
		getTriangleBody(i,triangleBody);
		if(!IS_VEC3(triangleBody.vertex0))
		{
			RRReporter::report(ERRO,"getTriangleBody(%d).vertex0==%f %f %f.\n",i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2]);
			numReports++;
		}
		if(!IS_VEC3(triangleBody.side1))
		{
			RRReporter::report(ERRO,"getTriangleBody(%d).side1==%f %f %f.\n",i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2]);
			numReports++;
		}
		if(!IS_VEC3(triangleBody.side2))
		{
			RRReporter::report(ERRO,"getTriangleBody(%d).side2==%f %f %f.\n",i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2]);
			numReports++;
		}
		if(!triangleBody.side1.length2())
		{
			RRReporter::report(WARN,"degen: getTriangleBody(%d).side1==0\n",i);
			numReports++;
		}
		if(!triangleBody.side2.length2())
		{
			RRReporter::report(WARN,"degen: getTriangleBody(%d).side2==0\n",i);
			numReports++;
		}

		// triangleBody equals triangle
		Vertex vertex[3];
		getVertex(triangle.m[0],vertex[0]);
		getVertex(triangle.m[1],vertex[1]);
		getVertex(triangle.m[2],vertex[2]);
		if(triangleBody.vertex0[0]!=vertex[0][0] || triangleBody.vertex0[1]!=vertex[0][1] || triangleBody.vertex0[2]!=vertex[0][2])
		{
			RRReporter::report(ERRO,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).vertex0==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangleBody.vertex0[0]-triangle[0],triangleBody.vertex0[1]-triangle[1],triangleBody.vertex0[2]-triangle[2]);
			numReports++;
		}
		float scale =
			fabs(vertex[1][0]-vertex[0][0])+fabs(triangleBody.side1[0])+
			fabs(vertex[1][1]-vertex[0][1])+fabs(triangleBody.side1[1])+
			fabs(vertex[1][2]-vertex[0][2])+fabs(triangleBody.side1[2]);
		float dif =
			fabs(vertex[1][0]-vertex[0][0]-triangleBody.side1[0])+
			fabs(vertex[1][1]-vertex[0][1]-triangleBody.side1[1])+
			fabs(vertex[1][2]-vertex[0][2]-triangleBody.side1[2]);
		if(dif>scale*1e-5)
		{
			RRReporter::report((dif>scale*0.01)?ERRO:WARN,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[1],vertex[1][0],vertex[1][1],vertex[1][2],
				vertex[1][0]-vertex[0][0]-triangleBody.side1[0],vertex[1][1]-vertex[0][1]-triangleBody.side1[1],vertex[1][2]-vertex[0][2]-triangleBody.side1[2]);
			numReports++;
		}
		scale =
			fabs(vertex[2][0]-vertex[0][0])+fabs(triangleBody.side2[0])+
			fabs(vertex[2][1]-vertex[0][1])+fabs(triangleBody.side2[1])+
			fabs(vertex[2][2]-vertex[0][2])+fabs(triangleBody.side2[2]);
		dif =
			fabs(vertex[2][0]-vertex[0][0]-triangleBody.side2[0])+
			fabs(vertex[2][1]-vertex[0][1]-triangleBody.side2[1])+
			fabs(vertex[2][2]-vertex[0][2]-triangleBody.side2[2]);
		if(dif>scale*1e-5)
		{
			RRReporter::report((dif>scale*0.01)?ERRO:WARN,"getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.\n",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[2],vertex[2][0],vertex[2][1],vertex[2][2],
				vertex[2][0]-vertex[0][0]-triangleBody.side2[0],vertex[2][1]-vertex[0][1]-triangleBody.side2[1],vertex[2][2]-vertex[0][2]-triangleBody.side2[2]);
			numReports++;
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
		bool notOrtogonal = false;
		for(unsigned j=0;j<3;j++)
		{
			if(!IS_VEC3(triangleNormals.vertex[j].normal)) nanOrInf = true;
			if(!IS_VEC3(triangleNormals.vertex[j].tangent)) nanOrInf = true;
			if(!IS_VEC3(triangleNormals.vertex[j].bitangent)) nanOrInf = true;
			if(fabs(size2(triangleNormals.vertex[j].normal)-1)>0.1f) denormalized = true;
			if(fabs(size2(triangleNormals.vertex[j].tangent)-1)>0.1f) denormalized = true;
			if(fabs(size2(triangleNormals.vertex[j].bitangent)-1)>0.1f) denormalized = true;
			if(size2(triangleNormals.vertex[j].normal-triangleNormalsFlat.vertex[0].normal)>2) badDirection = true;
			if(abs(dot(triangleNormals.vertex[j].normal,triangleNormals.vertex[j].tangent))>0.01f) notOrtogonal = true;
			if(abs(dot(triangleNormals.vertex[j].normal,triangleNormals.vertex[j].bitangent))>0.01f) notOrtogonal = true;
			if(abs(dot(triangleNormals.vertex[j].tangent,triangleNormals.vertex[j].bitangent))>0.01f) notOrtogonal = true;
		}
		if(nanOrInf)
		{
			RRReporter::report(ERRO,"getTriangleNormals(%d) are invalid, lengths of v0 norm/tang/bitang: %f %f %f, v1 %f %f %f, v2 %f %f %f.\n",i,
				size(triangleNormals.vertex[0].normal),size(triangleNormals.vertex[0].tangent),size(triangleNormals.vertex[0].bitangent),
				size(triangleNormals.vertex[1].normal),size(triangleNormals.vertex[1].tangent),size(triangleNormals.vertex[1].bitangent),
				size(triangleNormals.vertex[2].normal),size(triangleNormals.vertex[2].tangent),size(triangleNormals.vertex[2].bitangent));
		} 
		else
		if(denormalized)
		{
			RRReporter::report(WARN,"getTriangleNormals(%d) are denormalized, lengths of v0 norm/tang/bitang: %f %f %f, v1 %f %f %f, v2 %f %f %f.\n",i,
				size(triangleNormals.vertex[0].normal),size(triangleNormals.vertex[0].tangent),size(triangleNormals.vertex[0].bitangent),
				size(triangleNormals.vertex[1].normal),size(triangleNormals.vertex[1].tangent),size(triangleNormals.vertex[1].bitangent),
				size(triangleNormals.vertex[2].normal),size(triangleNormals.vertex[2].tangent),size(triangleNormals.vertex[2].bitangent));
		}
		else
		if(badDirection)
		{
			RRReporter::report(WARN,"getTriangleNormals(%d) point to back side.\n",i);
		}
		else
		if(notOrtogonal)
		{
			RRReporter::report(WARN,"getTriangleNormals(%d) are not ortogonal (normal,tangent,bitangent).\n",i);
		}

		// triangleMapping
		TriangleMapping triangleMapping;
		getTriangleMapping(i,triangleMapping);
		bool outOfRange = false;
		for(unsigned j=0;j<3;j++)
		{
			for(unsigned k=0;k<2;k++)
				if(triangleMapping.uv[j][k]<0 || triangleMapping.uv[j][k]>1)
					outOfRange = true;
		}
		if(outOfRange)
		{
			RRReporter::report(WARN,"getTriangleMapping(%d) out of range, %f %f  %f %f  %f %f.\n",
				i,
				triangleMapping.uv[0][0],triangleMapping.uv[0][1],
				triangleMapping.uv[1][0],triangleMapping.uv[1][1],
				triangleMapping.uv[2][0],triangleMapping.uv[2][1]
				);
		}
		
	}
	return numReports;
}

} //namespace
