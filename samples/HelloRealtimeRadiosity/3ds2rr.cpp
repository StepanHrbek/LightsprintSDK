// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to Model_3DS meshes, objects, materials.
// You can replace Model_3DS with your internal format and adapt this code
// so it works with your data.
//
// This code is sufficient for demonstration purposes, but for production code,
// more memory can be saved:
// 1) Don't duplicate data into this object. Reimplement methods like getVertex
//    so that they read from your original mesh, sizeof this class can go down
//    to several bytes.
// 2) For geometry instancing, split class into one that implements RRMesh
//    and one that implements RRObject.
//    You can then reuse meshes and colliders, multiple objects
//    (instances of geometry in scene) will share the same collider and mesh.
//    Even when properly written mesh has several bytes (it's only wrapper),
//    collider can be big.
//
// RRChanneledData - the biggest part of this implementation, provides access to
// custom .3ds data via our custom identifiers CHANNEL_SURFACE_DIF_TEX etc.
// It is used only by our custom renderer RendererOfRRObject
// (during render of scene with ambient maps),
// it is never accessed by radiosity solver.
// You may skip it in your implementation.


#include <cassert>
#include <cmath>
#include <vector>
#include "RRIllumination.h"
#include "3ds2rr.h"


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
// M3dsImporter

// See RRObject and RRMesh documentation for details
// on individual member functions.

class M3dsImporter : public rr::RRObject, rr::RRMesh
{
public:
	M3dsImporter(de::Model_3DS* model, unsigned objectIdx);
	rr::RRObjectIllumination* getIllumination();
	virtual ~M3dsImporter();

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual unsigned                getTriangleSurface(unsigned t) const;
	virtual const rr::RRSurface*    getSurface(unsigned s) const;
	virtual void                    getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual const rr::RRMatrix3x4*  getWorldMatrix();
	virtual const rr::RRMatrix3x4*  getInvWorldMatrix();

private:
	de::Model_3DS* model;
	de::Model_3DS::Object* object;

	// copy of object's geometry
	struct TriangleInfo
	{
		rr::RRMesh::Triangle t;
		unsigned s; // surface index
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's surface properties
	std::vector<rr::RRSurface> surfaces;
	
	// collider for ray-mesh collisions
	rr::RRCollider* collider;

	// indirect illumination (ambient maps etc)
	rr::RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter load

static void fillSurface(rr::RRSurface* s,de::Model_3DS::Material* m)
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
				rr::RRColor tmp;
				m->tex->getPixel(i/(float)size,j/(float)size,&tmp[0]);
				avg += tmp;
			}
		avg /= size*size;
	}
	else
	{
		avg[0] = m->color.r;
		avg[1] = m->color.g;
		avg[2] = m->color.b;
	}

	// set all properties to default
	s->reset(0);

	// set diffuse reflectance according to 3ds material
	s->diffuseReflectance = avg;

#ifdef VERIFY
	if(s->validate())
		reporter("Surface adjusted to physically valid.",NULL);
#else
	s->validate();
#endif
}

// Creates internal copies of .3ds geometry and surface properties.
// Implementation is simpler with internal copies, although less memory efficient.
M3dsImporter::M3dsImporter(de::Model_3DS* amodel, unsigned objectIdx)
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
		rr::RRSurface s;
		fillSurface(&s,&model->Materials[i]);
		surfaces.push_back(s);
	}

#ifdef VERIFY
	verify(reporter,NULL);
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);

	// create illumination
	illumination = new rr::RRObjectIllumination((unsigned)object->numVerts);
}

rr::RRObjectIllumination* M3dsImporter::getIllumination()
{
	return illumination;
}

M3dsImporter::~M3dsImporter()
{
	delete illumination;
	delete collider;
}

//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRChanneledData

