// --------------------------------------------------------------------------
// Creates Lightsprint interface for 3DS scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008
// --------------------------------------------------------------------------

// This code implements data adapters for access to Model_3DS meshes, objects, materials.
// You can replace Model_3DS with your internal format and adapt this code
// so it works with your data.
//
// For sake of simplicity, instancing is not used here and some data are duplicated.
// See RRObjectCollada as an example of adapter with instancing and
// nearly zero memory requirements.
//
// RRChanneledData - the biggest part of this implementation, provides access to
// custom .3ds data via our custom identifiers CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV etc.
// It is used only by our custom renderer RendererOfRRObject
// (during render of scene with diffuse maps or ambient maps),
// it is never accessed by radiosity solver.
// You may skip it in your implementation.


#include <cassert>
#include <cmath>
#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "RRObject3DS.h"


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

class RRObject3DS : public rr::RRObject, rr::RRMesh
{
public:
	RRObject3DS(Model_3DS* model, unsigned objectIdx);
	rr::RRObjectIllumination* getIllumination();
	virtual ~RRObject3DS();

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual const rr::RRMaterial*   getTriangleMaterial(unsigned t, const rr::RRLight* light, const RRObject* receiver) const;
	virtual const rr::RRMatrix3x4*  getWorldMatrix();

private:
	Model_3DS* model;
	Model_3DS::Object* object;

	// copy of object's geometry
	struct TriangleInfo
	{
		rr::RRMesh::Triangle t;
		unsigned s; // material index
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's material properties
	std::vector<rr::RRMaterial> materials;
	
	// collider for ray-mesh collisions
	const rr::RRCollider* collider;

	// indirect illumination (ambient maps etc)
	rr::RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS load

static void fillMaterial(rr::RRMaterial* s,Model_3DS::Material* m)
{
	enum {size = 8};

	// for diffuse textures provided by 3ds, 
	// it is sufficient to compute average texture color
	rr::RRVec3 avg = rr::RRVec3(0);
	if (m->tex)
	{
		for (unsigned i=0;i<size;i++)
			for (unsigned j=0;j<size;j++)
			{
				avg += m->tex->getElement(rr::RRVec3(i/(float)size,j/(float)size,0));
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
		rr::RRMaterial s;
		fillMaterial(&s,&model->Materials[i]);
		materials.push_back(s);
	}

#ifdef VERIFY
	checkConsistency();
#endif

	// create collider
	bool aborting = false;
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR,aborting);

	// create illumination
	illumination = new rr::RRObjectIllumination((unsigned)object->numVerts);
}

rr::RRObjectIllumination* RRObject3DS::getIllumination()
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
// RRObject3DS implements RRChanneledData

void RRObject3DS::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV:
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_TRANSPARENCY_UV:
			if (numItems) *numItems = RRObject3DS::getNumTriangles();
			if (itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			// unsupported channel
			RRMesh::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRObject3DS::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if (!itemData)
	{
		assert(0);
		return false;
	}
	switch(channelId)
	{
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV:
		case rr::RRObject::CHANNEL_TRIANGLE_VERTICES_TRANSPARENCY_UV:
		{
			if (itemIndex>=RRObject3DS::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRVec2 Out[3];
			Out* out = (Out*)itemData;
			if (sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			Triangle triangle;
			RRObject3DS::getTriangle(itemIndex,triangle);
			for (unsigned v=0;v<3;v++)
			{
				(*out)[v][0] = object->TexCoords[2*triangle[v]];
				(*out)[v][1] = object->TexCoords[2*triangle[v]+1];
			}
			return true;
		}
		case rr::RRObject::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
		{
			if (itemIndex>=RRObject3DS::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRObjectIllumination* Out;
			Out* out = (Out*)itemData;
			if (sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			*out = illumination;
			return true;
		}
		default:
			// unsupported channel
			return RRMesh::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
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


//////////////////////////////////////////////////////////////////////////////
//
// RRObject3DS implements RRObject

const rr::RRCollider* RRObject3DS::getCollider() const
{
	return collider;
}

const rr::RRMaterial* RRObject3DS::getTriangleMaterial(unsigned t, const rr::RRLight* light, const RRObject* receiver) const
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

// Unwrap is not present in .3ds file format.
// If you omit getTriangleMapping (as it happens here), emergency automatic unwrap
// is used and ambient map quality is reduced.
//void RRObject3DS::getTriangleMapping(unsigned t, TriangleMapping& out) const
//{
//	out.uv = unwrap baked with mesh;
//}

const rr::RRMatrix3x4* RRObject3DS::getWorldMatrix()
{
	// transformation matrices from 3ds are ignored
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFrom3DS

class ObjectsFrom3DS : public rr::RRObjects
{
public:
	ObjectsFrom3DS(Model_3DS* model)
	{
		for (unsigned i=0;i<(unsigned)model->numObjects;i++)
		{
			RRObject3DS* object = new RRObject3DS(model,i);
			push_back(rr::RRIlluminatedObject(object,object->getIllumination()));
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

rr::RRObjects* adaptObjectsFrom3DS(Model_3DS* model)
{
	return new ObjectsFrom3DS(model);
}
