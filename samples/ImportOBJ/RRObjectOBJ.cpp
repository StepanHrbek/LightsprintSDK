// --------------------------------------------------------------------------
// Creates Lightsprint interface for OBJ scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2008
// --------------------------------------------------------------------------

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


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectOBJ

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectOBJ : public rr::RRObject, rr::RRMesh
{
public:
	RRObjectOBJ(const char* filename, float scale)
	{
		FILE* f = fopen(filename,"rt");
		if(f)
		{
			char line[100];
			while(fgets(line,100,f))
			{
				VertexInfo v;
				if(sscanf(line,"v %f %f %f",&v.pos[0],&v.pos[1],&v.pos[2])==3)
				{
					v.pos *= scale;
					vertices.push_back(v);
				}
				int indices[4];
				int unused;
				int polySize;
				if((polySize=sscanf(line,"f %d %d %d %d\n",indices+0,indices+1,indices+2,indices+3))>=3
					|| (polySize=sscanf(line,"f %d/%d %d/%d %d/%d %d/%d\n",indices+0,&unused,indices+1,&unused,indices+2,&unused,indices+3,&unused)/2)>=3
					|| (polySize=sscanf(line,"f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n",indices+0,&unused,&unused,indices+1,&unused,&unused,indices+2,&unused,&unused,indices+3,&unused,&unused)/3)>=3)
				{
					TriangleInfo t;
					t.indices[0] = (indices[0]<0) ? (unsigned)(indices[0]+vertices.size()) : indices[0];
					for(int i=2;i<polySize;i++)
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
		verify();
		collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);
		illumination = new rr::RRObjectIllumination((unsigned)vertices.size());
	}
	rr::RRObjectIllumination* getIllumination()
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
	virtual const rr::RRCollider* getCollider() const
	{
		return collider;
	}
	virtual const rr::RRMaterial* getTriangleMaterial(unsigned t, const rr::RRLight* light, const RRObject* receiver) const
	{
		return &material;
	}

private:
	// copy of object's vertices
	struct VertexInfo
	{
		rr::RRVec3 pos;
	};
	std::vector<VertexInfo> vertices;

	// copy of object's triangles
	struct TriangleInfo
	{
		rr::RRMesh::Triangle indices;
	};
	std::vector<TriangleInfo> triangles;

	// default material
	rr::RRMaterial material;
	
	// collider for ray-mesh collisions
	rr::RRCollider* collider;

	// indirect illumination (ambient maps etc)
	rr::RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromOBJ

class ObjectsFromOBJ : public rr::RRObjects
{
public:
	ObjectsFromOBJ(const char* filename, float scale)
	{
		RRObjectOBJ* object = new RRObjectOBJ(filename,scale);
		if(object->getNumTriangles())
			push_back(rr::RRIlluminatedObject(object,object->getIllumination()));
		else
			delete object;
	}
	virtual ~ObjectsFromOBJ()
	{
		if(size())
			delete (*this)[0].object;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObjects* adaptObjectsFromOBJ(const char* filename, float scale)
{
	return new ObjectsFromOBJ(filename,scale);
}
