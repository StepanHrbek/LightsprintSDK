// --------------------------------------------------------------------------
// Lightsprint adapters for FCollada document.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_COLLADA

// This code implements data adapters for access to Collada meshes,
// objects, materials, loaded by FCollada library.
// You can replace Collada with your internal format and adapt this code
// so it works with your data.
//
// Adapters don't allocate additional memory, values are read from 
// Collada document.
//
// Instancing is supported, multiple instances with different
// positions and materials share one collider and mesh.
//
// What TEXCOORD channel is used for lightmaps?
// 1. channel assigned to ambient maps, if present
// 2. otherwise the highest channel number present
// This is not necessarily the one you intended for lightmaps.
// You may want to tweak this code, search for LIGHTMAP_TEXCOORD below.
//
// Internal units are automatically converted to meters.
//
// 'Up' vector is automatically converted to 0,1,0 (Y positive).

#include <cmath>
#include <map>

#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "FCDocument/FCDocumentTools.h"
#include "FCDocument/FCDAsset.h"
#include "FCDocument/FCDEffect.h"
#include "FCDocument/FCDEffectProfile.h"
#include "FCDocument/FCDEffectStandard.h"
#include "FCDocument/FCDEffectTechnique.h"
#include "FCDocument/FCDGeometry.h"
#include "FCDocument/FCDGeometryInstance.h"
#include "FCDocument/FCDGeometryMesh.h"
#include "FCDocument/FCDGeometryPolygons.h"
#include "FCDocument/FCDGeometryPolygonsInput.h"
#include "FCDocument/FCDGeometryPolygonsTools.h"
#include "FCDocument/FCDGeometrySource.h"
#include "FCDocument/FCDImage.h"
#include "FCDocument/FCDLibrary.h"
#include "FCDocument/FCDLight.h"
#include "FCDocument/FCDMaterial.h"
#include "FCDocument/FCDMaterialInstance.h"
#include "FCDocument/FCDSceneNode.h"
#include "FCDocument/FCDTexture.h"
#include "FCDocument/FCDVersion.h"
#include "FUtils/FUFileManager.h"

#include "RRObjectCollada.h"

#ifdef _MSC_VER
	#if _MSC_VER<1400
		#error "Third party library FCollada doesn't support VS2003".
	#else
		#pragma comment(lib,"FCollada.lib")
	#endif
#endif

using namespace rr;

enum
{
	// fixed channel numbers reserved for Collada adapter
	// must be <100 for realtime renderer (it uses only first 100 channels)
	// if these numbers collide with numbers in your data, change these numbers
	LIGHTMAP_CHANNEL = 86,
	UNSPECIFIED_CHANNEL = 87,
};

//////////////////////////////////////////////////////////////////////////////
//
// Verificiation
//
// Helps during development of new adapters.
// Define VERIFY to enable verification of adapters and data.
// RRReporter will be used to warn about detected data inconsistencies.
// Once your code/data are verified and don't emit messages via RRReporter,
// turn verifications off.
// If you encounter strange behaviour with new data later,
// reenable verifications to check that your data are ok.

//#define VERIFY


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada

// See RRMesh documentation for details
// on individual member functions.

