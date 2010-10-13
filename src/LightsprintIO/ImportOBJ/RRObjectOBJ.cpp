// --------------------------------------------------------------------------
// Lightsprint adapters for OBJ scene.
// Copyright (C) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// OBSOLETED: Assimp contains more complete .obj loader.
//  Still this loader may help learn basics as it is very simple.

#include "../supported_formats.h"
#ifdef SUPPORT_OBJ

// This code loads .obj geometry into RRObjects.
// Materials are not loaded, use Collada for rich import.
//
// Unlike adapters for other formats, this one doesn't adapt 3rd party structure
// in memory, it loads scene directly form file.
//
// http://local.wasp.uwa.edu.au/~pbourke/dataformats/obj/

#include <cstdio>
#include <vector>
#include "RRObjectOBJ.h"

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectOBJ

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectOBJ : public RRObject, RRMesh
{
public:
	RRObjectOBJ(const char* filename)
	{
		// .obj indices start by 1, push dummy elements indexed by 0
		positions.push_back(RRVec3(0));
		uvs.push_back(RRVec2(0,0));
		normals.push_back(RRVec3(0,1,0));

		FILE* f = fopen(filename,"rt");
		if (f)
		{
			char line[1000];
			while (fgets(line,1000,f))
			{
				RRVec3 v;
				int positionIndex[4]; // always specified in file
				int uvIndex[4] = {0,0,0,0}; // if not specified in file, index 0 is used
				int normalIndex[4] = {0,0,0,0}; // if not specified in file, index 0 is used
				int polySize;

				if (sscanf(line,"v %f %f %f",&v.x,&v.y,&v.z)==3)
				{
					positions.push_back(v);
				}
				else
				if (sscanf(line,"vn %f %f %f",&v.x,&v.y,&v.z)==3)
				{
					normals.push_back(v);
				}
				else
				if (sscanf(line,"vt %f %f %f",&v.x,&v.y,&v.z)>=2)
				{
					uvs.push_back(v);
				}
				else
				if ((polySize=sscanf(line,"f %d %d %d %d\n",positionIndex+0,positionIndex+1,positionIndex+2,positionIndex+3))>=3
					|| (polySize=sscanf(line,"f %d/%d %d/%d %d/%d %d/%d\n",positionIndex+0,uvIndex+0,positionIndex+1,uvIndex+1,positionIndex+2,uvIndex+2,positionIndex+3,uvIndex+3)/2)>=3
					|| (polySize=sscanf(line,"f %d//%d %d//%d %d//%d %d//%d\n",positionIndex+0,normalIndex+0,positionIndex+1,normalIndex+1,positionIndex+2,normalIndex+2,positionIndex+3,normalIndex+3)/2)>=3
					|| (polySize=sscanf(line,"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n",positionIndex+0,uvIndex+0,normalIndex+0,positionIndex+1,uvIndex+1,normalIndex+1,positionIndex+2,uvIndex+2,normalIndex+2,positionIndex+3,uvIndex+3,normalIndex+3)/3)>=3)
				{
					TriangleInfo ti;
					ti.positionIndex[0] = (unsigned)( (positionIndex[0]<0) ? positionIndex[0]+positions.size() : (positionIndex[0]) );
					ti.      uvIndex[0] = (unsigned)( (      uvIndex[0]<0) ?       uvIndex[0]+      uvs.size() : (      uvIndex[0]) );
					ti.  normalIndex[0] = (unsigned)( (  normalIndex[0]<0) ?   normalIndex[0]+  normals.size() : (  normalIndex[0]) );
					for (int i=2;i<polySize;i++)
					{
						ti.positionIndex[1] = (unsigned)( (positionIndex[i-1]<0) ? positionIndex[i-1]+positions.size() : (positionIndex[i-1]) );
						ti.      uvIndex[1] = (unsigned)( (      uvIndex[i-1]<0) ?       uvIndex[i-1]+      uvs.size() : (      uvIndex[i-1]) );
						ti.  normalIndex[1] = (unsigned)( (  normalIndex[i-1]<0) ?   normalIndex[i-1]+  normals.size() : (  normalIndex[i-1]) );
						ti.positionIndex[2] = (unsigned)( (positionIndex[i  ]<0) ? positionIndex[i  ]+positions.size() : (positionIndex[i  ]) );
						ti.      uvIndex[2] = (unsigned)( (      uvIndex[i  ]<0) ?       uvIndex[i  ]+      uvs.size() : (      uvIndex[i  ]) );
						ti.  normalIndex[2] = (unsigned)( (  normalIndex[i  ]<0) ?   normalIndex[i  ]+  normals.size() : (  normalIndex[i  ]) );
						triangles.push_back(ti);
					}
				}
				// Todo for materials: mtllib, usemtl, usemap
			}
			fclose(f);
		}
		material.reset(false);
		faceGroups.push_back(FaceGroup(&material,(unsigned)triangles.size()));
		bool aborting = false;
		setCollider(RRCollider::create(this,RRCollider::IT_LINEAR,aborting));
	}
	virtual ~RRObjectOBJ()
	{
		delete getCollider();
	}

	// RRMesh implementation
	unsigned getNumVertices() const
	{
		return (unsigned)positions.size()-1; // why -1? we make .obj vertex numbers 1,2,3... look as 0,1,2... for Lightsprint
	}
	void getVertex(unsigned v, Vertex& out) const
	{
		RR_ASSERT(v<RRObjectOBJ::getNumVertices());
		out = positions[v+1];
	}
	unsigned getNumTriangles() const
	{
		return (unsigned)triangles.size();
	}
	void getTriangle(unsigned t, Triangle& out) const
	{
		RR_ASSERT(t<RRObjectOBJ::getNumTriangles());
		out[0] = triangles[t].positionIndex[0]-1; // why -1? we make .obj vertex numbers 1,2,3... look as 0,1,2... for Lightsprint
		out[1] = triangles[t].positionIndex[1]-1;
		out[2] = triangles[t].positionIndex[2]-1;
	}
	void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		if (t>=RRObjectOBJ::getNumTriangles())
		{
			RR_ASSERT(0);
			return;
		}
		for (unsigned v=0;v<3;v++)
		{
			unsigned normalIndex = triangles[t].normalIndex[v];
			if (!normalIndex || normalIndex>=normals.size())
			{
				RR_ASSERT(normalIndex<normals.size());
				RRMesh::getTriangleNormals(t,out);
				return;
			}
			out.vertex[v].normal = normals[normalIndex];
			out.vertex[v].buildBasisFromNormal();
		}
	}
	bool getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
	{
		if (t>=RRObjectOBJ::getNumTriangles())
		{
			RR_ASSERT(0);
			return false;
		}
		for (unsigned v=0;v<3;v++)
		{
			unsigned uvIndex = triangles[t].uvIndex[v];
			if (!uvIndex || uvIndex>=uvs.size())
			{
				RR_ASSERT(uvIndex<uvs.size());
				return RRMesh::getTriangleMapping(t,out,channel);
			}
			out.uv[v] = uvs[uvIndex];
		}
		return true;
	}

