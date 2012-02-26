// --------------------------------------------------------------------------
// Lightsprint adapters for Quake3 map (.bsp).
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
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


////////////////////////////////////////////////////////////////////////////
//
// utility functions

static RRVec2 convertUv(const float* a)
{
	return RRVec2(a[0],a[1]);
}

static RRVec3 convertPos(const float* a)
{
	// To become compatible with our conventions, positions are transformed:
	// - Y / Z components swapped to get correct up vector
	// - one component negated to swap front/back
	// - *0.015f to convert to meters
	return RRVec3(a[0],a[2],-a[1])*0.015f;
}

TVertex TVertex::operator+(const TVertex& a) const
{
	TVertex result;
	for (unsigned i=0;i<10;i++) result.mPosition[i] = (RRReal)(mPosition[i]+a.mPosition[i]);
	for (unsigned i=0;i<4;i++) result.mColor[i] = (unsigned char)RR_CLAMPED(mColor[i]+a.mColor[i],0,255);
	return result;
}

TVertex TVertex::operator*(double a) const
{
	TVertex result;
	for (unsigned i=0;i<10;i++) result.mPosition[i] = (RRReal)(mPosition[i]*a);
	for (unsigned i=0;i<4;i++) result.mColor[i] = (unsigned char)RR_CLAMPED(mColor[i]*a,0,255);
	return result;
}


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
	std::vector<RRMesh::Triangle> triangles;
