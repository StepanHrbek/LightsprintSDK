// --------------------------------------------------------------------------
// Lightsprint adapters for 3DS scene.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_3DS

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
#include "RRObject3DS.h"
#include "Model_3DS.h"

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

	// collider for ray-mesh collisions
	const RRCollider* collider;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS load

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
		// get average colors from textures
		RRScaler* scaler = RRScaler::createFastRgbScaler();
		model->Materials[i].updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
		delete scaler;

		// autodetect keying
		model->Materials[i].updateKeyingFromTransmittance();

		// optimize material flags
		model->Materials[i].updateSideBitsFromColors();
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
					assert(0); // legal, but shouldn't happen in well coded program
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
	
	if (s>=(unsigned)model->numMaterials)
	{
		assert(0);
		return NULL;
	}
	return &model->Materials[s];
}

const RRMatrix3x4* RRObject3DS::getWorldMatrix()
{
	// transformation matrices from 3ds are ignored
	return NULL;
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
			RRObject3DS* object = new RRObject3DS(model,i);
			push_back(RRIlluminatedObject(object,object->getIllumination()));
		}
	}
	virtual ~RRObjects3DS()
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
// RRScene3DS

class RRScene3DS : public RRScene
{
public:
	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		RRScene3DS* scene = new RRScene3DS;
		if (!scene->scene_3ds.Load(filename,scale))
		{
			scene->objects = NULL;
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
	virtual const RRObjects* getObjects()
	{
		return objects;
	}
	virtual ~RRScene3DS()
	{
		delete objects;
	}
private:
	RRObjects*                 objects;
	Model_3DS                  scene_3ds;
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
	RRScene::registerLoader("3ds",RRScene3DS::load);
}

#endif // SUPPORT_3DS
