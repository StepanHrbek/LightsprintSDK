// --------------------------------------------------------------------------
// Creates Lightsprint interface for Quake3 map (.bsp)
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_QUAKE3

// This code implements data adapters for access to TMapQ3 meshes, objects, materials.
// You can replace TMapQ3 with your internal format and adapt this code
// so it works with your data.

#include <cassert>
#include <cmath>
#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "RRObjectBSP.h"

//#define MARK_OPENED // mark used textures by read-only attribute
#ifdef MARK_OPENED
	#include <io.h>
	#include <sys/stat.h>
#endif

#define PACK_VERTICES // reindex vertices, remove unused ones (optimization that makes vertex buffers smaller)

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new adapters.
// Define VERIFY to enable verification of adapters and data.
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

class RRObjectBSP : public RRObject, public RRMesh
{
public:
	RRObjectBSP(TMapQ3* model, const char* pathToTextures, bool stripPaths, RRBuffer* missingTexture);
	RRObjectIllumination* getIllumination();
	virtual ~RRObjectBSP();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	//virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;

	// RRObject
	virtual const RRCollider*   getCollider() const;
	virtual const RRMaterial*   getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const;

private:
	TMapQ3* model;

	// copy of object's geometry
	struct TriangleInfo
	{
		RRMesh::Triangle t;
		unsigned s; // material index
	};
	std::vector<TriangleInfo> triangles;
#ifdef PACK_VERTICES
	struct VertexInfo
	{
		RRVec3 position;
		RRVec2 texCoordDiffuse;
		RRVec2 texCoordLightmap;
	};
	std::vector<VertexInfo> vertices;
#endif

	// copy of object's materials
	std::vector<RRMaterial> materials;
	
	// collider for ray-mesh collisions
	const RRCollider* collider;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination* illumination;
};