#ifdef PACK_VERTICES
	struct VertexInfo
	{
		RRVec3 position;
		RRVec2 texCoordDiffuse;
		RRVec2 texCoordLightmap;
		VertexInfo()
		{
		}
		VertexInfo(RRVec3 _position)
		{
			position = _position;
			texCoordDiffuse = RRVec2(0);
			texCoordLightmap = RRVec2(0);
		}
		VertexInfo(const TVertex& a, unsigned lightmapIndex, unsigned numLightmaps)
		{
			position = convertPos(a.mPosition);
			texCoordDiffuse = convertUv(a.mTexCoord[0]);
			texCoordLightmap = convertUv(a.mTexCoord[1]);

			// as we merge all objects, we must merge also lightmaps
			unsigned w = (unsigned)(sqrt((float)numLightmaps)+0.99999f);
			unsigned h = (numLightmaps+w-1)/w;
			unsigned x = lightmapIndex % w;
			unsigned y = lightmapIndex / w;
			texCoordLightmap[0] = ( texCoordLightmap[0] + x)/w;
			texCoordLightmap[1] = ( texCoordLightmap[1] + y)/h;
		}
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
bool g_lightsmarkAttic = false;
bool g_lightsmarkCloister = false;

// Inputs: m
// Outputs: t, s
static void fillMaterial(RRMaterial& s, TTexture* m, const RRFileLocator* textureLocator)
{
	enum {size = 8}; // use 8x8 samples to detect average texture color

	// load texture
	const char* strippedName = m->mName;
	while (strchr(strippedName,'/') || strchr(strippedName,'\\')) strippedName++;
	bool knownMissingTexture = (g_lightsmarkAttic && !(strcmp(strippedName,"poltergeist") && strcmp(strippedName,"flare") && strcmp(strippedName,"padtele_green") && strcmp(strippedName,"padjump_green") && strcmp(strippedName,"padbubble"))) // temporary: don't report known missing textures in Lightsmark
		|| (g_lightsmarkCloister && !(strcmp(strippedName,"utopiaatoll")));
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
		unsigned numTrianglesBeforeFacegroup = triangles.size();
		RRMaterial* material = new RRMaterial;
		fillMaterial(*material,&model->mTextures[s],textureLocator);
		if (material->diffuseReflectance.texture)
		for (unsigned mdl=0;mdl<(unsigned)(model->mModels.size());mdl++)
		for (unsigned f=model->mModels[mdl].mFace;f<(unsigned)(model->mModels[mdl].mFace+model->mModels[mdl].mNbFaces);f++)
		if (model->mFaces[f].mTextureIndex==s)
		{
			if (model->mFaces[f].mType==1) // 1=polygon
			{
				for (unsigned j=(unsigned)model->mFaces[f].mMeshVertex;j<(unsigned)(model->mFaces[f].mMeshVertex+model->mFaces[f].mNbMeshVertices);j+=3)
				{
					RRMesh::Triangle ti;
					ti[0] = model->mFaces[f].mVertex + model->mMeshVertices[j  ].mMeshVert;
					ti[1] = model->mFaces[f].mVertex + model->mMeshVertices[j+2].mMeshVert;
					ti[2] = model->mFaces[f].mVertex + model->mMeshVertices[j+1].mMeshVert;

					// clip parts of scene never visible in Lightsmark 2008
					if (g_lightsmarkAttic)
					{
						unsigned clipped = 0;
						for (unsigned v=0;v<3;v++)
						{
							RRVec3 pos = convertPos(model->mVertices[ti[v]].mPosition);
							if (pos.y<-18.2f || pos.z>11.5f) clipped++;
							//if (x<23||x>24||y<-6||y>-4||z<-5||z>-3) clipped++; // zoom on geometry errors
						}
						if (clipped==3) continue;
					}

#ifdef PACK_VERTICES
					// pack vertices, remove unused
					for (unsigned v=0;v<3;v++)
					{
						if (xlat[ti[v]]==UINT_MAX)
						{
							xlat[ti[v]] = (unsigned)vertices.size();
							vertices.push_back(VertexInfo(model->mVertices[ti[v]],model->mFaces[f].mLightmapIndex,model->mLightMaps.size()));
						}
						ti[v] = xlat[ti[v]];
					}
#endif

					triangles.push_back(ti);
				}
			}
			else
			if (model->mFaces[f].mType==2) // 2=patch
			{
				// patch consists of segments LxL squares big, L1*L1 vertices big
				const TFace& patch = model->mFaces[f];
				int L = 4;
				int L1 = L+1;

				// add vertices
				unsigned vertexBase = (unsigned)vertices.size();
				unsigned vertexW = (patch.mPatchSize[0]-1)/2*L+1;
				unsigned vertexH = (patch.mPatchSize[1]-1)/2*L+1;
				vertices.resize(vertexBase+vertexW*vertexH);
				for (int y=0;y<(patch.mPatchSize[1]-1)/2;y++)
				for (int x=0;x<(patch.mPatchSize[0]-1)/2;x++)
				{
					TVertex controls[9];
					for (unsigned c=0;c<9;c++)
						controls[c] = model->mVertices[patch.mVertex+patch.mPatchSize[0]*(2*y+c/3)+2*x+(c%3)];

					unsigned vertexBase2 = vertexBase + vertexW*L*y + L*x;
					for (int j=0;j<L1;j++)
					{
						double a = (double)j/L;
						double b = 1-a;
						vertices[vertexBase2+j*vertexW] = VertexInfo(
							controls[0] * (b * b) + 
							controls[3] * (2 * b * a) +
							controls[6] * (a * a),
							patch.mLightmapIndex,model->mLightMaps.size());
					}
					for (int i=1;i<L1;i++)
					{
						double a = (double)i/L;
						double b = 1.0 - a;
						TVertex temp[3];
						for (int j=0;j<3;j++)
						{
							int k = 3 * j;
							temp[j] =
								controls[k + 0] * (b * b) + 
								controls[k + 1] * (2 * b * a) +
								controls[k + 2] * (a * a);
						}
						for(int j=0;j<L1;j++)
						{
							double a = (double)j/L;
							double b = 1.0 - a;
							vertices[vertexBase2+j*vertexW+i] = VertexInfo(
								temp[0] * (b * b) + 
								temp[1] * (2 * b * a) +
								temp[2] * (a * a),
								patch.mLightmapIndex,model->mLightMaps.size());
						}
					}
				}

				// add indices
				for (unsigned x=0;x+1<vertexW;x++)
				for (unsigned y=0;y+1<vertexH;y++)
				{
					RRMesh::Triangle ti;
					ti[0] = vertexBase+vertexW*(y  )+(x  );
					ti[1] = vertexBase+vertexW*(y  )+(x+1);
					ti[2] = vertexBase+vertexW*(y+1)+(x+1);
					triangles.push_back(ti);
					ti[1] = vertexBase+vertexW*(y+1)+(x+1);
					ti[2] = vertexBase+vertexW*(y+1)+(x  );
					triangles.push_back(ti);
				}
			}
			//else
			//if (model->mFaces[i].mType==3) // 3=mesh
			//{
			//	for (unsigned j=0;j<(unsigned)model->mFaces[i].mNbVertices;j+=3)
			//	{
			//		RRMesh::Triangle ti;
			//		ti[0] = model->mFaces[i].mVertex+j;
			//		ti[1] = model->mFaces[i].mVertex+j+1;
			//		ti[2] = model->mFaces[i].mVertex+j+2;
			//		triangles.push_back(ti);
			//	}
			//}
		}
		// create facegroup for used material / delete unused material
		if (triangles.size()-numTrianglesBeforeFacegroup)
		{
			materials.push_back(material);
			faceGroups.push_back(FaceGroup(material,triangles.size()-numTrianglesBeforeFacegroup));
		}
		else
			delete material;
	}

	// Lightsmark: add outer side of walls (fixes strips of sunlight leaking inside through thin 1-sided walls)
	if (g_lightsmarkCloister)
	{
		RRVec3 mini,maxi;
		getAABB(&mini,&maxi,NULL);
		mini -= (maxi-mini)*0.01f;
		maxi += (maxi-mini)*0.01f;
		RR_SAFE_DELETE(aabbCache);
		unsigned vertexBase = (unsigned)vertices.size();
		vertices.push_back(VertexInfo(RRVec3(mini.x,mini.y,mini.z)));
		vertices.push_back(VertexInfo(RRVec3(mini.x,maxi.y,mini.z)));
		vertices.push_back(VertexInfo(RRVec3(mini.x,mini.y,maxi.z)));
		vertices.push_back(VertexInfo(RRVec3(mini.x,maxi.y,maxi.z)));
		vertices.push_back(VertexInfo(RRVec3(maxi.x,mini.y,maxi.z)));
		vertices.push_back(VertexInfo(RRVec3(maxi.x,maxi.y,maxi.z)));
		vertices.push_back(VertexInfo(RRVec3(maxi.x,mini.y,mini.z)));
		vertices.push_back(VertexInfo(RRVec3(maxi.x,maxi.y,mini.z)));
		unsigned triangleBase = (unsigned)triangles.size();
		for (unsigned i=0;i<4;i++)
		{
			RRMesh::Triangle ti;
			ti[0] = vertexBase+i*2;
			ti[1] = vertexBase+(i*2+3)%8;
			ti[2] = vertexBase+i*2+1;
			triangles.push_back(ti);
			ti[1] = vertexBase+(i*2+2)%8;
			ti[2] = vertexBase+(i*2+3)%8;
			triangles.push_back(ti);
		}
		RRMaterial* material = new RRMaterial;
		material->reset(false);
		materials.push_back(material);
		faceGroups.push_back(RRObject::FaceGroup(material,8));
	}

#ifdef PACK_VERTICES
	delete[] xlat;
#endif

	// create collider
	bool aborting = false;
	setCollider(RRCollider::create(this,NULL,RRCollider::IT_LINEAR,aborting));
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
	out = convertPos(model->mVertices[v].mPosition);
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
	out = triangles[t];
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
			g_lightsmarkAttic = strstr(filename.c_str(),"wop_padattic")!=NULL;
			g_lightsmarkCloister = strstr(filename.c_str(),"wop_padcloister")!=NULL;
			scene->protectedObjects = adaptObjectsFromTMapQ3(&scene->scene_bsp,textureLocator);
			g_lightsmarkCloister = false;
			g_lightsmarkAttic = false;
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
