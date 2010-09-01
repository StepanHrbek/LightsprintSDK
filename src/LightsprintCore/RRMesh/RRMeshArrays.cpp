// --------------------------------------------------------------------------
// Mesh with direct access.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
	poolSize = 0;
	numTriangles = 0;
	triangle = NULL;
	numVertices = 0;
	position = NULL;
	normal = NULL;
	tangent = NULL;
	bitangent = NULL;
	// MeshArraysVBOs synces to mesh arrays pointer and version.
	// if someone deletes mesh arrays and creates new (different) one, pointer may be identical,
	// so make at least version different.
	version = rand();
}

RRMeshArrays::~RRMeshArrays()
{
	free(triangle);
}

// false = complete deallocate, mesh resized to 0,0
bool RRMeshArrays::resizeMesh(unsigned _numTriangles, unsigned _numVertices, const rr::RRVector<unsigned>* _texcoords, bool _tangents)
{
	// calculate new size in bytes
	unsigned newSize = _numTriangles*sizeof(Triangle) + _numVertices*((_tangents?4:2)*sizeof(RRVec3)+(_texcoords?_texcoords->size()*sizeof(RRVec2)+16:0))+100;

	// free/alloc
	char* pool = (char*)triangle;
	if (newSize>poolSize)
	{
		try
		{
			free(triangle);
			pool = newSize ? (char*)malloc(newSize) : NULL;
		}
		catch(...)
		{
			RRReporter::report(ERRO,"Allocation failed when resizing mesh to %d triangles, %d vertices.\n",_numTriangles,_numVertices);
			resizeMesh(0,0,NULL,false);
			return false;
		}
	}

	// remember new sizes
	poolSize = newSize;
	numTriangles = _numTriangles;
	numVertices = _numVertices;

	// fill pointers
	for (unsigned i=0;i<texcoord.size();i++)
	{
		texcoord[i] = NULL;
	}
	if (!newSize)
	{
		triangle = NULL;
		position = NULL;
		normal = NULL;
		tangent = NULL;
		bitangent = NULL;
	}
	else
	{
		#define ALIGN16(x) (((x)+15)&(~15))
		triangle = (Triangle*)pool; pool += ALIGN16(numTriangles*sizeof(Triangle));
		if (numVertices)
		{
			position = (RRVec3*)pool; pool += ALIGN16(numVertices*sizeof(RRVec3));
			normal = (RRVec3*)pool; pool += ALIGN16(numVertices*sizeof(RRVec3));
			if (_tangents)
			{
				tangent = (RRVec3*)pool; pool += ALIGN16(numVertices*sizeof(RRVec3));
				bitangent = (RRVec3*)pool; pool += ALIGN16(numVertices*sizeof(RRVec3));
			}
			else
			{
				tangent = NULL;
				bitangent = NULL;
			}
		}
		if (_texcoords)
		{
			for (unsigned i=0;i<_texcoords->size();i++)
			{
				if ((*_texcoords)[i]>1000000)
				{
					RRReporter::report(ERRO,"Texcoord numbers above million not supported (%d requested).\n",(*_texcoords)[i]);
					throw std::bad_alloc();
				}
				if ((*_texcoords)[i]>=texcoord.size())
				{
					texcoord.resize((*_texcoords)[i]+1,NULL);
				}
				texcoord[(*_texcoords)[i]] = (RRVec2*)pool; pool += ALIGN16(numVertices*sizeof(RRVec2));
			}
		}
	}

	version++;
	return true;
}