// texcoord channels
enum
{
	CH_LIGHTMAP,
	CH_DIFFUSE
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP load

// Inputs: m
// Outputs: t, s
static void fillMaterial(RRMaterial& s, TTexture* m,const char* pathToTextures, bool stripPaths, RRBuffer* fallback)
{
	enum {size = 8}; // use 8x8 samples to detect average texture color

	// load texture
	RRBuffer* t = NULL;
	RRReporter* oldReporter = RRReporter::getReporter();
	RRReporter::setReporter(NULL); // disable reporting temporarily, we don't know image extension so we try all of them
	const char* strippedName = m->mName;
	if (stripPaths)
	{
		while (strchr(strippedName,'/') || strchr(strippedName,'\\')) strippedName++;
	}
	const char* exts[3]={".jpg",".png",".tga"};
	for (unsigned e=0;e<3;e++)
	{
		char buf[300];
		_snprintf(buf,299,"%s%s%s",pathToTextures,strippedName,exts[e]);
		buf[299]=0;
		t = RRBuffer::load(buf,NULL,true,false);
#ifdef MARK_OPENED
		if (t) _chmod(buf,_S_IREAD); // mark opened files read only
#endif
		//if (t) {puts(buf);break;}
		if (t) break;
		//if (e==2) printf("Not found: %s\n",buf);
	}
	RRReporter::setReporter(oldReporter);
	if (!t)
	{
		t = fallback;
		if (strcmp(strippedName,"poltergeist") && strcmp(strippedName,"flare") && strcmp(strippedName,"padtele_green") && strcmp(strippedName,"padjump_green") && strcmp(strippedName,"padbubble")) // temporary: don't report known missing textures in Lightsmark
			RRReporter::report(ERRO,"Can't load texture %s%s.*\n",pathToTextures,strippedName);
	}

	// for diffuse textures provided by bsp,
	// it is sufficient to compute average texture color
	RRVec4 avg = RRVec4(0);
	if (t)
	{
		for (unsigned i=0;i<size;i++)
			for (unsigned j=0;j<size;j++)
			{
				avg += t->getElement(RRVec3(i/(float)size,j/(float)size,0));
			}
		avg /= size*size*0.5f; // 0.5 for quake map boost
		avg[3] *= 0.5f; // but not for alpha
		if (avg[3]==0) avg[3]=1; // all pixels 100% transparent? must be special material we can't handle, make it fully opaque, better than invisible
	}

	// set all properties to default
	s.reset(avg[3]!=1); // make transparent materials twosided
	// rgb is diffuse reflectance
	s.diffuseReflectance.color = avg;
	s.diffuseReflectance.texture = t;
	s.diffuseReflectance.texcoord = CH_DIFFUSE;
	// alpha is transparency (1=opaque)
	s.specularTransmittance.color = RRVec3(1-avg[3]);
	s.specularTransmittance.texture = (avg[3]==1) ? NULL : t;
	s.specularTransmittance.texcoord = CH_DIFFUSE;
	s.specularTransmittanceInAlpha = true;
	s.lightmapTexcoord = CH_LIGHTMAP;
	s.minimalQualityForPointMaterials = s.specularTransmittance.texture ? 30 : 300;
}

// Creates internal copies of .bsp geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObjectBSP::RRObjectBSP(TMapQ3* amodel, const char* pathToTextures, bool stripPaths, RRBuffer* missingTexture)
{
	model = amodel;

	// Lightsmark 2008 specific code. it affects only wop_padattic scene, but you can safely delete it
	bool lightsmark = strstr(pathToTextures,"wop_padattic")!=NULL;

#ifdef PACK_VERTICES
	// prepare for unused vertex removal
	unsigned* xlat = new unsigned[model->mVertices.size()]; // old vertexIdx to new vertexIdx, UINT_MAX=unknown yet
	for (unsigned i=0;i<(unsigned)model->mVertices.size();i++) xlat[i] = UINT_MAX;
#endif

	for (unsigned s=0;s<(unsigned)model->mTextures.size();s++)
	{
		RRMaterial material;
		bool triedLoadTexture = false;
		for (unsigned mdl=0;mdl<(unsigned)(model->mModels.size());mdl++)
		for (unsigned f=model->mModels[mdl].mFace;f<(unsigned)(model->mModels[mdl].mFace+model->mModels[mdl].mNbFaces);f++)
		{
			if (model->mFaces[f].mTextureIndex==s)
			//if (materials[model->mFaces[i].mTextureIndex].texture)
			{
				if (model->mFaces[f].mType==1)
				{
					for (unsigned j=(unsigned)model->mFaces[f].mMeshVertex;j<(unsigned)(model->mFaces[f].mMeshVertex+model->mFaces[f].mNbMeshVertices);j+=3)
					{
						// try load texture when it is mapped on at least 1 triangle
						if (!triedLoadTexture)
						{
							fillMaterial(material,&model->mTextures[s],pathToTextures,stripPaths,missingTexture);
							triedLoadTexture = true;
						}
						// if texture was loaded, accept triangles, otherwise ignore them
						if (material.diffuseReflectance.texture)
						{
							TriangleInfo ti;
							ti.t[0] = model->mFaces[f].mVertex + model->mMeshVertices[j  ].mMeshVert;
							ti.t[1] = model->mFaces[f].mVertex + model->mMeshVertices[j+2].mMeshVert;
							ti.t[2] = model->mFaces[f].mVertex + model->mMeshVertices[j+1].mMeshVert;
							ti.s = model->mFaces[f].mTextureIndex;

							// clip parts of scene never visible in Lightsmark 2008
							if (lightsmark)
							{
								unsigned clipped = 0;
								for (unsigned v=0;v<3;v++)
								{
									float y = model->mVertices[ti.t[v]].mPosition[2]*0.015f;
									float z = -model->mVertices[ti.t[v]].mPosition[1]*0.015f;
									if (y<-18.2f || z>11.5f) clipped++;
								}
								if (clipped==3) continue;
							}

#ifdef PACK_VERTICES
							// pack vertices, remove unused
							for (unsigned v=0;v<3;v++)
							{
								if (xlat[ti.t[v]]==UINT_MAX)
								{
									xlat[ti.t[v]] = (unsigned)vertices.size();
									VertexInfo vi;
									vi.position[0] = model->mVertices[ti.t[v]].mPosition[0]*0.015f;
									vi.position[1] = model->mVertices[ti.t[v]].mPosition[2]*0.015f;
									vi.position[2] = -model->mVertices[ti.t[v]].mPosition[1]*0.015f;
									vi.texCoordDiffuse[0] = model->mVertices[ti.t[v]].mTexCoord[0][0];
									vi.texCoordDiffuse[1] = model->mVertices[ti.t[v]].mTexCoord[0][1];

									// as we merge all objects, we must merge also lightmaps
									unsigned numLightmaps = (unsigned)model->mLightMaps.size();
									unsigned w = (unsigned)(sqrt((float)numLightmaps)+0.99999f);
									unsigned h = (numLightmaps+w-1)/w;
									unsigned x = model->mFaces[f].mLightmapIndex % w;
									unsigned y = model->mFaces[f].mLightmapIndex / w;
									vi.texCoordLightmap[0] = ( model->mVertices[ti.t[v]].mTexCoord[1][0] + x)/w;
									vi.texCoordLightmap[1] = ( model->mVertices[ti.t[v]].mTexCoord[1][1] + y)/h;

									vertices.push_back(vi);
								}
								ti.t[v] = xlat[ti.t[v]];
							}
#endif

							triangles.push_back(ti);
						}
					}
				}
			//else
			//if (model->mFaces[i].mType==3)
			//{
			//	for (unsigned j=0;j<(unsigned)model->mFaces[i].mNbVertices;j+=3)
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
		materials.push_back(material);
	}

#ifdef PACK_VERTICES
	delete[] xlat;
#endif

#ifdef VERIFY
	checkConsistency();
#endif

	// create collider
	bool aborting = false;
	collider = RRCollider::create(this,RRCollider::IT_LINEAR,aborting);

	// create illumination
	illumination = new RRObjectIllumination(RRObjectBSP::getNumVertices());
}

RRObjectIllumination* RRObjectBSP::getIllumination()
{
	return illumination;
}

RRObjectBSP::~RRObjectBSP()
{
	for (unsigned i=0;i<(unsigned)materials.size();i++) delete materials[i].diffuseReflectance.texture;
	delete illumination;
	delete collider;
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

unsigned RRObjectBSP::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObjectBSP::getTriangle(unsigned t, Triangle& out) const
{
	if (t>=RRObjectBSP::getNumTriangles()) 
	{
		assert(0);
		return;
	}
	out = triangles[t].t;
}

/*
void RRObjectBSP::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=RRObjectBSP::getNumTriangles())
	{
		assert(0);
		return;
	}
	Triangle triangle;
	RRObjectBSP::getTriangle(t,triangle);
	for (unsigned v=0;v<3;v++)
	{
		// nejsem si jisty jestli je to takhle dobre
		out.norm[v][0] = model->mVertices[triangle[v]].mNormal[0];
		out.norm[v][1] = model->mVertices[triangle[v]].mNormal[2];
		out.norm[v][2] = -model->mVertices[triangle[v]].mNormal[1];
	}
}*/

bool RRObjectBSP::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (t>=RRObjectBSP::getNumTriangles())
	{
		RR_ASSERT(0);
		return false;
	}
	if (channel!=CH_DIFFUSE && channel!=CH_LIGHTMAP)
	{
		return false;
	}
	Triangle triangle;
	RRObjectBSP::getTriangle(t,triangle);
	for (unsigned v=0;v<3;v++)
	{
#ifdef PACK_VERTICES
		out.uv[v] = (channel==CH_LIGHTMAP)
			? vertices[triangle[v]].texCoordLightmap
			: vertices[triangle[v]].texCoordDiffuse;
#else
		out.uv[v][0] = model->mVertices[triangle[v]].mTexCoord[0][0];
		out.uv[v][1] = model->mVertices[triangle[v]].mTexCoord[0][1];
#endif
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectBSP implements RRObject

const RRCollider* RRObjectBSP::getCollider() const
{
	return collider;
}

const RRMaterial* RRObjectBSP::getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
{
	if (t>=RRObjectBSP::getNumTriangles())
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


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromTMapQ3

class ObjectsFromTMapQ3 : public RRObjects
{
public:
	ObjectsFromTMapQ3(TMapQ3* model,const char* pathToTextures,bool stripPaths,RRBuffer* missingTexture)
	{
		RRObjectBSP* object = new RRObjectBSP(model,pathToTextures,stripPaths,missingTexture);
		push_back(RRIlluminatedObject(object,object->getIllumination()));
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

RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model,const char* pathToTextures,bool stripPaths,RRBuffer* missingTexture)
{
	return new ObjectsFromTMapQ3(model,pathToTextures,stripPaths,missingTexture);
}

#endif // SUPPORT_QUAKE3
