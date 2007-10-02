// --------------------------------------------------------------------------
// Creates Lightsprint interface for 3DS scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to Model_3DS meshes, objects, materials.
// You can replace Model_3DS with your internal format and adapt this code
// so it works with your data.
//
// For sake of simplicity, instancing is not used here and some data are duplicated.
// See RRObjectCollada as an example of wrapper with instancing and
// nearly zero memory requirements.
//
// RRChanneledData - the biggest part of this implementation, provides access to
// custom .3ds data via our custom identifiers CHANNEL_TRIANGLE_DIFFUSE_TEX etc.
// It is used only by our custom renderer RendererOfRRObject
// (during render of scene with diffuse maps or ambient maps),
// it is never accessed by radiosity solver.
// You may skip it in your implementation.


#include <cassert>
#include <cmath>
#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "RRObject3DS.h"
#include "Lightsprint/GL/RendererOfRRObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new wrappers.
// Define VERIFY to enable verification of wrappers and data.
// Once your code/data are verified and don't emit messages via reporter(),
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY

#ifdef VERIFY
void reporter(const char* msg, void* context)
{
	puts(msg);
}
#endif


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
	virtual const rr::RRMaterial*   getTriangleMaterial(unsigned t) const;
	virtual const rr::RRMatrix3x4*  getWorldMatrix();
	virtual const rr::RRMatrix3x4*  getInvWorldMatrix();

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
	rr::RRCollider* collider;

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
	rr::RRColor avg = rr::RRColor(0);
	if(m->tex)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				float tmp[4];
				m->tex->getPixel(i/(float)size,j/(float)size,0,tmp);
				avg += rr::RRVec3(tmp[0],tmp[1],tmp[2]);
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
	s->diffuseReflectance = avg;

#ifdef VERIFY
	if(s->validate())
		reporter("Material adjusted to physically valid.",NULL);
#else
	s->validate();
#endif
}

// Creates internal copies of .3ds geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObject3DS::RRObject3DS(Model_3DS* amodel, unsigned objectIdx)
{
	model = amodel;
	object = &model->Objects[objectIdx];

	for(unsigned i=0;i<(unsigned)object->numMatFaces;i++)
	{
		for(unsigned j=0;j<(unsigned)object->MatFaces[i].numSubFaces/3;j++)
		{
			TriangleInfo ti;
			ti.t[0] = object->MatFaces[i].subFaces[3*j];
			ti.t[1] = object->MatFaces[i].subFaces[3*j+1];
			ti.t[2] = object->MatFaces[i].subFaces[3*j+2];
			ti.s = object->MatFaces[i].MatIndex;
			triangles.push_back(ti);
		}
	}

	for(unsigned i=0;i<(unsigned)model->numMaterials;i++)
	{
		rr::RRMaterial s;
		fillMaterial(&s,&model->Materials[i]);
		materials.push_back(s);
	}

#ifdef VERIFY
	verify();
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);

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
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
		case rr_gl::CHANNEL_TRIANGLE_EMISSIVE_TEX:
			if(numItems) *numItems = RRObject3DS::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr_gl::Texture*);
			return;
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV:
			if(numItems) *numItems = RRObject3DS::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			// unsupported channel
			RRMesh::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRObject3DS::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		assert(0);
		return false;
	}
	switch(channelId)
	{
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
		case rr_gl::CHANNEL_TRIANGLE_EMISSIVE_TEX:
		{
			if(itemIndex>=RRObject3DS::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			unsigned materialIndex = triangles[itemIndex].s;
			if(materialIndex>=materials.size())
			{
				assert(0); // illegal
				return false;
			}
			typedef rr_gl::Texture* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			*out = model->Materials[materialIndex].tex;
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV:
		{
			if(itemIndex>=RRObject3DS::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRVec2 Out[3];
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			Triangle triangle;
			RRObject3DS::getTriangle(itemIndex,triangle);
			for(unsigned v=0;v<3;v++)
			{
				(*out)[v][0] = object->TexCoords[2*triangle[v]];
				(*out)[v][1] = object->TexCoords[2*triangle[v]+1];
			}
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
		{
			if(itemIndex>=RRObject3DS::getNumTriangles())
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRObjectIllumination* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
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
	if(t>=RRObject3DS::getNumTriangles()) 
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

const rr::RRMaterial* RRObject3DS::getTriangleMaterial(unsigned t) const
{
	if(t>=RRObject3DS::getNumTriangles())
	{
		assert(0);
		return NULL;
	}
	unsigned s = triangles[t].s;
	if(s>=materials.size())
	{
		assert(0);
		return NULL;
	}
	return &materials[s];
}

void RRObject3DS::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if(t>=RRObject3DS::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	RRObject3DS::getTriangle(t,triangle);
	for(unsigned v=0;v<3;v++)
	{
		out.norm[v][0] = object->Normals[3*triangle[v]];
		out.norm[v][1] = object->Normals[3*triangle[v]+1];
		out.norm[v][2] = object->Normals[3*triangle[v]+2];
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

const rr::RRMatrix3x4* RRObject3DS::getInvWorldMatrix()
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFrom3DS

class ObjectsFrom3DS : public rr::RRStaticObjects
{
public:
	ObjectsFrom3DS(Model_3DS* model)
	{
		for(unsigned i=0;i<(unsigned)model->numObjects;i++)
		{
			RRObject3DS* object = new RRObject3DS(model,i);
			push_back(rr::RRStaticObject(object,object->getIllumination()));
		}
	}
	virtual ~ObjectsFrom3DS()
	{
		for(unsigned i=0;i<size();i++)
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

rr::RRStaticObjects* adaptObjectsFrom3DS(Model_3DS* model)
{
	return new ObjectsFrom3DS(model);
}
