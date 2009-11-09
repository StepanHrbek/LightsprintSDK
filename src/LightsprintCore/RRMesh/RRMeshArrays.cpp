// --------------------------------------------------------------------------
// Mesh with direct access.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstring>
#include "Lightsprint/RRMesh.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshArrays

RRMeshArrays::RRMeshArrays()
{
	numTriangles = 0;
	triangle = NULL;
	numVertices = 0;
	position = NULL;
	normal = NULL;
	tangent = NULL;
	bitangent = NULL;
	version = 0;
}

RRMeshArrays::~RRMeshArrays()
{
	resizeMesh(0,0);
}

bool RRMeshArrays::resizeMesh(unsigned _numTriangles, unsigned _numVertices)
{
	// delete old arrays
	RR_SAFE_DELETE_ARRAY(triangle);
	RR_SAFE_DELETE_ARRAY(position);
	RR_SAFE_DELETE_ARRAY(normal);
	RR_SAFE_DELETE_ARRAY(tangent);
	RR_SAFE_DELETE_ARRAY(bitangent);
	for (unsigned i=0;i<texcoord.size();i++)
	{
		RR_SAFE_DELETE_ARRAY(texcoord[i]);
	}

	// remember new sizes
	numTriangles = _numTriangles;
	numVertices = _numVertices;

	// allocate new arrays
	try
	{
		if (numTriangles)
		{
			triangle = new Triangle[numTriangles];
		}
		if (numVertices)
		{	
			position = new RRVec3[numVertices];
			normal = new RRVec3[numVertices];
			tangent = new RRVec3[numVertices];
			bitangent = new RRVec3[numVertices];
		}
	}
	catch(...)
	{
		RRReporter::report(ERRO,"Allocation failed when resizing mesh to %d triangles, %d vertices.\n",_numTriangles,_numVertices);
		resizeMesh(0,0);
		return false;
	}

	// done
	version++;
	return true;
}

bool RRMeshArrays::addTexcoord(unsigned _texcoord)
{
	if (_texcoord>1000000)
	{
		RRReporter::report(ERRO,"Texcoord numbers above million not supported (%d requested).\n",_texcoord);
		return false;
	}
	try
	{
		if (_texcoord>=texcoord.size())
		{
			texcoord.resize(_texcoord+1,NULL);
		}
		texcoord[_texcoord] = new RRVec2[numVertices];
	}
	catch(...)
	{
		RRReporter::report(ERRO,"Allocation failed when adding texcoord (%d vertices).\n",numVertices);
		return false;
	}
}

void RRMeshArrays::deleteTexcoord(unsigned _texcoord)
{
	if (_texcoord<texcoord.size())
		RR_SAFE_DELETE_ARRAY(texcoord[_texcoord]);
}

bool RRMeshArrays::save(const char* filename) const
{
	//!!!
	return false;
}

bool RRMeshArrays::reload(const char* filename)
{
	//!!!
	return false;
}

RRMeshArrays* RRMeshArrays::load(const char* filename)
{
	RRMeshArrays* mesh = new RRMeshArrays();
	if (!mesh->reload(filename))
		RR_SAFE_DELETE(mesh);
	return mesh;
}

