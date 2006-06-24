#include "RRMesh.h"
#include "RRMeshTriStrip.h"
#include "RRMeshTriList.h"
#include "RRMeshIndexedTriStrip.h"
#include "RRMeshIndexedTriList.h"
#include "RRMeshLessVertices.h"
#include "RRMeshLessTriangles.h"
#include "RRMeshMulti.h"
#include "../RRMath/RRMathPrivate.h"

#include <cassert>
#include <cstdio>
#ifdef _MSC_VER
	#include "../RRCollider/stdint.h"
#else
	#include <stdint.h> // sizes of imported vertex/index arrays
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
	RRReal a=size2(v1-v0);
	RRReal b=size2(v2-v0);
	RRReal c=size2(v2-v1);
	return sqrt(2*b*c+2*c*a+2*a*b-a*a-b*b-c*c)/4;
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

RRMesh* RRMesh::createMultiMesh(RRMesh* const* meshes, unsigned numMeshes)
{
	return RRMeshMulti::create(meshes,numMeshes);
}

RRMesh* RRMesh::createOptimizedVertices(float vertexStitchMaxDistance)
{
	if(vertexStitchMaxDistance<0) return this;
	return new RRLessVerticesFilter<unsigned>(this,vertexStitchMaxDistance);
}

RRMesh* RRMesh::createOptimizedTriangles()
{
	return new RRLessTrianglesFilter(this);
}

unsigned RRMesh::verify(Reporter* reporter, void* context)
{
	unsigned numReports = 0;
	static char msg[500];
	// numVertices
	unsigned numVertices = getNumVertices();
	if(numVertices>=10000000)
	{
		sprintf(msg,"Warning: getNumVertices()==%d.",numVertices);
		reporter(msg,context);
		numReports++;
	}
	// numTriangles
	unsigned numTriangles = getNumTriangles();
	if(numTriangles>=10000000)
	{
		sprintf(msg,"Warning: getNumTriangles()==%d.",numTriangles);
		reporter(msg,context);
		numReports++;
	}
	// vertices
	for(unsigned i=0;i<numVertices;i++)
	{
		Vertex vertex;
		getVertex(i,vertex);
		if(!IS_VEC3(vertex))
		{
			sprintf(msg,"Error: getVertex(%d)==%f %f %f.",i,vertex[0],vertex[1],vertex[2]);
			reporter(msg,context);
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
			sprintf(msg,"Error: getTriangle(%d)==%d %d %d, getNumVertices()==%d.",i,triangle.m[0],triangle.m[1],triangle.m[2],numVertices);
			reporter(msg,context);
			numReports++;
		}
		// triangleBody
		TriangleBody triangleBody;
		getTriangleBody(i,triangleBody);
		if(!IS_VEC3(triangleBody.vertex0))
		{
			sprintf(msg,"Error: getTriangleBody(%d).vertex0==%f %f %f.",i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2]);
			reporter(msg,context);
			numReports++;
		}
		if(!IS_VEC3(triangleBody.side1))
		{
			sprintf(msg,"Error: getTriangleBody(%d).side1==%f %f %f.",i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2]);
			reporter(msg,context);
			numReports++;
		}
		if(!IS_VEC3(triangleBody.side2))
		{
			sprintf(msg,"Error: getTriangleBody(%d).side2==%f %f %f.",i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2]);
			reporter(msg,context);
			numReports++;
		}
		// triangleBody equals triangle
		Vertex vertex[3];
		getVertex(triangle.m[0],vertex[0]);
		getVertex(triangle.m[1],vertex[1]);
		getVertex(triangle.m[2],vertex[2]);
		if(triangleBody.vertex0[0]!=vertex[0][0] || triangleBody.vertex0[1]!=vertex[0][1] || triangleBody.vertex0[2]!=vertex[0][2])
		{
			sprintf(msg,"Error: getTriangle(%d)==%d %d %d, getTriangleBody(%d).vertex0==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.vertex0[0],triangleBody.vertex0[1],triangleBody.vertex0[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangleBody.vertex0[0]-triangle[0],triangleBody.vertex0[1]-triangle[1],triangleBody.vertex0[2]-triangle[2]);
			reporter(msg,context);
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
			sprintf(msg,"%s: getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.",
				(dif>scale*0.01)?"Error":"Warning",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side1[0],triangleBody.side1[1],triangleBody.side1[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[1],vertex[1][0],vertex[1][1],vertex[1][2],
				vertex[1][0]-vertex[0][0]-triangleBody.side1[0],vertex[1][1]-vertex[0][1]-triangleBody.side1[1],vertex[1][2]-vertex[0][2]-triangleBody.side1[2]);
			reporter(msg,context);
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
			sprintf(msg,"%s: getTriangle(%d)==%d %d %d, getTriangleBody(%d).side1==%f %f %f, getVertex(%d)==%f %f %f, getVertex(%d)==%f %f %f, delta=%f %f %f.",
				(dif>scale*0.01)?"Error":"Warning",
				i,triangle.m[0],triangle.m[1],triangle.m[2],
				i,triangleBody.side2[0],triangleBody.side2[1],triangleBody.side2[2],
				triangle.m[0],vertex[0][0],vertex[0][1],vertex[0][2],
				triangle.m[2],vertex[2][0],vertex[2][1],vertex[2][2],
				vertex[2][0]-vertex[0][0]-triangleBody.side2[0],vertex[2][1]-vertex[0][1]-triangleBody.side2[1],vertex[2][2]-vertex[0][2]-triangleBody.side2[2]);
			reporter(msg,context);
			numReports++;
		}
		//!!! pre/post import
	}
	return numReports;
}

} //namespace
