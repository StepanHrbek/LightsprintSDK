// --------------------------------------------------------------------------
// Lightsprint adapters for OBJ scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_OBJ

// This code loads .obj geometry into RRObjects.
// Materials are not loaded, use Collada for rich import.
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
// Verificiation
//
// Helps during development of new adapters.
// Define VERIFY to enable verification of adapters and data.
// RRReporter will be used to warn about detected data inconsistencies.
// Once your code/data are verified and don't emit messages via reporter(),
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


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
#ifdef VERIFY
		checkConsistency(UINT_MAX,UINT_MAX);
#endif
		bool aborting = false;
		collider = RRCollider::create(this,RRCollider::IT_LINEAR,aborting);
		illumination = new RRObjectIllumination(RRObjectOBJ::getNumVertices());
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