bool RRMeshArrays::reload(const RRMesh* mesh, bool indexed, unsigned numChannels, unsigned* channelNumbers)
{
	if (!mesh) return false;

	// sanitize inputs
	if (!channelNumbers)
		numChannels = 0;

	if (indexed)
	{
		// alloc
		if (!resizeMesh(mesh->getNumTriangles(),mesh->getNumVertices()))
		{
			return false;
		}

		// copy
		bool* filled = new bool[numVertices];
		#pragma omp parallel for
		for (int v=0;v<(int)numVertices;v++)
		{
			mesh->getVertex(v,position[v]);
			filled[v] = false;
		}
		TriangleMapping* mapping = new TriangleMapping[numChannels];
		#pragma omp parallel for
		for (int t=0;t<(int)numTriangles;t++)
		{
			mesh->getTriangle(t,triangle[t]);
			TriangleNormals normals;
			mesh->getTriangleNormals(t,normals);
			for (unsigned i=0;i<numChannels;i++)
			{
				mesh->getTriangleMapping(t,mapping[i],channelNumbers[i]);
				if (!t)
					addTexcoord(channelNumbers[i]);
			}
			for (unsigned v=0;v<3;v++)
			{
				if (triangle[t][v]<numVertices)
				{
					normal[triangle[t][v]] = normals.vertex[v].normal;
					tangent[triangle[t][v]] = normals.vertex[v].tangent;
					bitangent[triangle[t][v]] = normals.vertex[v].bitangent;
					for (unsigned i=0;i<numChannels;i++)
					{
						texcoord[i][triangle[t][v]] = mapping[i].uv[v];
					}
					filled[triangle[t][v]] = true;
				}
			}
		}
		delete[] mapping;
		unsigned unfilled = 0;
		for (unsigned v=0;v<numVertices;v++)
		{
			if (!filled[v]) unfilled++;
		}
		delete[] filled;
		if (unfilled)
			RRReporter::report(WARN,"RRMeshArrays::reload(): %d/%d unused vertices.\n",unfilled,numVertices);
	}
	else
	{
		// alloc
		if (!resizeMesh(mesh->getNumTriangles(),mesh->getNumTriangles()*3))
		{
			return false;
		}

		// copy
		TriangleMapping* mapping = new TriangleMapping[numChannels];
		#pragma omp parallel for
		for (int t=0;t<(int)numTriangles;t++)
		{
			triangle[t][0] = 3*t+0;
			triangle[t][1] = 3*t+1;
			triangle[t][2] = 3*t+2;
			Triangle triangleT;
			mesh->getTriangle(t,triangleT);
			TriangleNormals normals;
			mesh->getTriangleNormals(t,normals);
			for (unsigned i=0;i<numChannels;i++)
			{
				mesh->getTriangleMapping(t,mapping[i],channelNumbers[i]);
				if (!t)
					addTexcoord(channelNumbers[i]);
			}
			for (unsigned v=0;v<3;v++)
			{
				mesh->getVertex(triangleT[v],position[t*3+v]);
				normal[t*3+v] = normals.vertex[v].normal;
				tangent[t*3+v] = normals.vertex[v].tangent;
				bitangent[t*3+v] = normals.vertex[v].bitangent;
				for (unsigned i=0;i<numChannels;i++)
				{
					texcoord[i][t*3+v] = mapping[i].uv[v];
				}
			}
		}
		delete[] mapping;
	}

	version++;

	return true;
}

unsigned RRMeshArrays::getNumVertices() const
{
	return numVertices;
}

void RRMeshArrays::getVertex(unsigned v, Vertex& out) const
{
	RR_ASSERT(v<numVertices);
	out = position[v];
}

unsigned RRMeshArrays::getNumTriangles() const
{
	return numTriangles;
}

void RRMeshArrays::getTriangle(unsigned t, Triangle& out) const
{
	RR_ASSERT(t<numTriangles);
	out = triangle[t];
}

void RRMeshArrays::getTriangleBody(unsigned i, TriangleBody& out) const
{
	Triangle t;
	RRMeshArrays::getTriangle(i,t);
	Vertex v[3];
	RRMeshArrays::getVertex(t[0],v[0]);
	RRMeshArrays::getVertex(t[1],v[1]);
	RRMeshArrays::getVertex(t[2],v[2]);
	out.vertex0=v[0];
	out.side1=v[1]-v[0];
	out.side2=v[2]-v[0];
}

void RRMeshArrays::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return;
	}
	RR_ASSERT(normal);
	RR_ASSERT(tangent);
	RR_ASSERT(bitangent);
	for (unsigned v=0;v<3;v++)
	{
		RR_ASSERT(triangle[t][v]<numTriangles);
		out.vertex[v].normal = normal[triangle[t][v]];
		out.vertex[v].tangent = tangent[triangle[t][v]];
		out.vertex[v].bitangent = bitangent[triangle[t][v]];
	}
}

bool RRMeshArrays::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return false;
	}
	if (channel>=texcoord.size())
	{
		RR_ASSERT(0);
		return false;
	}
	if (!texcoord[channel])
	{
		RR_ASSERT(0);
		return false;
	}
	for (unsigned v=0;v<3;v++)
	{
		RR_ASSERT(triangle[t][v]<numTriangles);
		out.uv[v] = texcoord[channel][triangle[t][v]];
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

RRMeshArrays* RRMesh::createArrays(bool indexed, unsigned numChannels, unsigned* channelNumbers) const
{
	RRMeshArrays* importer = new RRMeshArrays();
	if (importer->reload(this,indexed,numChannels,channelNumbers)) return importer;
	delete importer;
	return NULL;
}

/*bool RRMesh::save(char* filename)
{
	RRMeshArrays* arrays = createArrays();
	bool result = arrays && arrays->save(filename);
	RR_SAFE_DELETE(arrays);
	return result;
}*/

} // namespace rr