bool RRMeshArrays::reload(const RRMesh* _mesh, bool _indexed, const RRVector<unsigned>& _texcoords)
{
	if (!_mesh) return false;

	if (_indexed)
	{
		// alloc
		if (!resizeMesh(_mesh->getNumTriangles(),_mesh->getNumVertices(),&_texcoords, true)) // tangents are always included
		{
			return false;
		}

		// copy
		bool filledStatic[256];
		bool* filled = (numVertices<=256)?filledStatic:new bool[numVertices];
		#if defined(_MSC_VER) && (_MSC_VER<1500)
			#pragma omp parallel for // 2005 SP1 has broken if
		#else
			#pragma omp parallel for if(numVertices>1000)
		#endif
		for (int v=0;v<(int)numVertices;v++)
		{
			_mesh->getVertex(v,position[v]);
			filled[v] = false;
		}
		#if defined(_MSC_VER) && (_MSC_VER<1500)
			#pragma omp parallel for // 2005 SP1 has broken if
		#else
			#pragma omp parallel for if(numTriangles>200)
		#endif
		for (int t=0;t<(int)numTriangles;t++)
		{
			_mesh->getTriangle(t,triangle[t]);
			TriangleNormals normals;
			_mesh->getTriangleNormals(t,normals);
			for (unsigned v=0;v<3;v++)
			{
				if (triangle[t][v]<numVertices)
				{
					normal[triangle[t][v]] = normals.vertex[v].normal;
					tangent[triangle[t][v]] = normals.vertex[v].tangent;
					bitangent[triangle[t][v]] = normals.vertex[v].bitangent;
					filled[triangle[t][v]] = true;
				}
			}
			for (unsigned i=0;i<_texcoords.size();i++)
			{
				TriangleMapping mapping;
				_mesh->getTriangleMapping(t,mapping,_texcoords[i]);
				for (unsigned v=0;v<3;v++)
				{
					if (triangle[t][v]<numVertices)
					{
						texcoord[_texcoords[i]][triangle[t][v]] = mapping.uv[v];
					}
				}
			}
		}
		unsigned unfilled = 0;
		for (unsigned v=0;v<numVertices;v++)
		{
			if (!filled[v]) unfilled++;
		}
		if (filled!=filledStatic)
			delete[] filled;
		// happens too often, takes ages in scene with 23k meshes with unused vertices
		//if (unfilled)
		//	RRReporter::report(WARN,"RRMeshArrays::reload(): %d/%d unused vertices.\n",unfilled,numVertices);
	}
	else
	{
		// alloc
		if (!resizeMesh(_mesh->getNumTriangles(),_mesh->getNumTriangles()*3,&_texcoords, true)) // tangents are always included
		{
			return false;
		}

		// copy
		#if defined(_MSC_VER) && (_MSC_VER<1500)
			#pragma omp parallel for // 2005 SP1 has broken if
		#else
			#pragma omp parallel for if(numTriangles>200)
		#endif
		for (int t=0;t<(int)numTriangles;t++)
		{
			triangle[t][0] = 3*t+0;
			triangle[t][1] = 3*t+1;
			triangle[t][2] = 3*t+2;
			Triangle triangleT;
			_mesh->getTriangle(t,triangleT);
			TriangleNormals normals;
			_mesh->getTriangleNormals(t,normals);
			for (unsigned v=0;v<3;v++)
			{
				_mesh->getVertex(triangleT[v],position[t*3+v]);
				normal[t*3+v] = normals.vertex[v].normal;
				tangent[t*3+v] = normals.vertex[v].tangent;
				bitangent[t*3+v] = normals.vertex[v].bitangent;
			}
			for (unsigned i=0;i<_texcoords.size();i++)
			{
				TriangleMapping mapping;
				_mesh->getTriangleMapping(t,mapping,_texcoords[i]);
				for (unsigned v=0;v<3;v++)
				{
					texcoord[_texcoords[i]][t*3+v] = mapping.uv[v];
				}
			}
		}
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

void RRMeshArrays::getTriangleBody(unsigned t, TriangleBody& out) const
{
	RR_ASSERT(t<numTriangles);
	RR_ASSERT(triangle[t][0]<numVertices);
	RR_ASSERT(triangle[t][1]<numVertices);
	RR_ASSERT(triangle[t][2]<numVertices);
	out.vertex0 = position[triangle[t][0]];
	out.side1 = position[triangle[t][1]]-position[triangle[t][0]];
	out.side2 = position[triangle[t][2]]-position[triangle[t][0]];
}

void RRMeshArrays::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return;
	}
	RR_ASSERT(normal);
	if (tangent && bitangent)
	{
		for (unsigned v=0;v<3;v++)
		{
			RR_ASSERT(triangle[t][v]<numVertices);
			out.vertex[v].normal = normal[triangle[t][v]];
			out.vertex[v].tangent = tangent[triangle[t][v]];
			out.vertex[v].bitangent = bitangent[triangle[t][v]];
		}
	}
	else
	{
		for (unsigned v=0;v<3;v++)
		{
			RR_ASSERT(triangle[t][v]<numVertices);
			out.vertex[v].normal = normal[triangle[t][v]];
			out.vertex[v].buildBasisFromNormal();
		}
	}
}