void M3dsImporter::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr_gl::CHANNEL_SURFACE_DIF_TEX:
			if(numItems) *numItems = model->numMaterials;
			if(itemSize) *itemSize = sizeof(de::Texture*);
			return;
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			if(numItems) *numItems = M3dsImporter::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			assert(0); // legal, but shouldn't happen in well coded program
			if(numItems) *numItems = 0;
			if(itemSize) *itemSize = 0;
	}
}

bool M3dsImporter::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		assert(0);
		return false;
	}
	switch(channelId)
	{
		case rr_gl::CHANNEL_SURFACE_DIF_TEX:
		{
			if(itemIndex>=(unsigned)model->numMaterials)
			{
				assert(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef de::Texture* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				assert(0);
				return false;
			}
			*out = model->Materials[itemIndex].tex;
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
		{
			if(itemIndex>=M3dsImporter::getNumTriangles())
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
			M3dsImporter::getTriangle(itemIndex,triangle);
			for(unsigned v=0;v<3;v++)
			{
				(*out)[v][0] = object->TexCoords[2*triangle[v]];
				(*out)[v][1] = object->TexCoords[2*triangle[v]+1];
			}
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
		{
			if(itemIndex>=M3dsImporter::getNumTriangles())
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
			assert(0); // legal, but shouldn't happen in well coded program
			return false;
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRMesh

unsigned M3dsImporter::getNumVertices() const
{
	return object->numVerts;
}

void M3dsImporter::getVertex(unsigned v, Vertex& out) const
{
	assert(v<(unsigned)object->numVerts);
	out[0] = object->Vertexes[3*v];
	out[1] = object->Vertexes[3*v+1];
	out[2] = object->Vertexes[3*v+2];
}

unsigned M3dsImporter::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void M3dsImporter::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=M3dsImporter::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out = triangles[t].t;
}


//////////////////////////////////////////////////////////////////////////////
//
// M3dsImporter implements RRObject

const rr::RRCollider* M3dsImporter::getCollider() const
{
	return collider;
}

unsigned M3dsImporter::getTriangleSurface(unsigned t) const
{
	if(t>=M3dsImporter::getNumTriangles())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<surfaces.size());
	return s;
}

const rr::RRSurface* M3dsImporter::getSurface(unsigned s) const
{
	if(s>=surfaces.size()) 
	{
		assert(0);
		return NULL;
	}
	return &surfaces[s];
}

void M3dsImporter::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if(t>=M3dsImporter::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	M3dsImporter::getTriangle(t,triangle);
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
//void M3dsImporter::getTriangleMapping(unsigned t, TriangleMapping& out) const
//{
//	out.uv = unwrap baked with mesh;
//}

const rr::RRMatrix3x4* M3dsImporter::getWorldMatrix()
{
	// transformation matrices from 3ds are ignored
	return NULL;
}

const rr::RRMatrix3x4* M3dsImporter::getInvWorldMatrix()
{
	return NULL;
}


//////////////////////////////////////////////////////////////////////////////
//
// main

M3dsImporter* new_3ds_importer(de::Model_3DS* model, unsigned objectIdx)
{
	M3dsImporter* importer = new M3dsImporter(model, objectIdx);
#ifdef VERIFY
	importer->getCollider()->getMesh()->verify(reporter,NULL);
#endif
	return importer;
}

void insert3dsToRR(de::Model_3DS* model,rr::RRRealtimeRadiosity* app,const rr::RRScene::SmoothingParameters* smoothing)
{
	if(app)
	{
		rr::RRRealtimeRadiosity::Objects objects;
		for(unsigned i=0;i<(unsigned)model->numObjects;i++)
		{
			M3dsImporter* object = new_3ds_importer(model,i);
			objects.push_back(rr::RRRealtimeRadiosity::Object(object,object->getIllumination()));
		}
		app->setObjects(objects,smoothing);
	}
}

void delete3dsFromRR(rr::RRRealtimeRadiosity* app)
{
	if(app)
	{
		for(unsigned i=0;i<app->getNumObjects();i++)
		{
			// no need to delete illumination separately, we created it as part of object
			//delete app->getIllumination(i);
			delete app->getObject(i);
		}
	}
}
