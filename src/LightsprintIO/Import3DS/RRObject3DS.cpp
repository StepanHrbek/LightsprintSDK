// --------------------------------------------------------------------------
// Lightsprint adapters for 3DS scene.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
// See RRObjectFCollada as an example of adapter with instancing and
// nearly zero memory requirements.


#include <cmath>
#include <vector>
#include "RRObject3DS.h"
#include "Model_3DS.h"

using namespace rr;


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
	std::vector<RRMesh::Triangle> triangles;
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
			RRMesh::Triangle ti;
			ti[0] = object->MatFaces[i].subFaces[3*j];
			ti[1] = object->MatFaces[i].subFaces[3*j+1];
			ti[2] = object->MatFaces[i].subFaces[3*j+2];
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
	out = triangles[t];
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
	if (channel==0 && t<RRObject3DS::getNumTriangles())
	{
		Triangle triangle;
		RRObject3DS::getTriangle(t,triangle);
		for (unsigned v=0;v<3;v++)
		{
			out.uv[v] = object->TexCoords[triangle[v]];
		}
		return true;
	}
	return false;
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
	static RRScene* load(const char* filename, bool* aborting)
	{
		RRScene3DS* scene = new RRScene3DS;
		if (!scene->scene_3ds.Load(filename,1))
		{
			delete scene;
			RRReporter::report(WARN,"Failed loading scene %s.\n");
			return NULL;
		}
		else
		{
			scene->protectedObjects = adaptObjectsFrom3DS(&scene->scene_3ds);
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
