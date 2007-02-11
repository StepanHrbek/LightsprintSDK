// --------------------------------------------------------------------------
// Imports TMapQ3 into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to TMapQ3 meshes, objects, materials.
// You can replace TMapQ3 with your internal format and adapt this code
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
// custom .bsp data via our custom identifiers CHANNEL_SURFACE_DIF_TEX etc.
// It is used only by our custom renderer RendererOfRRObject
// (during render of scene with ambient maps),
// it is never accessed by radiosity solver.
// You may skip it in your implementation.

#include <cassert>
#include <cmath>
#include <vector>
#include "RRIllumination.h"
#include "RRObjectBSP.h"


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

#define VERIFY

#ifdef VERIFY
void reporter(const char* msg, void* context)
{
	puts(msg);
}
#endif


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectBSP : public rr::RRObject, rr::RRMesh
{
public:
	RRObjectBSP(de::TMapQ3* model, const char* pathToTextures);
	rr::RRObjectIllumination* getIllumination();
	virtual ~RRObjectBSP();

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	//virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const;
	//virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual unsigned                getTriangleSurface(unsigned t) const;
	virtual const rr::RRSurface*    getSurface(unsigned s) const;
	//virtual void                    getTriangleNormals(unsigned t, TriangleNormals& out) const;

private:
	de::TMapQ3* model;

	// copy of object's geometry
	struct TriangleInfo
	{
		rr::RRMesh::Triangle t;
		unsigned s; // surface index
	};
	std::vector<TriangleInfo> triangles;

	// copy of object's surface properties
	struct SurfaceInfo
	{
		rr::RRSurface surface;
		de::Texture* texture;
	};
	std::vector<SurfaceInfo> surfaces;
	
	// collider for ray-mesh collisions
	rr::RRCollider* collider;

	// indirect illumination (ambient maps etc)
	rr::RRObjectIllumination* illumination;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP load

// Inputs: m
// Outputs: t, s
static void fillSurface(rr::RRSurface& s,de::Texture*& t,de::TTexture* m,const char* pathToTextures)
{
	enum {size = 8};

	// load texture
	char* exts[3]={".jpg",".png",".tga"};
	for(unsigned e=0;e<3;e++)
	{
		char buf[300];
		_snprintf(buf,299,"%s%s%s",pathToTextures,m->mName,exts[e]);
		buf[299]=0;
		t = de::Texture::load(buf,NULL);
		if(t) break;
	}

	// for diffuse textures provided by bsp,
	// it is sufficient to compute average texture color
	rr::RRColor avg = rr::RRColor(0);
	if(t)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				rr::RRColor tmp;
				t->getPixel(i/(float)size,j/(float)size,&tmp[0]);
				avg += tmp;
			}
		avg /= size*size;
	}

	// set all properties to default
	s.reset(0);

	// set diffuse reflectance according to bsp material
	s.diffuseReflectance = avg;

#ifdef VERIFY
	if(s.validate())
		reporter("Surface adjusted to physically valid.",NULL);
#else
	s.validate();
#endif
}

// Creates internal copies of .bsp geometry and surface properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObjectBSP::RRObjectBSP(de::TMapQ3* amodel, const char* pathToTextures)
{
	model = amodel;

	for(unsigned i=0;i<(unsigned)model->mTextures.size();i++)
	{
		SurfaceInfo si;
		fillSurface(si.surface,si.texture,&model->mTextures[i],pathToTextures);
		surfaces.push_back(si);
	}

	//for(unsigned i=0;i<(unsigned)model->mFaces.size();i++)
	for(unsigned s=0;s<surfaces.size();s++)
	for(unsigned i=model->mModels[0].mFace;i<(unsigned)(model->mModels[0].mFace+model->mModels[0].mNbFaces);i++)
	{
		if(model->mFaces[i].mTextureIndex==s)
		if(surfaces[model->mFaces[i].mTextureIndex].texture)
		{
			if(model->mFaces[i].mType==1)
			{
				for(unsigned j=(unsigned)model->mFaces[i].mMeshVertex;j<(unsigned)(model->mFaces[i].mMeshVertex+model->mFaces[i].mNbMeshVertices);j+=3)
				{
					TriangleInfo ti;
					ti.t[0] = model->mFaces[i].mVertex + model->mMeshVertices[j  ].mMeshVert;
					ti.t[1] = model->mFaces[i].mVertex + model->mMeshVertices[j+2].mMeshVert;
					ti.t[2] = model->mFaces[i].mVertex + model->mMeshVertices[j+1].mMeshVert;
					ti.s = model->mFaces[i].mTextureIndex;
					triangles.push_back(ti);
				}
			}
			//else
			//if(model->mFaces[i].mType==3)
			//{
			//	for(unsigned j=0;j<(unsigned)model->mFaces[i].mNbVertices;j+=3)
			//	{
			//		TriangleInfo ti;
			//		ti.t[0] = model->mFaces[i].mVertex+j;
			//		ti.t[1] = model->mFaces[i].mVertex+j+1;
			//		ti.t[2] = model->mFaces[i].mVertex+j+2;
			//		ti.s = model->mFaces[i].mTextureIndex;
			//		triangles.push_back(ti);
			//	}
			//}
		}
	}

#ifdef VERIFY
	verify(reporter,NULL);
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);

	// create illumination
	illumination = new rr::RRObjectIllumination((unsigned)model->mVertices.size());
}

