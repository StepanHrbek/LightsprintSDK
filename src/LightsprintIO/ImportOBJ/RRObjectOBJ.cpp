// --------------------------------------------------------------------------
// Lightsprint adapters for OBJ scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_OBJ

// This code loads .obj geometry into RRObjects.
// Only raw triangles are loaded, use Collada for rich import.
// .obj even doesn't support lights, so loaded scene is completely black.
//
// Unlike adapters for other formats, this one doesn't adapt 3rd party structure
// in memory, it loads scene directly form file.

#include <cstdio>
#include <vector>
#include "Lightsprint/RRIllumination.h"
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
	RRObjectOBJ(const char* filename, float scale)
	{
		FILE* f = fopen(filename,"rt");
		if (f)
		{
			char line[100];
			while (fgets(line,100,f))
			{
				VertexInfo v;
				if (sscanf(line,"v %f %f %f",&v.pos[0],&v.pos[1],&v.pos[2])==3)
				{
					v.pos *= scale;
					vertices.push_back(v);
				}
				int indices[4];
				int unused;
				int polySize;
				if ((polySize=sscanf(line,"f %d %d %d %d\n",indices+0,indices+1,indices+2,indices+3))>=3
					|| (polySize=sscanf(line,"f %d/%d %d/%d %d/%d %d/%d\n",indices+0,&unused,indices+1,&unused,indices+2,&unused,indices+3,&unused)/2)>=3
					|| (polySize=sscanf(line,"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n",indices+0,&unused,&unused,indices+1,&unused,&unused,indices+2,&unused,&unused,indices+3,&unused,&unused)/3)>=3)
				{
					TriangleInfo t;
					t.indices[0] = (indices[0]<0) ? (unsigned)(indices[0]+vertices.size()) : indices[0];
					for (int i=2;i<polySize;i++)
					{
						t.indices[1] = (indices[i-1]<0) ? (unsigned)(indices[i-1]+vertices.size()) : indices[i-1];
						t.indices[2] = (indices[i]<0) ? (unsigned)(indices[i]+vertices.size()) : indices[i];
						triangles.push_back(t);
					}
				}
			}
			fclose(f);
		}
		material.reset(false);
//		checkConsistency();
		bool aborting = false;
		collider = RRCollider::create(this,RRCollider::IT_LINEAR,aborting);
		illumination = new RRObjectIllumination((unsigned)vertices.size());
	}
	RRObjectIllumination* getIllumination()
	{
		return illumination;
	}
	virtual ~RRObjectOBJ()
	{
		delete illumination;
		delete collider;
	}

	// RRMesh implementation
	unsigned getNumVertices() const
	{
		return (unsigned)vertices.size();
	}
	void getVertex(unsigned v, Vertex& out) const
	{
		RR_ASSERT(v<vertices.size());
		out = vertices[v].pos;
	}
	unsigned getNumTriangles() const
	{
		return (unsigned)triangles.size();
	}
	void getTriangle(unsigned t, Triangle& out) const
	{
		RR_ASSERT(t<triangles.size());
		out = triangles[t].indices;
	}

	// RRObject implementation
	virtual const RRCollider* getCollider() const
	{
		return collider;
	}
	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
		return &material;
	}

private:
	// copy of object's vertices
	struct VertexInfo
	{
		RRVec3 pos;
	};
	std::vector<VertexInfo> vertices;

	// copy of object's triangles
	struct TriangleInfo
	{
		RRMesh::Triangle indices;
	};
	std::vector<TriangleInfo> triangles;

	// default material
	RRMaterial material;
	
	// collider for ray-mesh collisions
	const RRCollider* collider;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsOBJ

class RRObjectsOBJ : public RRObjects
{
public:
	RRObjectsOBJ(const char* filename, float scale)
	{
		RRObjectOBJ* object = new RRObjectOBJ(filename,scale);
		if (object->getNumTriangles())
			push_back(RRIlluminatedObject(object,object->getIllumination()));
		else
			delete object;
	}
	virtual ~RRObjectsOBJ()
	{
		if (size())
			delete (*this)[0].object;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneOBJ

class RRSceneOBJ : public RRScene
{
public:
	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		RRSceneOBJ* scene = new RRSceneOBJ;
		scene->objects = adaptObjectsFromOBJ(filename);
		return scene;
	}
	virtual const RRObjects* getObjects()
	{
		return objects;
	}
	virtual ~RRSceneOBJ()
	{
		delete objects;
	}
private:
	RRObjects*             objects;
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromOBJ(const char* filename, float scale)
{
	return new RRObjectsOBJ(filename,scale);
}

void registerLoaderOBJ()
{
	RRScene::registerLoader("obj",RRSceneOBJ::load);
}

#endif // SUPPORT_OBJ
