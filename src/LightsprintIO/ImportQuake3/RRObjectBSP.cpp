// --------------------------------------------------------------------------
// Lightsprint adapters for Quake3 map (.bsp).
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_QUAKE3

// This code implements data adapters for access to TMapQ3 meshes, objects, materials.
// You can replace TMapQ3 with your internal format and adapt this code
// so it works with your data.

#include <cmath>
#include <cstdio>
#include <vector>
#include "RRObjectBSP.h"
#include <boost/filesystem.hpp> // parent_path()
namespace bf = boost::filesystem;

#define PACK_VERTICES // reindex vertices, remove unused ones (optimization that makes vertex buffers smaller)

using namespace rr;


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectQuake3

// See RRObject and RRMesh documentation for details
// on individual member functions.

class RRObjectQuake3 : public RRObject, public RRMesh
{
public:
	RRObjectQuake3(TMapQ3* model, const RRFileLocator* textureLocator);
	virtual ~RRObjectQuake3();

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	//virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;

private:
	TMapQ3* model;

	// copy of object's geometry
	struct TriangleInfo
	{
		RRMesh::Triangle t;
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
	std::vector<RRMaterial*> materials;
};

// texcoord channels
enum
{
	CH_LIGHTMAP,
	CH_DIFFUSE
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectQuake3 load

// enables Lightsmark 2008 specific code. it affects only wop_padattic scene, you can safely delete it
bool g_lightsmark = false;

// Inputs: m
// Outputs: t, s
static void fillMaterial(RRMaterial& s, TTexture* m, const RRFileLocator* textureLocator)
{
	enum {size = 8}; // use 8x8 samples to detect average texture color

	// load texture
	const char* strippedName = m->mName;
	while (strchr(strippedName,'/') || strchr(strippedName,'\\')) strippedName++;
	bool knownMissingTexture = g_lightsmark && !(strcmp(strippedName,"poltergeist") && strcmp(strippedName,"flare") && strcmp(strippedName,"padtele_green") && strcmp(strippedName,"padjump_green") && strcmp(strippedName,"padbubble")); // temporary: don't report known missing textures in Lightsmark
	RRBuffer* t = knownMissingTexture ? NULL : RRBuffer::load(m->mName,NULL,textureLocator);
	if (t)
		t->flip(false,true,false);

	// for diffuse textures provided by bsp,
	// it is sufficient to compute average texture color
	RRVec4 avg(0);
	if (t)
	{
		for (unsigned i=0;i<size;i++)
			for (unsigned j=0;j<size;j++)
			{
				avg += t->getElementAtPosition(RRVec3(i/(float)size,j/(float)size,0));
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
	s.specularTransmittance.texture = (!t || avg[3]==1) ? NULL : t->createReference();
	s.specularTransmittance.texcoord = CH_DIFFUSE;
	s.specularTransmittanceInAlpha = true;
	s.lightmapTexcoord = CH_LIGHTMAP;
	s.minimalQualityForPointMaterials = s.specularTransmittance.texture ? 30 : 300;

	// get average colors from textures. we already set them, but this will be slightly more precise thanks to scaler
	RRScaler* scaler = RRScaler::createFastRgbScaler();
	s.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
	delete scaler;
	// apply quake map boost again, updateColorsFromTextures removed it
	s.diffuseReflectance.color *= 2;

	// autodetect keying
	s.updateKeyingFromTransmittance();

	// optimize material flags
	s.updateSideBitsFromColors();
}

// Creates internal copies of .bsp geometry and material properties.
// Implementation is simpler with internal copies, although less memory efficient.
RRObjectQuake3::RRObjectQuake3(TMapQ3* amodel, const RRFileLocator* textureLocator)
{
	model = amodel;

#ifdef PACK_VERTICES
	// prepare for unused vertex removal
	unsigned* xlat = new unsigned[model->mVertices.size()]; // old vertexIdx to new vertexIdx, UINT_MAX=unknown yet
	for (unsigned i=0;i<(unsigned)model->mVertices.size();i++) xlat[i] = UINT_MAX;
#endif

	for (unsigned s=0;s<(unsigned)model->mTextures.size();s++)
	{
		unsigned numTrianglesInFacegroup = 0;
		RRMaterial* material = new RRMaterial;
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
							fillMaterial(*material,&model->mTextures[s],textureLocator);
							triedLoadTexture = true;
						}
						// if texture was loaded, accept triangles, otherwise ignore them
						if (material->diffuseReflectance.texture)
						{
							TriangleInfo ti;
							ti.t[0] = model->mFaces[f].mVertex + model->mMeshVertices[j  ].mMeshVert;
							ti.t[1] = model->mFaces[f].mVertex + model->mMeshVertices[j+2].mMeshVert;
							ti.t[2] = model->mFaces[f].mVertex + model->mMeshVertices[j+1].mMeshVert;

							// clip parts of scene never visible in Lightsmark 2008
							if (g_lightsmark)
							{
								unsigned clipped = 0;
								for (unsigned v=0;v<3;v++)
								{
									//float x = model->mVertices[ti.t[v]].mPosition[0]*0.015f;
									float y = model->mVertices[ti.t[v]].mPosition[2]*0.015f;
									float z = -model->mVertices[ti.t[v]].mPosition[1]*0.015f;
									if (y<-18.2f || z>11.5f) clipped++;
									//if (x<23||x>24||y<-6||y>-4||z<-5||z>-3) clipped++; // zoom on geometry errors
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
							numTrianglesInFacegroup++;
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
		faceGroups.push_back(FaceGroup(material,numTrianglesInFacegroup));
	}

#ifdef PACK_VERTICES
	delete[] xlat;
#endif

	// create collider
	bool aborting = false;
	setCollider(RRCollider::create(this,RRCollider::IT_LINEAR,aborting));
}

RRObjectQuake3::~RRObjectQuake3()
{
	for (unsigned i=0;i<(unsigned)materials.size();i++)
	{
		delete materials[i];
	}
	delete getCollider();
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectQuake3 implements RRMesh

unsigned RRObjectQuake3::getNumVertices() const
{
#ifdef PACK_VERTICES
	return (unsigned)vertices.size();
#else
	return (unsigned)model->mVertices.size();
#endif
}

void RRObjectQuake3::getVertex(unsigned v, Vertex& out) const
{
	RR_ASSERT(v<RRObjectQuake3::getNumVertices());
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

unsigned RRObjectQuake3::getNumTriangles() const
{
	return (unsigned)triangles.size();
}

void RRObjectQuake3::getTriangle(unsigned t, Triangle& out) const
{
	if (t>=RRObjectQuake3::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	out = triangles[t].t;
}

/*
void RRObjectQuake3::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	if (t>=RRObjectQuake3::getNumTriangles())
	{
		RR_ASSERT(0);
		return;
	}
	Triangle triangle;
	RRObjectQuake3::getTriangle(t,triangle);
	for (unsigned v=0;v<3;v++)
	{
		// nejsem si jisty jestli je to takhle dobre
		out.norm[v][0] = model->mVertices[triangle[v]].mNormal[0];
		out.norm[v][1] = model->mVertices[triangle[v]].mNormal[2];
		out.norm[v][2] = -model->mVertices[triangle[v]].mNormal[1];
	}
}*/

bool RRObjectQuake3::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (t>=RRObjectQuake3::getNumTriangles())
	{
		RR_ASSERT(0);
		return false;
	}
	if (channel!=CH_DIFFUSE && channel!=CH_LIGHTMAP)
	{
		return false;
	}
	Triangle triangle;
	RRObjectQuake3::getTriangle(t,triangle);
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
// RRObjectsQuake3

class RRObjectsQuake3 : public RRObjects
{
public:
	RRObjectsQuake3(TMapQ3* model,RRFileLocator* textureLocator)
	{
		RRObjectQuake3* object = new RRObjectQuake3(model,textureLocator);
		push_back(object);
	}
	virtual ~RRObjectsQuake3()
	{
		delete (*this)[0];
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneQuake3

class RRSceneQuake3 : public RRScene
{
public:
	static RRScene* load(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
	{
		RRSceneQuake3* scene = new RRSceneQuake3;
		if (!readMap(RR_RR2CHAR(filename),filename.w_str(),scene->scene_bsp)) // bsp outside windows does not support unicode filename
		{
			delete scene;
			RRReporter::report(WARN,"Failed loading scene %ls.\n",filename.w_str());
			return NULL;
		}
		else
		{
			if (textureLocator)
			{
				// tweak locator for .bsp
				textureLocator->setLibrary(true,RR_PATH2RR(bf::path(RR_RR2PATH(filename)).parent_path()));
				textureLocator->setExtensions(true,".jpg;.png;.tga");
			}
			g_lightsmark = strstr(filename.c_str(),"wop_padattic")!=NULL;
			scene->protectedObjects = adaptObjectsFromTMapQ3(&scene->scene_bsp,textureLocator);
			g_lightsmark = false;
			if (textureLocator)
			{
				// undo local changes
				textureLocator->setLibrary(false,RR_PATH2RR(bf::path(RR_RR2PATH(filename)).parent_path()));
				textureLocator->setExtensions(false,".jpg;.png;.tga");
			}
			return scene;
		}
	}
	virtual ~RRSceneQuake3()
	{
		freeMap(scene_bsp);
	}
private:
	TMapQ3 scene_bsp;
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model, RRFileLocator* textureLocator)
{
	return new RRObjectsQuake3(model,textureLocator);
}

void registerLoaderQuake3()
{
	RRScene::registerLoader("*.bsp",RRSceneQuake3::load);
}

#endif // SUPPORT_QUAKE3