private:
	// copy of object's vertices
	std::vector<RRVec3> positions; // valid indices are 1,2,3... (as in file)
	std::vector<RRVec3> normals; // valid indices are 1,2,3... (as in file) and 0 if not specified in file
	std::vector<RRVec2> uvs; // valid indices are 1,2,3... (as in file) and 0 if not specified in file

	// copy of object's triangles
	struct TriangleInfo
	{
		RRMesh::Triangle positionIndex;
		RRMesh::Triangle normalIndex;
		RRMesh::Triangle uvIndex;
	};
	std::vector<TriangleInfo> triangles;

	// default material
	RRMaterial material;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsOBJ

class RRObjectsOBJ : public RRObjects
{
public:
	RRObjectsOBJ(const char* filename)
	{
		RRObjectOBJ* object = new RRObjectOBJ(filename);
		if (object->getNumTriangles())
			push_back(object);
		else
			delete object;
	}
	virtual ~RRObjectsOBJ()
	{
		if (size())
			delete (*this)[0];
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneOBJ

class RRSceneOBJ : public RRScene
{
public:
	static RRScene* load(const char* filename, bool* aborting, float emissiveMultiplier)
	{
		RRSceneOBJ* scene = new RRSceneOBJ;
		scene->protectedObjects = adaptObjectsFromOBJ(filename);
		return scene;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromOBJ(const char* filename)
{
	return new RRObjectsOBJ(filename);
}

void registerLoaderOBJ()
{
	RRScene::registerLoader("*.obj",RRSceneOBJ::load);
}

#endif // SUPPORT_OBJ