class RRMeshCollada : public RRMesh
{
public:
	RRMeshCollada(const FCDGeometryMesh* _mesh);

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual bool         getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const;

private:
	const FCDGeometryMesh* mesh;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada load

// Doesn't create mesh copy, stores only pointer, so it depends on original collada mesh.
RRMeshCollada::RRMeshCollada(const FCDGeometryMesh* _mesh)
{
	mesh = _mesh;
}

// for non TEXCOORD semantics, inputSet is ignored
// for TEXCOORD semantic, inputSet is mandatory
//   for lightmaps, inputSet is LIGHTMAP_CHANNEL and we read data from the highest valid inputSet.
//   for broken documents without binding, inputSet is UNSPECIFIED_CHANNEL and we read data from first valid inputSet.
bool getTriangleVerticesData(const FCDGeometryMesh* mesh, FUDaeGeometryInput::Semantic semantic, unsigned inputSet, unsigned floatsPerVertexExpected, unsigned itemIndex, void* itemData, unsigned itemSize)
{
	RR_ASSERT(itemSize==12*floatsPerVertexExpected);
	for (size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if (polygons)
		{
			FCDGeometryPolygons::PrimitiveType primitiveType = polygons->GetPrimitiveType();
			if (primitiveType==FCDGeometryPolygons::POLYGONS || primitiveType==FCDGeometryPolygons::TRIANGLE_FANS || primitiveType==FCDGeometryPolygons::TRIANGLE_STRIPS)
			{
				RR_LIMITED_TIMES(10,RR_ASSERT(polygons->TestPolyType()==3)); // this is expensive check, do it only few times
				size_t relativeIndex = itemIndex - polygons->GetFaceOffset();
				if (relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
				{
					bool reverse = inputSet==LIGHTMAP_CHANNEL; // iterate from highest to lowers inputSet
					for (size_t m=reverse?polygons->GetInputCount():0; reverse?m--:(m<polygons->GetInputCount()); reverse?0:m++)
					{
						const FCDGeometryPolygonsInput* polygonsInput = polygons->GetInput(m);
						if (polygonsInput && polygonsInput->GetSemantic()==semantic &&
							(
								semantic!=FUDaeGeometryInput::TEXCOORD ||

								// typical data, optional "set" specified
								// <input semantic="TEXCOORD" source="#lobby_ceiling-mesh-map-channel1" offset="2" set="1"/> -> GetSet=1
								// <bind_vertex_input semantic="CHANNEL1" input_semantic="TEXCOORD" input_set="1"/> -> inputSet=1
								polygonsInput->GetSet()==inputSet ||

								// data from MeshLab, optional "set" not specified
								// <input offset="2" semantic="TEXCOORD" source="#shape0-lib-map"/> -> GetSet=-1 (default in fcollda)
								// <bind_vertex_input semantic="UVSET0" input_semantic="TEXCOORD"/> -> inputSet=0 (default in fcollda)
								(inputSet==0 && polygonsInput->GetSet()==-1) ||

								inputSet==LIGHTMAP_CHANNEL ||
								inputSet==UNSPECIFIED_CHANNEL
							))
						{
							const FCDGeometrySource* source = polygonsInput->GetSource();
							if (source)
							{
								const float* data = source->GetData();
								size_t dataCount = source->GetDataCount();
								unsigned floatsPerVertexPresent = source->GetStride();
								const uint32* indices = polygonsInput->GetIndices();
								if (indices)
								{
									//RR_ASSERT(relativeIndex*3+2<polygonsInput->GetIndexCount());
									if (relativeIndex*3+2>=polygonsInput->GetIndexCount())
									{
										int polyType = polygons->TestPolyType();
										RR_ASSERT(polyType==3);
										RR_ASSERT(0);
									}
									float* out = (float*)itemData;
									for (unsigned j=0;j<3;j++)
									{
										for (unsigned k=0;k<floatsPerVertexExpected;k++)
										{
											unsigned dataIndex = indices[relativeIndex*3+j]*floatsPerVertexPresent+k;
											if (dataIndex>=dataCount)
											{
												RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Out of range indices in Collada file.\n"));
												return false;
											}
											*out++ = data[dataIndex];
										}
									}
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	// we ask for geotangents even if they don't exist etc, so don't assert
	//RR_ASSERT(0);
	return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRMesh

unsigned RRMeshCollada::getNumVertices() const
{
	const FCDGeometrySource* source = mesh->GetVertexSource(0);
	if (!source)
	{
		RR_ASSERT(0);
		return 0;
	}
	return (unsigned)source->GetValueCount();
}

void RRMeshCollada::getVertex(unsigned v, Vertex& out) const
{
	RR_ASSERT(v<RRMeshCollada::getNumVertices());
	const FCDGeometrySource* source = mesh->GetVertexSource(0);
	if (!source)
	{
		RR_ASSERT(0);
		return;
	}
	//RR_ASSERT(source->GetSourceStride()==3);
	memcpy(&out,source->GetValue(v),sizeof(out));
}

unsigned RRMeshCollada::getNumTriangles() const
{
	// mesh must be triangulated
	return (unsigned)mesh->GetFaceCount();
}

void RRMeshCollada::getTriangle(unsigned t, Triangle& out) const
{
	// mesh must be triangulated
	if (t>=RRMeshCollada::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	const FCDGeometrySource* source = mesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	if (!source)
	{
		RR_ASSERT(0);
		return;
	}
	for (size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if (polygons)
		{
			FCDGeometryPolygons::PrimitiveType primitiveType = polygons->GetPrimitiveType();
			if (primitiveType==FCDGeometryPolygons::POLYGONS || primitiveType==FCDGeometryPolygons::TRIANGLE_FANS || primitiveType==FCDGeometryPolygons::TRIANGLE_STRIPS)
			{
				if (t<polygons->GetFaceCount())
				{
					const FCDGeometryPolygonsInput* polygonsInput = polygons->FindInput(source);
					if (polygonsInput)
					{
						const uint32* indices = polygonsInput->GetIndices();
						if (indices)
						{
							RR_ASSERT(3*t+2<polygonsInput->GetIndexCount());
							out[0] = indices[3*t+0];
							out[1] = indices[3*t+1];
							out[2] = indices[3*t+2];
							return;
						}
					}
					RR_ASSERT(0);
					return;
				}
				t -= (unsigned)polygons->GetFaceCount();
			}
		}
	}
	RR_ASSERT(0);
}

void RRMeshCollada::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	RRVec3 normal[3];
	RRVec3 tangent[3];
	RRVec3 bitangent[3];
	if (!getTriangleVerticesData(mesh,FUDaeGeometryInput::NORMAL,0,3,t,&normal,sizeof(normal)))
	{
		// all data generated
		RRMesh::getTriangleNormals(t,out);
	}
	else
	if (getTriangleVerticesData(mesh,FUDaeGeometryInput::GEOTANGENT,0,3,t,&tangent,sizeof(tangent)) && getTriangleVerticesData(mesh,FUDaeGeometryInput::GEOBINORMAL,0,3,t,&bitangent,sizeof(bitangent)))
	{
		// all data from collada
		for (unsigned v=0;v<3;v++)
		{
			out.vertex[v].normal = normal[v];
			out.vertex[v].tangent = tangent[v];
			out.vertex[v].bitangent = bitangent[v];
		}
	}
	else
	{
		// normals from collada, tangent+bitangent generated
		for (unsigned v=0;v<3;v++)
		{
			out.vertex[v].normal = normal[v];
			out.vertex[v].buildBasisFromNormal();
		}
	}
}

bool RRMeshCollada::getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
{
	if (getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,channel,2,t,&out,sizeof(out)))
	{
		// collada contains requested data
		return true;
	}
	if (channel==LIGHTMAP_CHANNEL)
	{
		// unwrap was not found, but we can autogenerate it
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"RRObjectCollada: No TEXCOORD channel for lightmaps, falling back to automatic.\n"));
		return RRMesh::getTriangleMapping(t,out,0);
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// MaterialCache
//
// data stored per material, shared by multiple objects
// includes textures, but not binding to texcoord channels

// Makes key in MaterialCache map FCDEffectStandard, not FCDMaterialInstance.
// + reduces number of unique materials, saves memory, makes render faster
// - introduces error if two instances of the same material use different uv channels
// When you see wrong uv, disable AGGRESSIVE_CACHE.
#define AGGRESSIVE_CACHE

RRVec3 colorToColor(FMVector4 color)
{
	return RRVec3(color.x,color.y,color.z);
}

RRReal colorToFloat(FMVector4 color)
{
	return (color.x+color.y+color.z)*0.333f;
}

RRVec4 getAvgColor(RRBuffer* buffer)
{
	enum {size = 8};
	RRVec4 avg = RRVec4(0);
	for (unsigned i=0;i<size;i++)
		for (unsigned j=0;j<size;j++)
			avg += buffer->getElement(RRVec3(i/(float)size,j/(float)size,0));
	return avg/(size*size);
}

class MaterialCacheCollada
{
public:
	MaterialCacheCollada(const char* _pathToTextures, float _emissiveMultiplier)
	{
		pathToTextures = _pathToTextures;
		emissiveMultiplier = _emissiveMultiplier;
		invertedA_ONETransparency = false;
		defaultMaterial.reset(false);
	}
	RRMaterial* getMaterial(const FCDMaterialInstance* materialInstance)
	{
		if (!materialInstance)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (material instance missing).\n"));
			return &defaultMaterial;
		}

#ifdef AGGRESSIVE_CACHE
		const FCDMaterial* material = materialInstance->GetMaterial();
		if (!material)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (material instance missing 2).\n"));
			return &defaultMaterial;
		}

		const FCDEffect* effect = material->GetEffect();
		if (!effect)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (effect missing).\n"));
			return &defaultMaterial;
		}

		const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
		if (!effectProfile)
		{
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (effect profile missing).\n"));
			return &defaultMaterial;
		}

		const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);
		Cache::iterator i = cache.find(effectStandard);
		if (i!=cache.end())
		{
			return i->second;
		}
		else
		{
			return cache[effectStandard] = loadMaterial(materialInstance,effectStandard);
		}
#else
		Cache::iterator i = cache.find(materialInstance);
		if (i!=cache.end())
		{
			return i->second;
		}
		else
		{
			const FCDMaterial* material = materialInstance->GetMaterial();
			if (!material)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (material instance missing 2).\n"));
				return &defaultMaterial;
			}

			const FCDEffect* effect = material->GetEffect();
			if (!effect)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (effect missing).\n"));
				return &defaultMaterial;
			}

			const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
			if (!effectProfile)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (effect profile missing).\n"));
				return &defaultMaterial;
			}

			const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);

			return cache[materialInstance] = loadMaterial(materialInstance,effectStandard);
		}
#endif
	}
	~MaterialCacheCollada()
	{
		// delete materials we created
		for (Cache::iterator i=cache.begin();i!=cache.end();++i)
		{
			delete i->second;
		}
	}
private:
	// _filename may contain path, it is used in first attempt
	// _pathToTextures should end by \ or /, it is used in second attempt
	static RRBuffer* loadTextureTwoPaths(const char* _filename, const char* _pathToTextures)
	{
		if (!_filename) return NULL;
		if (!_pathToTextures) return NULL; // we expect sanitized string
		RRBuffer* buffer = NULL;
		RRReporter* oldReporter = RRReporter::getReporter();
		RRReporter::setReporter(NULL); // disable reporting temporarily while we try texture load at 2 locations
		for (unsigned stripPaths=0;stripPaths<2;stripPaths++)
		{
			std::string strippedName = _filename;
			if (stripPaths)
			{
				size_t pos = strippedName.find_last_of("/\\");
				if (pos!=-1) strippedName.erase(0,pos+1);
			}
			if (stripPaths || (strippedName.size()>=2 && strippedName[0]!='/' && strippedName[0]!='\\' && strippedName[1]!=':'))
			{
				strippedName.insert(0,_pathToTextures);
			}
			buffer = rr::RRBuffer::load(strippedName.c_str(),NULL);
			if (buffer) break;
		}
		RRReporter::setReporter(oldReporter);
		if (!buffer)
		{
			std::string strippedName = _filename;
			if (strippedName.size()>=2 && strippedName[0]!='/' && strippedName[0]!='\\' && strippedName[1]!=':')
			{
				strippedName.insert(0,_pathToTextures);
			}
			RRReporter::report(WARN,"Can't load texture %s\n",strippedName.c_str());
		}
		return buffer;
	}
	void loadTexture(FUDaeTextureChannel::Channel channel, RRMaterial::Property& materialProperty, const FCDMaterialInstance* materialInstance, const FCDEffectStandard* effectStandard)
	{
		materialProperty.texture = NULL;
		materialProperty.texcoord = 0;
		const FCDTexture* texture = effectStandard->GetTextureCount(channel) ? effectStandard->GetTexture(channel,0) : NULL;
		if (texture)
		{
			// load texture
			const FCDImage* image = texture->GetImage();
			if (image)
			{
				const fstring& filename = image->GetFilename();
				materialProperty.texture = loadTextureTwoPaths(filename.c_str(),pathToTextures.c_str());
				if (materialProperty.texture)
				{
					materialProperty.color = getAvgColor(materialProperty.texture);
				}
			}
			// load texcoord
			if (materialInstance)
			{
				const FCDEffectParameterInt* set = texture->GetSet();
				if (set)
				{
					const FCDMaterialInstanceBindVertexInput* materialInstanceBindVertexInput = materialInstance->FindVertexInputBinding(set->GetSemantic());
					if (materialInstanceBindVertexInput) // is NULL in kalasatama.dae
					{
						// 1 for <input semantic="TEXCOORD" source="#lobby_ceiling-mesh-map-channel1" offset="2" set="1"/>
						// 0 for <input semantic="TEXCOORD" source="#lobby_ceiling-mesh-map-channel1" offset="2"/> (data from meshlab, 0 is default in fcollada)
						materialProperty.texcoord = materialInstanceBindVertexInput->inputSet;
					}
					else
					{
						materialProperty.texcoord = UNSPECIFIED_CHANNEL;
						RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Not a proper Collada 1.4.1 file (texcoord binding missing).\n"));
					}
				}
			}
		}
	}

	RRMaterial* loadMaterial(const FCDMaterialInstance* materialInstance, const FCDEffectStandard* effectStandard)
	{
		RRMaterial* materialPtr = new RRMaterial;
		RRMaterial& material = *materialPtr;
		const fchar* extra = effectStandard->GetExtraAttribute("MAX3D","double_sided");
		bool twoSided = !extra || extra[0]!='0'; // collada default is 2sided, but MAX3D extra '0' can change it to 1-sided (=not rendered, and photons hitting back side are deleted)
		material.reset(twoSided);
		material.diffuseReflectance.color = colorToColor(effectStandard->GetDiffuseColor());
		material.diffuseEmittance.color = colorToColor(effectStandard->GetEmissionFactor() * effectStandard->GetEmissionColor());
		material.specularReflectance.color = colorToColor(effectStandard->GetSpecularFactor() * effectStandard->GetSpecularColor());
		material.specularTransmittance.color = (effectStandard->GetTransparencyMode()==FCDEffectStandard::A_ONE)
			? RRVec3( 1 - effectStandard->GetTranslucencyFactor() * effectStandard->GetTranslucencyColor().w )
			: colorToColor( effectStandard->GetTranslucencyFactor() * effectStandard->GetTranslucencyColor() );
		if (effectStandard->GetTransparencyMode()==FCDEffectStandard::A_ONE)
		{
			if (effectStandard->GetTranslucencyFactor()==0 && !invertedA_ONETransparency)
			{
				invertedA_ONETransparency = true;
				RRReporter::report(WARN,"Transparency looks inverted. Enabling workaround for Google Sketch Up bug.\n");
			}
			if (invertedA_ONETransparency)
			{
				material.specularTransmittance.color = RRVec3(1)-material.specularTransmittance.color;
			}
		}
		material.refractionIndex = effectStandard->GetIndexOfRefraction();
		if (material.refractionIndex==0) material.refractionIndex = 1; // FCollada returns 0 when information is missing, but default refraction is 1

		loadTexture(FUDaeTextureChannel::DIFFUSE,material.diffuseReflectance,materialInstance,effectStandard);
		loadTexture(FUDaeTextureChannel::EMISSION,material.diffuseEmittance,materialInstance,effectStandard);
		material.diffuseEmittance.multiplyAdd(RRVec4(emissiveMultiplier),RRVec4(0));
		loadTexture(FUDaeTextureChannel::TRANSPARENT,material.specularTransmittance,materialInstance,effectStandard);
		material.specularTransmittanceInAlpha = effectStandard->GetTransparencyMode()==FCDEffectStandard::A_ONE;
		if (material.specularTransmittance.texture)
		{
			if (material.specularTransmittanceInAlpha)
			{
				RRVec4 avg = getAvgColor(material.specularTransmittance.texture);
				material.specularTransmittance.color = RRVec3(1-avg[3]);
			}

			if (effectStandard->GetTranslucencyFactor()!=1)
			{
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Translucency factor combined with texture ignored by Collada adapter.\n"));
			}
		}

		// workaround for Google Sketchup bug, diffuse map used as transparency map
		if (material.diffuseReflectance.texture && !material.specularTransmittance.texture && material.diffuseReflectance.texture->getFormat()==BF_RGBA)
		{
			RRReporter::report(WARN,"Transparency in diffuse map. Enabling workaround for Google Sketch Up bug.\n");
			material.specularTransmittance.texture = material.diffuseReflectance.texture->createReference();
			material.specularTransmittance.texcoord = material.diffuseReflectance.texcoord;
			material.specularTransmittanceInAlpha = true;
		}

		// get lightmapTexcoord
		//  default is to use channel with the highest number
		material.lightmapTexcoord = LIGHTMAP_CHANNEL;
		//  but if scene contains ambient map, use its channel
		const FCDTexture* texture = effectStandard->GetTextureCount(FUDaeTextureChannel::AMBIENT) ? effectStandard->GetTexture(FUDaeTextureChannel::AMBIENT,0) : NULL;
		if (texture && materialInstance)
		{
			const FCDEffectParameterInt* set = texture->GetSet();
			if (set)
			{
				const FCDMaterialInstanceBindVertexInput* materialInstanceBindVertexInput = materialInstance->FindVertexInputBinding(set->GetSemantic());
				if (materialInstanceBindVertexInput)
				{
					// wow, scene has uv channel marked as "this is for lightmaps" (ok, ambient maps)
					material.lightmapTexcoord = materialInstanceBindVertexInput->inputSet;
				}
			}
		}

		material.name = effectStandard->GetParent()->GetName().c_str();

		// get average colors from textures
		RRScaler* scaler = RRScaler::createRgbScaler();
		material.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
		delete scaler;

		// autodetect keying
		material.updateKeyingFromTransmittance();

		// optimize material flags
		material.updateSideBitsFromColors();

		return materialPtr;
	}

#ifdef AGGRESSIVE_CACHE
	typedef std::map<const FCDEffectStandard*,RRMaterial*> Cache;
#else
	typedef std::map<const FCDMaterialInstance*,RRMaterial*> Cache;
#endif
	Cache cache;

	RRString pathToTextures;
	float emissiveMultiplier;
	bool invertedA_ONETransparency; // workaround for Google Sketch Up bug
	RRMaterial defaultMaterial;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectCollada

// See RRObject documentation for details
// on individual member functions.

class RRObjectCollada : public RRObject
{
public:
	RRObjectCollada(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const RRCollider* collider, MaterialCacheCollada* materialCache);
	virtual ~RRObjectCollada();

private:
	const FCDSceneNode*        node;
	const FCDGeometryInstance* geometryInstance;

	// materials
	MaterialCacheCollada*      materialCache;
};

void getNodeMatrices(const FCDSceneNode* node, RRMatrix3x4* worldMatrix, RRMatrix3x4* invWorldMatrix)
{
	FMMatrix44 world = node->CalculateWorldTransform();
	if (worldMatrix)
	{
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				worldMatrix->m[i][j] = world.m[j][i];
	}
	if (invWorldMatrix)
	{
		const FMMatrix44 inv = world.Inverted();
		for (unsigned i=0;i<3;i++)
			for (unsigned j=0;j<4;j++)
				invWorldMatrix->m[i][j] = world.m[j][i];
	}
}

RRObjectCollada::RRObjectCollada(const FCDSceneNode* _node, const FCDGeometryInstance* _geometryInstance, const RRCollider* _collider, MaterialCacheCollada* _materialCache)
{
	RR_ASSERT(_node);
	RR_ASSERT(_collider);
	RR_ASSERT(_materialCache);
	node = _node;
	geometryInstance = _geometryInstance;
	setCollider(_collider);
	materialCache = _materialCache;
	name = node->GetName().c_str();

	// create transformation matrices
	RRMatrix3x4 worldMatrix;
	getNodeMatrices(node,&worldMatrix,NULL);
	setWorldMatrix(&worldMatrix);

	// init facegroups
	if (geometryInstance)
	{
		const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
		if (geometry)
		{
			const FCDGeometryMesh* mesh = geometry->GetMesh();
			if (mesh)
			{
				for (size_t i=0;i<mesh->GetPolygonsCount();i++)
				{
					const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
					if (polygons)
					{
						FCDGeometryPolygons::PrimitiveType primitiveType = polygons->GetPrimitiveType();
						if (primitiveType==FCDGeometryPolygons::POLYGONS || primitiveType==FCDGeometryPolygons::TRIANGLE_FANS || primitiveType==FCDGeometryPolygons::TRIANGLE_STRIPS)
						{
							const fstring symbol = polygons->GetMaterialSemantic();
							const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
							if (!materialInstance)
							{
								// workaround for buggy documents created by Right Hemisphere Collada Interface v146.46 with FCollada v1.13.
								// they state e.g. <instance_material symbol="BROWN_BRONZE3452816845" target="#BROWN_BRONZE"/>
								// while <instance_material symbol="BROWN_BRONZE" target="#BROWN_BRONZE"/> is expected
								for (size_t materialInstanceNum=0;materialInstanceNum<geometryInstance->GetMaterialInstanceCount();materialInstanceNum++)
								{
									materialInstance = geometryInstance->GetMaterialInstance(materialInstanceNum);
									if (materialInstance && materialInstance->GetSemantic().substr(0,symbol.size())==symbol)
									{
										// symbol we are looking for found as substring, use it
										break;
									}
									// symbol we are looking for not found, use any material instance that offers binding
								}
							}
							rr::RRMaterial* material = materialCache->getMaterial(materialInstance);
							faceGroups.push_back(FaceGroup(material,(unsigned)polygons->GetFaceCount()));
						}
					}
				}
			}
		}
	}
}

RRObjectCollada::~RRObjectCollada()
{
	// don't delete collider and mesh, we haven't created them
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsCollada

class RRObjectsCollada : public RRObjects
{
public:
	RRObjectsCollada(FCDocument* document, const char* pathToTextures, float emissiveMultiplier);
	virtual ~RRObjectsCollada();

private:
	const RRCollider*          newColliderCached(const FCDGeometryMesh* mesh);
	RRObjectCollada*           newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance);
	void                       addNode(const FCDSceneNode* node);

	// collider and mesh cache, for instancing
	typedef std::map<const FCDGeometryMesh*,const RRCollider*> ColliderCache;
	ColliderCache              colliderCache;
	MaterialCacheCollada       materialCache;
};

// Creates new RRCollider from FCDGeometryMesh.
// Caching on, first query creates collider, second query reads it from cache.
const RRCollider* RRObjectsCollada::newColliderCached(const FCDGeometryMesh* mesh)
{
	if (!mesh)
	{
		RR_ASSERT(0);
		return NULL;
	}
	for (size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if (polygons)
		{
			FCDGeometryPolygons::PrimitiveType primitiveType = polygons->GetPrimitiveType();
			if (primitiveType==FCDGeometryPolygons::POLYGONS || primitiveType==FCDGeometryPolygons::TRIANGLE_FANS || primitiveType==FCDGeometryPolygons::TRIANGLE_STRIPS)
			{
				if (polygons->GetFaceCount())
				{
					goto triangle_found;
				}
			}
		}
	}
	// mesh has 0 triangles (could consist of lines, points)
	return NULL;
triangle_found:

	ColliderCache::iterator i = colliderCache.find(mesh);
	if (i!=colliderCache.end())
	{
		return (*i).second;
	}
	else
	{
		bool aborting = false;
		return colliderCache[mesh] = RRCollider::create(new RRMeshCollada(mesh),RRCollider::IT_LINEAR,aborting);
	}
}

// Creates new RRObject from FCDEntityInstance.
// Always creates, no caching (only internal caching of colliders and meshes).
RRObjectCollada* RRObjectsCollada::newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance)
{
	if (!geometryInstance)
	{
		return NULL;
	}
	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if (!geometry)
	{
		return NULL;
	}
	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if (!mesh)
	{
		return NULL;
	}
	const RRCollider* collider = newColliderCached(mesh);
	if (!collider)
	{
		return NULL;
	}
	return new RRObjectCollada(node,geometryInstance,collider,&materialCache);
}

// Adds all instances from node and his subnodes to 'objects'.
void RRObjectsCollada::addNode(const FCDSceneNode* node)
{
	if (!node)
		return;
	// add instances from node
	for (size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if (entityInstance->GetEntityType()==FCDEntity::GEOMETRY)
		{
			const FCDGeometryInstance* geometryInstance = static_cast<const FCDGeometryInstance*>(entityInstance);
			RRObjectCollada* object = newObject(node,geometryInstance);
			if (object)
			{
#ifdef VERIFY
				object->getCollider()->getMesh()->checkConsistency(LIGHTMAP_CHANNEL,size());
#endif
				push_back(object);
			}
		}
	}
	// add children
	for (size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if (child)
		{
			addNode(child);
		}
	}
}

RRObjectsCollada::RRObjectsCollada(FCDocument* document, const char* pathToTextures, float emissiveMultiplier)
	: materialCache(pathToTextures,emissiveMultiplier)
{
	if (!document)
		return;

	// standardize up axis and units
	{
		RRReportInterval report(INF3,"Standardizing geometry...\n");
		FCDocumentTools::StandardizeUpAxisAndLength(document,FMVector3(0,1,0),1);
	}

	// triangulate all polygons
	{
		RRReportInterval report(INF3,"Triangulating geometry...\n");
		FCDGeometryLibrary* geometryLibrary = document->GetGeometryLibrary();
		for (size_t i=0;i<geometryLibrary->GetEntityCount();i++)
		{
			FCDGeometry* geometry = static_cast<FCDGeometry*>(geometryLibrary->GetEntity(i));
			FCDGeometryMesh* mesh = geometry->GetMesh();
			if (mesh)
			{	
				if (!mesh->IsTriangles())
					FCDGeometryPolygonsTools::Triangulate(mesh);
			}
		}
	}

	// generate unique indices
	// only important for realtime rendering
	// very slow (4 minutes in 800ktri scene)
	// without this code, single vertex number has unique pos, but it could have different normal/uv in different triangles
	{
		RRReportInterval report(INF2,"Generating unique indices...\n");
		FCDGeometryLibrary* geometryLibrary = document->GetGeometryLibrary();
		for (size_t i=0;i<geometryLibrary->GetEntityCount();i++)
		{
			FCDGeometry* geometry = static_cast<FCDGeometry*>(geometryLibrary->GetEntity(i));
			FCDGeometryMesh* mesh = geometry->GetMesh();
			if (mesh)
			{	
				FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh);
			}
		}
	}

	// adapt objects
	{
		RRReportInterval report(INF3,"Adapting objects...\n");
		const FCDSceneNode* root = document->GetVisualSceneInstance();
		if (!root) RRReporter::report(WARN,"RRObjectCollada: No visual scene instance found.\n");
		addNode(root);
	}
}

RRObjectsCollada::~RRObjectsCollada()
{
	// delete objects
	for (unsigned i=0;i<size();i++)
	{
		delete (*this)[i];
	}
	// delete meshes and colliders (stored in cache)
	for (ColliderCache::iterator i = colliderCache.begin(); i!=colliderCache.end(); i++)
	{
		const RRCollider* collider = (*i).second;
		delete collider->getMesh();
		delete collider;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// RRLightsCollada

class RRLightsCollada : public RRLights
{
public:
	RRLightsCollada(FCDocument* document);
	void addNode(const FCDSceneNode* node);
	virtual ~RRLightsCollada();
};

RRLightsCollada::RRLightsCollada(FCDocument* document)
{
	if (!document)
		return;

	// normalize geometry
	FCDocumentTools::StandardizeUpAxisAndLength(document,FMVector3(0,1,0),1);

	// import all lights
	addNode(document->GetVisualSceneInstance());
}

void RRLightsCollada::addNode(const FCDSceneNode* node)
{
	if (!node)
		return;
	// add instances from node
	for (size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if (entityInstance->GetEntityType()==FCDEntity::LIGHT)
		{
			const FCDLight* light = static_cast<const FCDLight*>(entityInstance->GetEntity());
			if (light)
			{
				// get position and direction
				RRMatrix3x4 invWorldMatrix;
				getNodeMatrices(node,NULL,&invWorldMatrix);
				RRVec3 position = invWorldMatrix.transformedPosition(RRVec3(0));
				RRVec3 direction = invWorldMatrix.transformedDirection(RRVec3(0,0,-1));

				// create RRLight
				RRVec3 color = RRVec3(light->GetColor()->x,light->GetColor()->y,light->GetColor()->z)*light->GetIntensity();
				RRVec4 polynom = RRVec4(light->GetConstantAttenuationFactor(),light->GetLinearAttenuationFactor(),light->GetQuadraticAttenuationFactor(),0.0001f);
				RRLight* rrlight = NULL;
				switch(light->GetLightType())
				{
					case FCDLight::POINT:
						rrlight = RRLight::createPointLightPoly(position,color,polynom);
						break;
					case FCDLight::SPOT:
						rrlight = RRLight::createSpotLightPoly(position,color,polynom,direction,light->GetOuterAngle(),light->GetFallOffAngle(),1);
						break;
					case FCDLight::DIRECTIONAL:
						rrlight = RRLight::createDirectionalLight(direction,color,false);
						break;
				}
				if (rrlight)
				{
					rrlight->name = light->GetName().c_str();
					push_back(rrlight);
				}
			}
		}
	}
	// add children
	for (size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if (child)
		{
			addNode(child);
		}
	}
}

RRLightsCollada::~RRLightsCollada()
{
	// delete lights
	for (unsigned i=0;i<size();i++)
		delete (*this)[i];
}


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneCollada

class RRSceneCollada : public RRScene
{
public:
	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		RRSceneCollada* scene = new RRSceneCollada;
		FCollada::Initialize();
		scene->scene_dae = FCollada::NewTopDocument();
		FUErrorSimpleHandler errorHandler;
		FCollada::LoadDocumentFromFile(scene->scene_dae,filename);
		if (!errorHandler.IsSuccessful())
		{
			FCDVersion version = scene->scene_dae->GetVersion();
			FCDVersion versionWanted("1.4.1");
			bool wrongVersion = version<versionWanted || version>versionWanted;
			if (wrongVersion)
				RRReporter::report(ERRO,"Collada %d.%d.%d is not fully supported, please use Collada 1.4.1. We recommend OpenCollada plugins for Max and Maya, http://opencollada.org. (%s)\n",version.major,version.minor,version.revision,filename);
			RRReporter::report(wrongVersion?WARN:ERRO,"%s\n",errorHandler.GetErrorString());
			delete scene;
			return NULL;
		}
		else
		{
			char* pathToTextures = _strdup(filename);
			char* tmp = RR_MAX(strrchr(pathToTextures,'\\'),strrchr(pathToTextures,'/'));
			if (tmp) tmp[1] = 0;
			RRReportInterval report(INF3,"Adapting scene...\n");
			scene->objects = adaptObjectsFromFCollada(scene->scene_dae,pathToTextures,emissiveMultiplier);
			scene->lights = adaptLightsFromFCollada(scene->scene_dae);
			free(pathToTextures);
			return scene;
		}
	}
	virtual ~RRSceneCollada()
	{
		delete scene_dae;
		FCollada::Release();
	}

private:
	FCDocument*                scene_dae;
};


//////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromFCollada(FCDocument* document, const char* pathToTextures, float emissiveMultiplier)
{
	return new RRObjectsCollada(document,pathToTextures,emissiveMultiplier);
}

RRLights* adaptLightsFromFCollada(class FCDocument* document)
{
	return new RRLightsCollada(document);
}

void registerLoaderCollada()
{
	RRScene::registerLoader("*.dae",RRSceneCollada::load);
}

#endif // SUPPORT_COLLADA
