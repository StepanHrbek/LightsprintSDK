// --------------------------------------------------------------------------
// Lightsprint adapters for 3DS scene.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// OBSOLETED: Assimp contains more complete .3ds loader.
//  Still this loader may help learn basics as it is very simple.

#include "../supported_formats.h"
#ifdef SUPPORT_3DS

// This code implements data adapters for access to Model_3DS meshes, objects, materials.
// You can replace Model_3DS with your internal format and adapt this code
// so it works with your data.
//
// For sake of simplicity, instancing is not used here and some data are duplicated.
// See RRObjectCollada as an example of adapter with instancing and
// nearly zero memory requirements.


#include <cmath>
#include <vector>
#include "RRObject3DS.h"
#include "Model_3DS.h"

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
// RRObject3DS

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObject3DS : public RRObject, public RRMesh
{
public:
	RRObject3DS(Model_3DS* model, unsigned objectIdx);
	virtual ~RRObject3DS();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;

private:
	Model_3DS::Object* object;
	
	RRMaterial defaultGray;

	// copy of object's indices
	struct TriangleInfo
	{
		RRMesh::Triangle t;
		RRMaterial* material;
	};
	std::vector<TriangleInfo> triangles;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS load

// Creates internal copies of .3ds geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObject3DS::RRObject3DS(Model_3DS* _model, unsigned _objectIdx)
{
	defaultGray.reset(false);
	object = _model->Objects+_objectIdx;
	name = object->name;

	for (unsigned i=0;i<(unsigned)object->numMatFaces;i++)
	{
		for (unsigned j=0;j<(unsigned)object->MatFaces[i].numSubFaces/3;j++)
		{
			TriangleInfo ti;
			ti.t[0] = object->MatFaces[i].subFaces[3*j];
			ti.t[1] = object->MatFaces[i].subFaces[3*j+1];
			ti.t[2] = object->MatFaces[i].subFaces[3*j+2];
			triangles.push_back(ti);
		}
		// create facegroup
		unsigned materialIndex = object->MatFaces[i].MatIndex;
		rr::RRMaterial* material;
		if (materialIndex>=(unsigned)_model->numMaterials)
		{
			// no material found in .3ds
			material = &defaultGray;
		}
		else
		{
			material = &_model->Materials[materialIndex];
		}
		faceGroups.push_back(FaceGroup(material,(unsigned)object->MatFaces[i].numSubFaces/3));
	}

#ifdef VERIFY
	checkConsistency(UINT_MAX,_objectIdx);
#endif

	// create collider
	bool aborting = false;
	setCollider(RRCollider::create(this,RRCollider::IT_LINEAR,aborting));
}

RRObject3DS::~RRObject3DS()
{
	delete getCollider();
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
	RR_ASSERT(v<(unsigned)object->numVerts);
	out = object->Vertexes[v];
}

unsigned RRObject3DS::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObject3DS::getTriangle(unsigned t, Triangle& out) const
{
	if (t>=RRObject3DS::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	out = triangles[t].t;
}

void RRObject3DS::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=RRObject3DS::getNumTriangles())
	{
		RR_ASSERT(0);
		return;
	}
	Triangle triangle;
	RRObject3DS::getTriangle(t,triangle);
	for (unsigned v=0;v<3;v++)
	{
		out.vertex[v].normal = object->Normals[triangle[v]];
		out.vertex[v].buildBasisFromNormal();
	}
}

bool RRObject3DS::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	switch (channel)
	{
		case Model_3DS::Material::CH_LIGHTMAP:
			// 3ds doesn't contain unwrap, let's fallback to autogenerated one
			return RRMesh::getTriangleMapping(t,out,0);
		case Model_3DS::Material::CH_DIFFUSE_SPECULAR_EMISSIVE_OPACITY:
			{
				if (t>=RRObject3DS::getNumTriangles())
				{
					RR_ASSERT(0); // legal, but shouldn't happen in well coded program
					return false;
				}
				Triangle triangle;
				RRObject3DS::getTriangle(t,triangle);
				for (unsigned v=0;v<3;v++)
				{
					out.uv[v] = object->TexCoords[triangle[v]];
				}
				return true;
			}
		default:
			return false;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjects3DS

class RRObjects3DS : public RRObjects
{
public:
	RRObjects3DS(Model_3DS* model)
	{
		for (unsigned i=0;i<(unsigned)model->numObjects;i++)
		{
			push_back(new RRObject3DS(model,i));
		}
	}
	virtual ~RRObjects3DS()
	{
		for (unsigned i=size();i--;)
		{
			delete (*this)[i];
		}
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRScene3DS

class RRScene3DS : public RRScene
{
public:
	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		RRScene3DS* scene = new RRScene3DS;
		if (!scene->scene_3ds.Load(filename,scale))
		{
			delete scene;
			RRReporter::report(WARN,"Failed loading scene %s.\n");
			return NULL;
		}
		else
		{
			scene->objects = adaptObjectsFrom3DS(&scene->scene_3ds);
			return scene;
		}
	}
private:
	Model_3DS scene_3ds;
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFrom3DS(Model_3DS* model)
{
	return new RRObjects3DS(model);
}

void registerLoader3DS()
{
	RRScene::registerLoader("*.3ds",RRScene3DS::load);
}

#endif // SUPPORT_3DS