bool RRMeshArrays::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (t>=numTriangles)
	{
		RR_ASSERT(0);
		return false;
	}
	if (channel>=texcoord.size() || !texcoord[channel])
	{
		// querying channel without data, this is legal
		return false;
	}
	for (unsigned v=0;v<3;v++)
	{
		RR_ASSERT(triangle[t][v]<numVertices);
		out.uv[v] = texcoord[channel][triangle[t][v]];
	}
	return true;
}

void RRMeshArrays::getUvChannels(rr::RRVector<unsigned>& out) const
{
	out.clear();
	rr::RRMesh::TriangleMapping mapping;
	for (unsigned i=0;i<texcoord.size();i++)
		if (texcoord[i])
			out.push_back(i);
}

struct AABBCache
{
	RRVec3 mini;
	RRVec3 maxi;
	RRVec3 center;
	unsigned version;
};

void RRMeshArrays::getAABB(RRVec3* _mini, RRVec3* _maxi, RRVec3* _center) const
{
	if (!aabbCache || aabbCache->version!=version)
	#pragma omp critical
	{
		if (!aabbCache)
			const_cast<RRMeshArrays*>(this)->aabbCache = new AABBCache; // hack: we write to const mesh. critical section makes it safe
		if (numVertices)
		{
			RRVec3 center = RRVec3(0);
			RRVec3 mini = RRVec3(1e37f); // with FLT_MAX/FLT_MIN, vs2008 produces wrong result
			RRVec3 maxi = RRVec3(-1e37f);
			for (unsigned v=0;v<numVertices;v++)
			{
				for (unsigned j=0;j<3;j++)
					if (_finite(position[v][j])) // filter out INF/NaN
					{
						mini[j] = RR_MIN(mini[j],position[v][j]);
						maxi[j] = RR_MAX(maxi[j],position[v][j]);
						center[j] += position[v][j];
					}
			}

			// fix negative size
			for (unsigned j=0;j<3;j++)
				if (mini[j]>maxi[j]) mini[j] = maxi[j] = 0;

			aabbCache->mini = mini;
			aabbCache->maxi = maxi;
			aabbCache->center = center/numVertices;
		}
		else
		{
			aabbCache->mini = RRVec3(0);
			aabbCache->maxi = RRVec3(0);
			aabbCache->center = RRVec3(0);
		}
		aabbCache->version = version;
		RR_ASSERT(aabbCache->mini.finite());
		RR_ASSERT(aabbCache->maxi.finite());
		RR_ASSERT(aabbCache->center.finite());
	}
	if (_mini) *_mini = aabbCache->mini;
	if (_maxi) *_maxi = aabbCache->maxi;
	if (_center) *_center = aabbCache->center;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

RRMeshArrays* RRMesh::createArrays(bool indexed, const RRVector<unsigned>& texcoords) const
{
	RRMeshArrays* importer = new RRMeshArrays();
	if (importer->reload(this,indexed,texcoords)) return importer;
	delete importer;
	return NULL;
}

} // namespace rr
