// --------------------------------------------------------------------------
// Creates Lightsprint interface for Quake3 map (.bsp)
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

// This code implements data wrappers for access to TMapQ3 meshes, objects, materials.
// You can replace TMapQ3 with your internal format and adapt this code
// so it works with your data.
//
// For sake of simplicity, some data are duplicated here.
// See RRObjectCollada as an example of wrapper with
// nearly zero memory requirements, no duplications.
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
#include "RRObjectBSP.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "GL/glew.h"

//#define MARK_OPENED // mark used textures by read-only attribute
#ifdef MARK_OPENED
	#include <io.h>
	#include <sys/stat.h>
#endif

#define PACK_VERTICES // reindex vertices, remove unused ones


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new wrappers.
// Define VERIFY to enable verification of wrappers and data.
// Display system messages using RRReporter.
// Once your code/data are verified and don't emit messages via RRReporter,
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectBSP : public rr::RRObject, public rr::RRMesh
{
public:
	RRObjectBSP(TMapQ3* model, const char* pathToTextures, bool stripPaths, rr_gl::Texture* missingTexture);
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
	//virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	//virtual unsigned     getPreImportVertex(unsigned postImportVertex, unsigned postImportTriangle) const;
	//virtual unsigned     getPostImportVertex(unsigned preImportVertex, unsigned preImportTriangle) const;

	// RRObject
	virtual const rr::RRCollider*   getCollider() const;
	virtual const rr::RRMaterial*   getTriangleMaterial(unsigned t, const rr::RRLight* light, const RRObject* receiver) const;

private:
	TMapQ3* model;

	// copy of object's geometry
	struct TriangleInfo
	{
		rr::RRMesh::Triangle t;
		unsigned s; // material index
	};
	std::vector<TriangleInfo> triangles;
#ifdef PACK_VERTICES
	struct VertexInfo
	{
		rr::RRVec3 position;
		rr::RRVec2 texCoord;
	};
	std::vector<VertexInfo> vertices;
#endif

	// copy of object's material properties
	struct MaterialInfo
	{
		rr::RRMaterial material;
		rr_gl::Texture* texture;
	};
	std::vector<MaterialInfo> materials;
	
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
static void fillMaterial(rr::RRMaterial& s, rr_gl::Texture*& t, TTexture* m,const char* pathToTextures, bool stripPaths, rr_gl::Texture* fallback)
{
	enum {size = 8}; // use 8x8 samples to detect average texture color

	// load texture
	rr::RRReporter* oldReporter = rr::RRReporter::getReporter();
	rr::RRReporter::setReporter(NULL); // disable reporting temporarily, we don't know image extension so we try all of them
	const char* strippedName = m->mName;
	if(stripPaths)
	{
		while(strchr(strippedName,'/') || strchr(strippedName,'\\')) strippedName++;
	}
	char* exts[3]={".jpg",".png",".tga"};
	for(unsigned e=0;e<3;e++)
	{
		char buf[300];
		_snprintf(buf,299,"%s%s%s",pathToTextures,strippedName,exts[e]);
		buf[299]=0;
		t = rr_gl::Texture::load(buf,NULL,true,false,GL_LINEAR,GL_LINEAR_MIPMAP_LINEAR,GL_REPEAT,GL_REPEAT);
#ifdef MARK_OPENED
		if(t) _chmod(buf,_S_IREAD); // mark opened files read only
#endif
		//if(t) {puts(buf);break;}
		if(t) break;
		//if(e==2) printf("Not found: %s\n",buf);
	}
	rr::RRReporter::setReporter(oldReporter);
	if(!t)
	{
		t = fallback;
		if(strcmp(strippedName,"poltergeist") && strcmp(strippedName,"flare") && strcmp(strippedName,"padtele_green") && strcmp(strippedName,"padjump_green"))
			rr::RRReporter::report(rr::ERRO,"Can't load texture %s%s.*\n",pathToTextures,strippedName);
	}

	// for diffuse textures provided by bsp,
	// it is sufficient to compute average texture color
	rr::RRVec4 avg = rr::RRVec4(0);
	if(t)
	{
		for(unsigned i=0;i<size;i++)
			for(unsigned j=0;j<size;j++)
			{
				rr::RRVec4 tmp;
				t->getPixel(i/(float)size,j/(float)size,0,&tmp.x);
				avg += tmp;
			}
		avg /= size*size*0.5f; // 0.5 for quake map boost
		avg[3] *= 0.5f; // but not for alpha
		if(avg[3]==0) avg[3]=1; // all pixels 100% transparent? must be special material we can't handle, make it fully opaque, better than invisible
	}

	// set all properties to default
	s.reset(0);
	// rgb is diffuse reflectance
	s.diffuseReflectance = avg;
	// alpha is transparency
	s.specularTransmittance = rr::RRColor(1-avg[3]);

#ifdef VERIFY
	if(s.validate())
		rr::RRReporter::report(rr::WARN,"Material adjusted to physically valid.\n");
#else
	s.validate();
#endif
}

// Creates internal copies of .bsp geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObjectBSP::RRObjectBSP(TMapQ3* amodel, const char* pathToTextures, bool stripPaths, rr_gl::Texture* missingTexture)
{
	model = amodel;

#ifdef PACK_VERTICES
	// prepare for unused vertex removal
	unsigned* xlat = new unsigned[model->mVertices.size()]; // old vertexIdx to new vertexIdx, UINT_MAX=unknown yet
	for(unsigned i=0;i<(unsigned)model->mVertices.size();i++) xlat[i] = UINT_MAX;
#endif

	for(unsigned s=0;s<(unsigned)model->mTextures.size();s++)
	{
		MaterialInfo si;
		bool triedLoadTexture = false;
		for(unsigned mdl=0;mdl<(unsigned)(model->mModels.size());mdl++)
		for(unsigned i=model->mModels[mdl].mFace;i<(unsigned)(model->mModels[mdl].mFace+model->mModels[mdl].mNbFaces);i++)
		{
			if(model->mFaces[i].mTextureIndex==s)
			//if(materials[model->mFaces[i].mTextureIndex].texture)
			{
				if(model->mFaces[i].mType==1)
				{
					for(unsigned j=(unsigned)model->mFaces[i].mMeshVertex;j<(unsigned)(model->mFaces[i].mMeshVertex+model->mFaces[i].mNbMeshVertices);j+=3)
					{
						// try load texture when it is mapped on at least 1 triangle
						if(!triedLoadTexture)
						{
							fillMaterial(si.material,si.texture,&model->mTextures[s],pathToTextures,stripPaths,missingTexture);
							triedLoadTexture = true;
						}
						// if texture was loaded, accept triangles, otherwise ignore them
						if(si.texture)
						{
							TriangleInfo ti;
							ti.t[0] = model->mFaces[i].mVertex + model->mMeshVertices[j  ].mMeshVert;
							ti.t[1] = model->mFaces[i].mVertex + model->mMeshVertices[j+2].mMeshVert;
							ti.t[2] = model->mFaces[i].mVertex + model->mMeshVertices[j+1].mMeshVert;
							ti.s = model->mFaces[i].mTextureIndex;

#ifdef PACK_VERTICES
							// pack vertices, remove unused
							for(unsigned i=0;i<3;i++)
							{
								if(xlat[ti.t[i]]==UINT_MAX)
								{
									xlat[ti.t[i]] = vertices.size();
									VertexInfo vi;
									vi.position[0] = model->mVertices[ti.t[i]].mPosition[0]*0.015f;
									vi.position[1] = model->mVertices[ti.t[i]].mPosition[2]*0.015f;
									vi.position[2] = -model->mVertices[ti.t[i]].mPosition[1]*0.015f;
									vi.texCoord[0] = model->mVertices[ti.t[i]].mTexCoord[0][0];
									vi.texCoord[1] = model->mVertices[ti.t[i]].mTexCoord[0][1];
									vertices.push_back(vi);
								}
								ti.t[i] = xlat[ti.t[i]];
							}
#endif

							triangles.push_back(ti);
						}
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
		// push all materials to preserve material numbering
		materials.push_back(si);
	}

#ifdef PACK_VERTICES
	delete[] xlat;
#endif

#ifdef VERIFY
	verify();
#endif

	// create collider
	collider = rr::RRCollider::create(this,rr::RRCollider::IT_LINEAR);

	// create illumination
	illumination = new rr::RRObjectIllumination(RRObjectBSP::getNumVertices());
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
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
			if(numItems) *numItems = RRObjectBSP::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr_gl::Texture*);
			return;
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
			if(numItems) *numItems = RRObjectBSP::getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRVec2[3]);
			return;
		default:
			// unsupported channel
			RRMesh::getChannelSize(channelId,numItems,itemSize);
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
		case rr_gl::CHANNEL_TRIANGLE_DIFFUSE_TEX:
		{
			if(itemIndex>=RRObjectBSP::getNumTriangles())
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
			*out = materials[materialIndex].texture;
			return true;
		}
		case rr_gl::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
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
#ifdef PACK_VERTICES
				(*out)[v] = vertices[triangle[v]].texCoord;
#else
				(*out)[v][0] = model->mVertices[triangle[v]].mTexCoord[0][0];
				(*out)[v][1] = model->mVertices[triangle[v]].mTexCoord[0][1];
#endif
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
			// unsupported channel
			return RRMesh::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP implements RRMesh

unsigned RRObjectBSP::getNumVertices() const
{
#ifdef PACK_VERTICES
	return (unsigned)vertices.size();
#else
	return (unsigned)model->mVertices.size();
#endif
}

void RRObjectBSP::getVertex(unsigned v, Vertex& out) const
{
	assert(v<RRObjectBSP::getNumVertices());
#ifdef PACK_VERTICES
	out = vertices[v].position;
#else
	// To become compatible with our conventions, positions are transformed:
	// - Y / Z components swapped to get correct up vector
	// - one component negated to swap front/back
	// - *0.015f to convert to meters
	out[0] = model->mVertices[v].mPosition[0]*0.015f;
	out[1] = model->mVertices[v].mPosition[2]*0.015f;
	out[2] = -model->mVertices[v].mPosition[1]*0.015f;
#endif
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
		// nejsem si jisty jestli je to takhle dobre
		out.norm[v][0] = model->mVertices[triangle[v]].mNormal[0];
		out.norm[v][1] = model->mVertices[triangle[v]].mNormal[2];
		out.norm[v][2] = -model->mVertices[triangle[v]].mNormal[1];
	}
}*/


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP implements RRObject

const rr::RRCollider* RRObjectBSP::getCollider() const
{
	return collider;
}

const rr::RRMaterial* RRObjectBSP::getTriangleMaterial(unsigned t, const rr::RRLight* light, const RRObject* receiver) const
{
	if(t>=RRObjectBSP::getNumTriangles())
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
	return &materials[s].material;
}

// Unwrap is not present in .bsp file format.
// If you omit getTriangleMapping (as it happens here), emergency automatic unwrap
// is used and ambient map quality is reduced.
//void RRObjectBSP::getTriangleMapping(unsigned t, TriangleMapping& out) const
//{
//	out.uv = unwrap baked with mesh;
//}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromTMapQ3

class ObjectsFromTMapQ3 : public rr::RRObjects
{
public:
	ObjectsFromTMapQ3(TMapQ3* model,const char* pathToTextures,bool stripPaths,rr_gl::Texture* missingTexture)
	{
		RRObjectBSP* object = new RRObjectBSP(model,pathToTextures,stripPaths,missingTexture);
		push_back(rr::RRIlluminatedObject(object,object->getIllumination()));
	}
	virtual ~ObjectsFromTMapQ3()
	{
		// no need to delete illumination separately, we created it as part of object
		//delete (*this)[0].illumination;
		delete (*this)[0].object;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model,const char* pathToTextures,bool stripPaths,rr_gl::Texture* missingTexture)
{
	return new ObjectsFromTMapQ3(model,pathToTextures,stripPaths,missingTexture);
}
