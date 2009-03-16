// --------------------------------------------------------------------------
// Creates Lightsprint interface for 3DS scene
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// This code implements data adapters for access to Model_3DS meshes, objects, materials.
// You can replace Model_3DS with your internal format and adapt this code
// so it works with your data.
//
// For sake of simplicity, instancing is not used here and some data are duplicated.
// See RRObjectCollada as an example of adapter with instancing and
// nearly zero memory requirements.


#include <cassert>
#include <cmath>
#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "RRObject3DS.h"

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new adapters.
// Define VERIFY to enable verification of adapters and data.
// Once your code/data are verified and don't emit messages via reporter(),
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObject3DS : public RRObject, RRMesh
{
public:
	RRObject3DS(Model_3DS* model, unsigned objectIdx);
	RRObjectIllumination* getIllumination();
	virtual ~RRObject3DS();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;

	// RRObject
	virtual const RRCollider*   getCollider() const;
	virtual const RRMaterial*   getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const;
	virtual const RRMatrix3x4*  getWorldMatrix();

private:
	Model_3DS* model;
	Model_3DS::Object* object;

	// copy of object's geometry
	struct TriangleInfo
	{
		RRMesh::Triangle t;
		unsigned s; // material index
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's material properties
	std::vector<RRMaterial> materials;
	
	// collider for ray-mesh collisions
	const RRCollider* collider;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS load

enum Channel
{
	CH_LIGHTMAP,
	CH_DIFFUSE,
};

static void fillMaterial(RRMaterial* s,Model_3DS::Material* m)
{
	enum {size = 8};

	// for diffuse textures provided by 3ds, 
	// it is sufficient to compute average texture color
	RRVec3 avg = RRVec3(0);
	if (m->tex)
	{
		for (unsigned i=0;i<size;i++)
			for (unsigned j=0;j<size;j++)
			{
				avg += m->tex->getElement(RRVec3(i/(float)size,j/(float)size,0));
			}
		avg /= size*size;
		//avg[0] *= m->color.r/255.0f;
		//avg[1] *= m->color.g/255.0f;
		//avg[2] *= m->color.b/255.0f;
	}
	else
	{
		avg[0] = m->color.r/255.0f;
		avg[1] = m->color.g/255.0f;
		avg[2] = m->color.b/255.0f;
	}

	// set all properties to default
	s->reset(0);

	// set diffuse reflectance according to 3ds material
	s->diffuseReflectance.color = avg;
	s->diffuseReflectance.texture = m->tex;
	s->diffuseReflectance.texcoord = CH_DIFFUSE;
	s->lightmapTexcoord = CH_LIGHTMAP;
}

// Creates internal copies of .3ds geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObject3DS::RRObject3DS(Model_3DS* amodel, unsigned objectIdx)
{
	model = amodel;
	object = &model->Objects[objectIdx];

	for (unsigned i=0;i<(unsigned)object->numMatFaces;i++)
	{
		for (unsigned j=0;j<(unsigned)object->MatFaces[i].numSubFaces/3;j++)
		{
			TriangleInfo ti;
			ti.t[0] = object->MatFaces[i].subFaces[3*j];
			ti.t[1] = object->MatFaces[i].subFaces[3*j+1];
			ti.t[2] = object->MatFaces[i].subFaces[3*j+2];
			ti.s = object->MatFaces[i].MatIndex;
			triangles.push_back(ti);
		}
	}

	for (unsigned i=0;i<(unsigned)model->numMaterials;i++)
	{
		RRMaterial s;
		fillMaterial(&s,&model->Materials[i]);
		materials.push_back(s);
	}

#ifdef VERIFY
	checkConsistency();
#endif

	// create collider
	bool aborting = false;
	collider = RRCollider::create(this,RRCollider::IT_LINEAR,aborting);

	// create illumination
	illumination = new RRObjectIllumination((unsigned)object->numVerts);
}

RRObjectIllumination* RRObject3DS::getIllumination()
{
	return illumination;
}

RRObject3DS::~RRObject3DS()
{
	delete illumination;
	delete collider;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS implements RRMesh

unsigned RRObject3DS::getNumVertices() const
{
	return object->numVerts;
}

void RRObject3DS::getVertex(unsigned v, Vertex& out) const
{
	assert(v<(unsigned)object->numVerts);
	out[0] = object->Vertexes[3*v];
	out[1] = object->Vertexes[3*v+1];
	out[2] = object->Vertexes[3*v+2];
}

unsigned RRObject3DS::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObject3DS::getTriangle(unsigned t, Triangle& out) const
{
	if (t>=RRObject3DS::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out = triangles[t].t;
}

void RRObject3DS::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=RRObject3DS::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	RRObject3DS::getTriangle(t,triangle);
	for (unsigned v=0;v<3;v++)
	{
		out.vertex[v].normal[0] = object->Normals[3*triangle[v]];
		out.vertex[v].normal[1] = object->Normals[3*triangle[v]+1];
		out.vertex[v].normal[2] = object->Normals[3*triangle[v]+2];
		out.vertex[v].buildBasisFromNormal();
	}
}

bool RRObject3DS::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	switch (channel)
	{
		case CH_LIGHTMAP:
			// 3ds doesn't contain unwrap, let's fallback to autogenerated one
			return RRMesh::getTriangleMapping(t,out,0);
		case CH_DIFFUSE:
			{
				if (t>=RRObject3DS::getNumTriangles())
				{
					assert(0); // legal, but shouldn't happen in well coded program
					return false;
				}
				Triangle triangle;
				RRObject3DS::getTriangle(t,triangle);
				for (unsigned v=0;v<3;v++)
				{
					out.uv[v][0] = object->TexCoords[2*triangle[v]];
					out.uv[v][1] = object->TexCoords[2*triangle[v]+1];
				}
				return true;
			}
		default:
			return false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS implements RRObject

const RRCollider* RRObject3DS::getCollider() const
{
	return collider;
}

const RRMaterial* RRObject3DS::getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
{
	if (t>=RRObject3DS::getNumTriangles())
	{
		assert(0);
		return NULL;
	}
	unsigned s = triangles[t].s;
	if (s>=materials.size())
	{
		assert(0);
		return NULL;
	}
	return &materials[s];
}

const RRMatrix3x4* RRObject3DS::getWorldMatrix()
{
	// transformation matrices from 3ds are ignored
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFrom3DS

class ObjectsFrom3DS : public RRObjects
{
public:
	ObjectsFrom3DS(Model_3DS* model)
	{
		for (unsigned i=0;i<(unsigned)model->numObjects;i++)
		{
			RRObject3DS* object = new RRObject3DS(model,i);
			push_back(RRIlluminatedObject(object,object->getIllumination()));
		}
	}
	virtual ~ObjectsFrom3DS()
	{
		for (unsigned i=0;i<size();i++)
		{
			// no need to delete illumination separately, we created it as part of object
			//delete (*this)[i].illumination;
			delete (*this)[i].object;
		}
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFrom3DS(Model_3DS* model)
{
	return new ObjectsFrom3DS(model);
}
