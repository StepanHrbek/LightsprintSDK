// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Mesh with direct access.
// --------------------------------------------------------------------------

#include <cstdlib>
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
	triangle = nullptr;
	numVertices = 0;
	position = nullptr;
	normal = nullptr;
	tangent = nullptr;
	bitangent = nullptr;
	// MeshArraysVBOs synces to mesh arrays pointer and version.
	// if someone deletes mesh arrays and creates new (different) one, pointer may be identical,
	// so make at least version different.
	version = rand();
	unwrapChannel = UINT_MAX;
	unwrapWidth = 0;
	unwrapHeight = 0;
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
	RRMeshArrays* backup = _preserveContents ? createArrays(true,_texcoords?*_texcoords:tmp,_tangents) : nullptr;

	// free/alloc
	char* pool = (char*)triangle;
	if (newSize>poolSize)
	{
		free(triangle);
		pool = newSize ? (char*)malloc(newSize) : nullptr;
		if (newSize && !pool)
		{
			RRReporter::report(ERRO,"Allocation of %dMB failed when resizing mesh to %d triangles, %d vertices, %d uv channels.\n",newSize/1024/1024,_numTriangles,_numVertices,_texcoords?_texcoords->size():0);
			resizeMesh(0,0,nullptr,false,false);
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
		texcoord[i] = nullptr;
	}
	if (!newSize)
	{
		triangle = nullptr;
		position = nullptr;
		normal = nullptr;
		tangent = nullptr;
		bitangent = nullptr;
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
				tangent = nullptr;
				bitangent = nullptr;
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
					texcoord.resize((*_texcoords)[i]+1,nullptr);
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
		#pragma omp parallel for schedule(static) if(numVertices>RR_OMP_MIN_ELEMENTS)
		for (int v=0;v<(int)numVertices;v++)
		{
			_mesh->getVertex(v,position[v]);
			filled[v] = false;
		}
		#pragma omp parallel for schedule(static) if(numTriangles>RR_OMP_MIN_ELEMENTS)
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
		//unsigned unfilled = 0;
		//for (unsigned v=0;v<numVertices;v++)
		//{
		//	if (!filled[v]) unfilled++;
		//}
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
		#pragma omp parallel for schedule(static) if(numTriangles>RR_OMP_MIN_ELEMENTS)
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
	RR_ASSERT(normal);
	if (t>=numTriangles || !normal)
	{
		RR_ASSERT(0);
		return;
	}
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
					if (std::isfinite(position[v][j])) // filter out INF/NaN
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

unsigned RRMeshArrays::flipFrontBack(unsigned numNormalsThatMustPointBack, bool negativeScale)
{
	if (numNormalsThatMustPointBack>3)
		return 0;
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
					if (normal[triangle[t][v]].dot(triangleNormal)*(negativeScale?-1.f:1.f)<0)
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
	const RRVec2* uv = uvChannel<texcoord.size() ? texcoord[uvChannel] : nullptr;
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

			if (udir.finite() && vdir.finite()) // exclude invalid inputs, exclude corner cases with r = 1 / 0;
			{
				tangent[i1] += udir;
				tangent[i2] += udir;
				tangent[i3] += udir;

				bitangent[i1] += vdir;
				bitangent[i2] += vdir;
				bitangent[i3] += vdir;
			}
		}
		for (unsigned v=0;v<numVertices;v++)
		{
			const RRVec3& n = normal[v];
			RRVec3 t = tangent[v];
			tangent[v] = (t-n*n.dot(t)).normalizedSafe(); // sum of valid vectors can be 0, normalize safely
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
// RRMeshArrays factory

RRMeshArrays* RRMeshArrays::rectangle()
{
	RRMeshArrays* arrays = new RRMeshArrays;

	constexpr int W=1, H=1;
	enum {TRIANGLES=W*H*2,VERTICES=(W+1)*(H+1)};
	rr::RRVector<unsigned> texcoords;
	texcoords.push_back(0);
	arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
	for (unsigned j=0;j<H;j++)
	for (unsigned i=0;i<W;i++)
	{
		arrays->triangle[2*(i+W*j)  ][0] = i  + j   *(W+1);
		arrays->triangle[2*(i+W*j)  ][1] = i  +(j+1)*(W+1);
		arrays->triangle[2*(i+W*j)  ][2] = i+1+(j+1)*(W+1);
		arrays->triangle[2*(i+W*j)+1][0] = i  + j   *(W+1);
		arrays->triangle[2*(i+W*j)+1][1] = i+1+(j+1)*(W+1);
		arrays->triangle[2*(i+W*j)+1][2] = i+1+ j   *(W+1);
	}
	for (unsigned i=0;i<VERTICES;i++)
	{
		arrays->position[i] = rr::RRVec3((i%(W+1))/(float)W-0.5f,0,i/(W+1)/(float)H-0.5f);
		arrays->normal[i] = rr::RRVec3(0,1,0);
		arrays->texcoord[0][i] = rr::RRVec2(i/(W+1)/(float)H,(i%(W+1))/(float)W);
	}

	return arrays;
}

RRMeshArrays* RRMeshArrays::plane()
{
	RRMeshArrays* arrays = new RRMeshArrays;

	enum {H=6,TRIANGLES=1+6*H,VERTICES=3+3*H+2}; // 2 extra (unused) vertices
	rr::RRVector<unsigned> texcoords;
	texcoords.push_back(0);
	arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
	arrays->triangle[0][0] = 0;
	arrays->triangle[0][1] = 1;
	arrays->triangle[0][2] = 2;
	for (unsigned i=0;i<TRIANGLES-1;i+=2)
	{
		arrays->triangle[i+1][0] = i/6*3+((2+((i/2)%3))%3);
		arrays->triangle[i+1][1] = i/6*3+((1+((i/2)%3))%3);
		arrays->triangle[i+1][2] = i/6*3+((0+((i/2)%3))%3)+3;
		arrays->triangle[i+2][0] = i/6*3+((2+((i/2)%3))%3);
		arrays->triangle[i+2][1] = i/6*3+((0+((i/2)%3))%3)+3;
		arrays->triangle[i+2][2] = i/6*3+((1+((i/2)%3))%3)+3;
	}
	float v[] = {1.732f,-1, -1.732f,-1, 0,2};
	for (unsigned i=0;i<VERTICES;i++)
	{
		arrays->position[i] = rr::RRVec3(v[(i%3)*2],0,v[(i%3)*2+1])*powf(4,i/3)*((i/3)%2?1:-1.f);
		arrays->normal[i] = rr::RRVec3(0,1,0);
		arrays->texcoord[0][i] = rr::RRVec2(arrays->position[i].z,arrays->position[i].x);
	}

	// adjust 2 extra (unused) vertices to make also center of bounding box in 0
	// (center of gravity already is in 0)
	arrays->position[VERTICES-1].x = arrays->position[VERTICES-1].z;
	arrays->position[VERTICES-2] = -arrays->position[VERTICES-1];

	return arrays;
}

RRMeshArrays* RRMeshArrays::box()
{
	RRMeshArrays* arrays = new RRMeshArrays;

	enum {TRIANGLES=12,VERTICES=24};
	const unsigned char triangles[3*TRIANGLES] = {0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20};
	const unsigned char positions[3*VERTICES] = {
		0,1,0, 0,1,1, 1,1,1, 1,1,0,
		0,1,1, 0,0,1, 1,0,1, 1,1,1,
		1,1,1, 1,0,1, 1,0,0, 1,1,0,
		1,1,0, 1,0,0, 0,0,0, 0,1,0,
		0,1,0, 0,0,0, 0,0,1, 0,1,1,
		1,0,0, 1,0,1, 0,0,1, 0,0,0};
	const unsigned char uvs[2*VERTICES] = {0,1, 0,0, 1,0, 1,1};
	rr::RRVector<unsigned> texcoords;
	texcoords.push_back(0);
	arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
	for (unsigned i=0;i<TRIANGLES;i++)
		for (unsigned j=0;j<3;j++)
			// commented out code can replace constant arrays, but 2 cube sides have mapping rotated
			//arrays->triangle[i][j] = i*2+((i%2)?1-(((i/2)%2)?2-j:j):(((i/2)%2)?2-j:j));
			arrays->triangle[i][j] = triangles[3*i+j];
	for (unsigned i=0;i<VERTICES;i++)
	{
		for (unsigned j=0;j<3;j++)
			//arrays->position[i][j] = ((j==((i/8)%3)) ? ((i/4)%2) : ((j==((1+i/8)%3)) ? ((i/2)%2) : (i%2))) - 0.5f;
			arrays->position[i][j] = positions[3*i+j]-0.5f;
		//arrays->texcoord[0][i] = ((i/4)%2) ? rr::RRVec2((i/2)%2,i%2) : rr::RRVec2(1-(i/2)%2,(i%2));
		for (unsigned j=0;j<2;j++)
			arrays->texcoord[0][i][j] = uvs[2*(i%4)+j];
	}
	arrays->buildNormals();

	return arrays;
}

RRMeshArrays* RRMeshArrays::sphere()
{
	RRMeshArrays* arrays = new RRMeshArrays;

	constexpr int W=30, H=15;
	enum {TRIANGLES=W*H*2,VERTICES=(W+1)*(H+1)};
	rr::RRVector<unsigned> texcoords;
	texcoords.push_back(0);
	arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
	for (unsigned j=0;j<H;j++)
	for (unsigned i=0;i<W;i++)
	{
		arrays->triangle[2*(i+W*j)  ][0] = i  + j   *(W+1);
		arrays->triangle[2*(i+W*j)  ][1] = i+1+(j+1)*(W+1);
		arrays->triangle[2*(i+W*j)  ][2] = i  +(j+1)*(W+1);
		arrays->triangle[2*(i+W*j)+1][0] = i  + j   *(W+1);
		arrays->triangle[2*(i+W*j)+1][1] = i+1+ j   *(W+1);
		arrays->triangle[2*(i+W*j)+1][2] = i+1+(j+1)*(W+1);
	}
	for (unsigned i=0;i<VERTICES;i++)
	{
		arrays->normal[i] = rr::RRVec3(cos(2*RR_PI*(i%(W+1))/W)*sin(i/(W+1)*RR_PI/H),cos(i/(W+1)*RR_PI/H),sin(2*RR_PI*(i%(W+1))/W)*sin(i/(W+1)*RR_PI/H));
		if (i/(W+1)==H)
		{
			// sin(0)==0 so north pole is centered; sin(RR_PI)!=0 so south pole is slightly off; also center from getAABB() is slightly off zero
			// this centers south pole
			// with uncentered south pole, RL's spherical mapping would look bad around pole (all south pole vertices would stitched)
			arrays->normal[i] = rr::RRVec3(0,-1,0);
		}
		arrays->position[i] = arrays->normal[i]/2;
		arrays->texcoord[0][i] = rr::RRVec2(1-(i%(W+1))/(float)W,1-i/(W+1)/(float)H);
	}

	return arrays;
}

RRMeshArrays* RRMeshArrays::cylinder()
{
	RRMeshArrays* arrays = new RRMeshArrays;

	constexpr int W=30;
	enum {TRIANGLES=(W-2)+W+W+(W-2),VERTICES=W+(W+1)+(W+1)+W};
	rr::RRVector<unsigned> texcoords;
	texcoords.push_back(0);
	arrays->resizeMesh(TRIANGLES,VERTICES,&texcoords,false,false);
	unsigned v = 0;
	unsigned t = 0;
	// add sides
	for (unsigned j=0;j<2;j++)
	for (unsigned i=0;i<W+1;i++)
	{
		arrays->position[v] = rr::RRVec3(cos(2*RR_PI*i/W),j?1:-1.f,sin(2*RR_PI*i/W))*0.5f;
		arrays->normal[v] = rr::RRVec3(cos(2*RR_PI*i/W),0,sin(2*RR_PI*i/W));
		arrays->texcoord[0][v] = rr::RRVec2((float)(W-i)/W,j?1.f:0);
		if (j==0 && i!=W)
		{
			arrays->triangle[t][0] = v+1;
			arrays->triangle[t][1] = v;
			arrays->triangle[t++][2] = v+W+1;
		}
		else
		if (j==1 && i!=W)
		{
			arrays->triangle[t][0] = v;
			arrays->triangle[t][1] = v+1;
			arrays->triangle[t++][2] = v-W;
		}
		v++;
	}
	// add top+bottom
	for (unsigned j=0;j<2;j++)
	for (unsigned i=0;i<W;i++)
	{
		arrays->position[v] = rr::RRVec3(cos(2*RR_PI*i/W),j?1:-1.f,sin(2*RR_PI*i/W))*0.5f;
		arrays->normal[v] = rr::RRVec3(0,j?1:-1.f,0);
		arrays->texcoord[0][v] = rr::RRVec2(cos(2*RR_PI*i/W)*(j?-0.5f:0.5f)+0.5f,sin(2*RR_PI*i/W)*0.5f+0.5f);
		if (j==0 && i<W-2)
		{
			arrays->triangle[t][0] = v;
			arrays->triangle[t][1] = v+1;
			arrays->triangle[t++][2] = v-i+W-1;
		}
		else
		if (j==1 && i<W-2)
		{
			arrays->triangle[t][0] = v+1;
			arrays->triangle[t][1] = v;
			arrays->triangle[t++][2] = v-i+W-1;
		}
		v++;
	}
	RR_ASSERT(t==TRIANGLES);
	RR_ASSERT(v==VERTICES);

	return arrays;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMesh

RRMeshArrays* RRMesh::createArrays(bool indexed, const RRVector<unsigned>& texcoords, bool tangents) const
{
	RRMeshArrays* importer = new RRMeshArrays();
	if (importer->reload(this,indexed,texcoords,tangents)) return importer;
	delete importer;
	return nullptr;
}

} // namespace rr
