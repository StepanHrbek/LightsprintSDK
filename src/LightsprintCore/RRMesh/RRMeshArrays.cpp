// --------------------------------------------------------------------------
// Mesh with direct access.
// Copyright (c) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstring>
#include "Lightsprint/RRMesh.h"
#include "../RRMathPrivate.h"

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
bool RRMeshArrays::resizeMesh(unsigned _numTriangles, unsigned _numVertices, const RRVector<unsigned>* _texcoords, bool _tangents, bool _preserveContents)
{
	// calculate new size in bytes
	unsigned newSize = _numTriangles*sizeof(Triangle) + _numVertices*((_tangents?4:2)*sizeof(RRVec3)+(_texcoords?_texcoords->size()*sizeof(RRVec2)+16:0))+100;

	// preserve contents 1.
	RRVector<unsigned> tmp;
	RRMeshArrays* backup = _preserveContents ? createArrays(true,_texcoords?*_texcoords:tmp,_tangents) : NULL;

	// free/alloc
	char* pool = (char*)triangle;
	if (newSize>poolSize)
	{
		free(triangle);
		pool = newSize ? (char*)malloc(newSize) : NULL;
		if (newSize && !pool)
		{
			RRReporter::report(ERRO,"Allocation failed when resizing mesh to %d triangles, %d vertices.\n",_numTriangles,_numVertices);
			resizeMesh(0,0,NULL,false,false);
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

	// preserve contents 2.
	if (backup)
	{
		reload(backup,true,_texcoords?*_texcoords:tmp,_tangents);
		delete backup;
	}

	version++;
	return true;
}

bool RRMeshArrays::reload(const RRMesh* _mesh, bool _indexed, const RRVector<unsigned>& _texcoords, bool _tangents)
{
	if (!_mesh) return false;

	if (_indexed)
	{
		// alloc
		if (!resizeMesh(_mesh->getNumTriangles(),_mesh->getNumVertices(),&_texcoords,_tangents,false))
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
					if (_tangents)
					{
						tangent[triangle[t][v]] = normals.vertex[v].tangent;
						bitangent[triangle[t][v]] = normals.vertex[v].bitangent;
					}
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
		if (!resizeMesh(_mesh->getNumTriangles(),_mesh->getNumTriangles()*3,&_texcoords,_tangents,false))
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
				if (_tangents)
				{
					tangent[t*3+v] = normals.vertex[v].tangent;
					bitangent[t*3+v] = normals.vertex[v].bitangent;
				}
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

void RRMeshArrays::getUvChannels(RRVector<unsigned>& out) const
{
	out.clear();
	RRMesh::TriangleMapping mapping;
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
	#pragma omp critical(aabbCache)
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

unsigned RRMeshArrays::flipFrontBack(unsigned numNormalsThatMustPointBack)
{
	unsigned numFlips = 0;
	for (unsigned t=0;t<numTriangles;t++)
	{
		unsigned numNormalsPointingBack = 0;
		if (numNormalsThatMustPointBack)
		{
			RRVec3 triangleNormal = orthogonalTo(position[triangle[t][1]]-position[triangle[t][0]],position[triangle[t][2]]-position[triangle[t][0]]).normalized();
			if (triangleNormal.finite())
			{
				for (unsigned v=0;v<3;v++)
				{
					if (normal[triangle[t][v]].dot(triangleNormal)<0)
						numNormalsPointingBack++;
				}
			}
		}
		if (numNormalsPointingBack>=numNormalsThatMustPointBack)
		{
			unsigned i = triangle[t][0];
			triangle[t][0] = triangle[t][1];
			triangle[t][1] = i;
			numFlips++;
		}
	}
	if (numFlips)
		version++;
	return numFlips;
}

void RRMeshArrays::buildNormals()
{
	memset(normal,0,sizeof(normal[0])*numVertices);
	for (unsigned t=0;t<numTriangles;t++)
	{
		RRVec3 n = orthogonalTo(position[triangle[t][1]]-position[triangle[t][0]],position[triangle[t][2]]-position[triangle[t][0]]).normalized()
			* getTriangleArea(t); // protection agains tiny needle with wrong normal doing big damage
		if (n.finite())
		{
			normal[triangle[t][0]] += n;
			normal[triangle[t][1]] += n;
			normal[triangle[t][2]] += n;
		}
	}
	for (unsigned v=0;v<numVertices;v++)
	{
		normal[v].normalizeSafe();
	}
	version++;
}

void RRMeshArrays::buildTangents(unsigned uvChannel)
{
	// allocate tangents
	if (!tangent)
	{
		RRVector<unsigned> texcoords;
		for (unsigned i=0;i<texcoord.size();i++) if (texcoord[i]) texcoords.push_back(i);
		RRMeshArrays* tmp = createArrays(true,texcoords,true);
		reload(tmp,true,texcoords,true);
		delete tmp;
	}
	// does uvChannel exist?
	const RRVec2* uv = uvChannel<texcoord.size() ? texcoord[uvChannel] : NULL;
	// generate tangents
	if (!uv)
	{
		// calculated from normals
		for (unsigned v=0;v<numVertices;v++)
		{
			TangentBasis tb;
			tb.normal = normal[v];
			tb.buildBasisFromNormal();
			tangent[v] = tb.tangent;
			bitangent[v] = tb.bitangent;
		}
	}
	else
	{
		// calculated from normals and uvs
		for (unsigned v=0;v<numVertices;v++)
		{
			tangent[v] = RRVec3(0);
			bitangent[v] = RRVec3(0);
		}
		for (unsigned t=0;t<numTriangles;t++)
		{
			unsigned i1 = triangle[t][0];
			unsigned i2 = triangle[t][1];
			unsigned i3 = triangle[t][2];

			const RRVec3& v1 = position[i1];
			const RRVec3& v2 = position[i2];
			const RRVec3& v3 = position[i3];

			const RRVec2& w1 = uv[i1];
			const RRVec2& w2 = uv[i2];
			const RRVec2& w3 = uv[i3];

			float x1 = v2.x - v1.x;
			float x2 = v3.x - v1.x;
			float y1 = v2.y - v1.y;
			float y2 = v3.y - v1.y;
			float z1 = v2.z - v1.z;
			float z2 = v3.z - v1.z;

			float s1 = w2.x - w1.x;
			float s2 = w3.x - w1.x;
			float t1 = w2.y - w1.y;
			float t2 = w3.y - w1.y;

			float r = 1 / (s1 * t2 - s2 * t1);
			RRVec3 udir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
			RRVec3 vdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

			tangent[i1] += udir;
			tangent[i2] += udir;
			tangent[i3] += udir;

			bitangent[i1] += vdir;
			bitangent[i2] += vdir;
			bitangent[i3] += vdir;
		}
		for (unsigned v=0;v<numVertices;v++)
		{
			const RRVec3& n = normal[v];
			RRVec3 t = tangent[v];
			tangent[v] = (t-n*n.dot(t)).normalized();
			bitangent[v] = n.cross(tangent[v])*(n.cross(t).dot(bitangent[v])<0?-1.0f:1.0f);
		}
	}
	version++;
}

unsigned RRMeshArrays::manipulateMapping(unsigned sourceChannel, const float* matrix2x3, unsigned destinationChannel)
{
	if (sourceChannel>=texcoord.size() || !texcoord[sourceChannel])
	{
		return 0;
	}
	if (destinationChannel>=texcoord.size() || !texcoord[destinationChannel])
	{
		RRVector<unsigned> channels;
		getUvChannels(channels);
		channels.push_back(destinationChannel);
		resizeMesh(numTriangles,numVertices,&channels,tangent?true:false,true);
	}
	//if (sourceChannel>=texcoord.size() || !texcoord[sourceChannel])
	//{
	//	for (unsigned i=0;i<numVertices;i++)
	//		texcoord[destinationChannel][i] = RRVec2(matrix2x3[2],matrix2x3[5]);
	//}
	//else
	if (matrix2x3)
	{
		for (unsigned i=0;i<numVertices;i++)
			texcoord[destinationChannel][i] = RRVec2(
				texcoord[sourceChannel][i].x*matrix2x3[0]+texcoord[sourceChannel][i].y*matrix2x3[1]+matrix2x3[2],
				texcoord[sourceChannel][i].x*matrix2x3[3]+texcoord[sourceChannel][i].y*matrix2x3[4]+matrix2x3[5]);
	}
	else
	{
		for (unsigned i=0;i<numVertices;i++)
			texcoord[destinationChannel][i] = texcoord[sourceChannel][i];
	}
	version++;
	return 1;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

RRMeshArrays* RRMesh::createArrays(bool indexed, const RRVector<unsigned>& texcoords, bool tangents) const
{
	RRMeshArrays* importer = new RRMeshArrays();
	if (importer->reload(this,indexed,texcoords,tangents)) return importer;
	delete importer;
	return NULL;
}

} // namespace rr
