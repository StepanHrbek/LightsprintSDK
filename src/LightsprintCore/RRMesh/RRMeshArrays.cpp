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
	for (unsigned i=0;i<MAX_CHANNELS;i++)
	{
		uv[i] = NULL;
		uvChannel[i] = 0;
	}
	version = 0;
}

RRMeshArrays::~RRMeshArrays()
{
	setNumTriangles(0);
	setNumVertices(0,0);
}

bool RRMeshArrays::setNumTriangles(unsigned _numTriangles)
{
	RR_SAFE_DELETE_ARRAY(triangle);
	numTriangles = _numTriangles;
	if (numTriangles)
	{
		try
		{
			triangle = new Triangle[numTriangles];
		}
		catch(...)
		{
			RRReporter::report(ERRO,"Allocation failed when resizing mesh to %d triangles.\n",_numTriangles);
			setNumTriangles(0);
			setNumVertices(0,0); // free also vertices, keeping them with 0 triangles would be safe, but memory wasting
			return false;
		}
	}
	version++;
	return true;
}

bool RRMeshArrays::setNumVertices(unsigned _numVertices, unsigned _numChannels)
{
	RR_SAFE_DELETE_ARRAY(position);
	RR_SAFE_DELETE_ARRAY(normal);
	RR_SAFE_DELETE_ARRAY(tangent);
	RR_SAFE_DELETE_ARRAY(bitangent);
	for (unsigned i=0;i<MAX_CHANNELS;i++)
	{
		RR_SAFE_DELETE_ARRAY(uv[i]);
	}
	numVertices = _numVertices;
	if (numVertices)
	{	
		if (_numChannels>MAX_CHANNELS)
		{
			RRReporter::report(WARN,"RRMeshArrays::setNumVertices(,%d): max supported is %d.\n",_numChannels,MAX_CHANNELS);
			_numChannels = MAX_CHANNELS;
		}
		try
		{
			position = new RRVec3[numVertices];
			normal = new RRVec3[numVertices];
			tangent = new RRVec3[numVertices];
			bitangent = new RRVec3[numVertices];
			for (unsigned i=0;i<_numChannels;i++)
			{
				uv[i] = new RRVec2[numVertices];
			}
		}
		catch(...)
		{
			RRReporter::report(ERRO,"Allocation failed when resizing mesh to %d vertices.\n",_numVertices);
			setNumVertices(0,0);
			setNumTriangles(0); // free also triangles, keeping them with 0 vertices would make mesh invalid
			return false;
		}
	}
	version++;
	return true;
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
	if (numChannels>MAX_CHANNELS)
	{
		RRReporter::report(WARN,"RRMeshArrays::reload(): only %d out of %d uv channels will be accessible.\n",MAX_CHANNELS,numChannels);
		numChannels = MAX_CHANNELS;
	}

	if (indexed)
	{
		// alloc
		if (!setNumTriangles(mesh->getNumTriangles()) || !setNumVertices(mesh->getNumVertices(),numChannels))
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
		#pragma omp parallel for
		for (int t=0;t<(int)numTriangles;t++)
		{
			mesh->getTriangle(t,triangle[t]);
			TriangleNormals normals;
			mesh->getTriangleNormals(t,normals);
			TriangleMapping mapping[MAX_CHANNELS];
			for (unsigned i=0;i<numChannels;i++)
			{
				mesh->getTriangleMapping(t,mapping[i],channelNumbers[i]);
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
						uv[i][triangle[t][v]] = mapping[i].uv[v];
					}
					filled[triangle[t][v]] = true;
				}
			}
		}
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
		if (!setNumTriangles(mesh->getNumTriangles()) || !setNumVertices(mesh->getNumTriangles()*3,numChannels))
		{
			return false;
		}

		// copy
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
			TriangleMapping mapping[MAX_CHANNELS];
			for (unsigned i=0;i<numChannels;i++)
			{
				mesh->getTriangleMapping(t,mapping[i],channelNumbers[i]);
			}
			for (unsigned v=0;v<3;v++)
			{
				mesh->getVertex(triangleT[v],position[t*3+v]);
				normal[t*3+v] = normals.vertex[v].normal;
				tangent[t*3+v] = normals.vertex[v].tangent;
				bitangent[t*3+v] = normals.vertex[v].bitangent;
				for (unsigned i=0;i<numChannels;i++)
				{
					uv[i][t*3+v] = mapping[i].uv[v];
				}
			}
		}
	}

	for (unsigned i=0;i<numChannels;i++)
	{
		uvChannel[i] = channelNumbers[i];
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
	for (unsigned ch=0;ch<MAX_CHANNELS;ch++)
	{
		if (uvChannel[ch]==channel && uv[ch])
		{
			for (unsigned v=0;v<3;v++)
			{
				RR_ASSERT(triangle[t][v]<numTriangles);
				out.uv[v] = uv[ch][triangle[t][v]];
			}
			return true;
		}
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