rr::RRObjectIllumination* RRObjectBSP::getIllumination()
{
	return illumination;
}

RRObjectBSP::~RRObjectBSP()
{
	delete illumination;
	delete collider;
}

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP implements RRChanneledData

void RRObjectBSP::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr_gl::CHANNEL_SURFACE_DIF_TEX:
			if(numItems) *numItems = model->mTextures.size();
			if(itemSize) *itemSize = sizeof(de::Texture*);
			return;
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
			if(numItems) *numItems = RRObjectBSP::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			assert(0); // legal, but shouldn't happen in well coded program
			if(numItems) *numItems = 0;
			if(itemSize) *itemSize = 0;
	}
}

bool RRObjectBSP::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
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
			if(itemIndex>=(unsigned)model->mTextures.size())
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
			*out = surfaces[itemIndex].texture;
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIF_UV:
		{
			if(itemIndex>=RRObjectBSP::getNumTriangles())
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
			RRObjectBSP::getTriangle(itemIndex,triangle);
			for(unsigned v=0;v<3;v++)
			{
				(*out)[v][0] = model->mVertices[triangle[v]].mTexCoord[0][0];
				(*out)[v][1] = model->mVertices[triangle[v]].mTexCoord[0][1];
			}
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
		{
			if(itemIndex>=RRObjectBSP::getNumTriangles())
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
// RRObjectBSP implements RRMesh

unsigned RRObjectBSP::getNumVertices() const
{
	return (unsigned)model->mVertices.size();
}

void RRObjectBSP::getVertex(unsigned v, Vertex& out) const
{
	assert(v<(unsigned)model->mVertices.size());
	out[0] = model->mVertices[v].mPosition[0]*0.03f;
	out[1] = model->mVertices[v].mPosition[2]*0.03f;
	out[2] = -model->mVertices[v].mPosition[1]*0.03f;
}

//unsigned RRObjectBSP::getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const
//{
//	return postImportVertex;
//}

//unsigned RRObjectBSP::getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const
//{
//	return preImportVertex;
//}

unsigned RRObjectBSP::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObjectBSP::getTriangle(unsigned t, Triangle& out) const
{
	if(t>=RRObjectBSP::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out = triangles[t].t;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP implements RRObject

const rr::RRCollider* RRObjectBSP::getCollider() const
{
	return collider;
}

unsigned RRObjectBSP::getTriangleSurface(unsigned t) const
{
	if(t>=RRObjectBSP::getNumTriangles())
	{
		assert(0);
		return UINT_MAX;
	}
	unsigned s = triangles[t].s;
	assert(s<surfaces.size());
	return s;
}

const rr::RRSurface* RRObjectBSP::getSurface(unsigned s) const
{
	if(s>=surfaces.size()) 
	{
		assert(0);
		return NULL;
	}
	return &surfaces[s].surface;
}
/*
void RRObjectBSP::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if(t>=RRObjectBSP::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	RRObjectBSP::getTriangle(t,triangle);
	for(unsigned v=0;v<3;v++)
	{
		out.norm[v][0] = model->mVertices[triangle[v]].mNormal[0];
		out.norm[v][1] = model->mVertices[triangle[v]].mNormal[1];
		out.norm[v][2] = model->mVertices[triangle[v]].mNormal[2];
	}
}
*/
// Unwrap is not present in .bsp file format.
// If you omit getTriangleMapping (as it happens here), emergency automatic unwrap
// is used and ambient map quality is reduced.
//void RRObjectBSP::getTriangleMapping(unsigned t, TriangleMapping& out) const
//{
//	out.uv = unwrap baked with mesh;
//}


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjectBSP* new_bsp_importer(de::TMapQ3* model, const char* pathToTextures)
{
	RRObjectBSP* importer = new RRObjectBSP(model,pathToTextures);
#ifdef VERIFY
	importer->getCollider()->getMesh()->verify(reporter,NULL);
#endif
	return importer;
}

void insertBspToRR(de::TMapQ3* model,const char* pathToTextures,rr::RRRealtimeRadiosity* app,const rr::RRScene::SmoothingParameters* smoothing)
{
	if(app)
	{
		rr::RRRealtimeRadiosity::Objects objects;
		RRObjectBSP* object = new_bsp_importer(model,pathToTextures);
		objects.push_back(rr::RRRealtimeRadiosity::Object(object,object->getIllumination()));
		app->setObjects(objects,smoothing);
	}
}

void deleteBspFromRR(rr::RRRealtimeRadiosity* app)
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
