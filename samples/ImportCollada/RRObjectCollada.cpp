// --------------------------------------------------------------------------
// Imports Collada scene (FCDocument) into RRDynamicSolver
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008
// --------------------------------------------------------------------------

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
// First TEXCOORD channel from document is assigned to diffuse maps.
// If second TEXCOORD channel is present, it is assigned to lightmaps,
// otherwise automatic unwrap for lightmaps is generated.
// Eventually if document contains no textures, first TEXCOORD is assigned to lightmaps.
// You can easily tweak this setup, see TEXCOORD,0 and TEXCOORD,lightmapUvChannel.
//
// Internal units are automatically converted to meters.
//
// 'Up' vector is automatically converted to 0,1,0 (Y positive).
//
// 'pointDetails' flag that hints solver to use slower per-pixel material path
// is enabled for materials with at least 1% transparency specified by texture.

#if 1 // 0 disables Collada support, 1 enables

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
#include "FUtils/FUFileManager.h"

#include "RRObjectCollada.h"

#ifdef _MSC_VER
	#if _MSC_VER<1400
		#error Third party library FCollada doesn't support VS2003.
	#else
		#pragma comment(lib,"FCollada.lib")
	#endif
#endif

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
// RRMeshCollada

// See RRMesh documentation for details
// on individual member functions.

class RRMeshCollada : public RRMesh
{
public:
	RRMeshCollada(const FCDGeometryMesh* _mesh, int _lightmapUvChannel);

	// RRChanneledData
	virtual void         getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool         getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRMesh
	virtual unsigned     getNumVertices() const;
	virtual void         getVertex(unsigned v, Vertex& out) const;
	virtual unsigned     getNumTriangles() const;
	virtual void         getTriangle(unsigned t, Triangle& out) const;
	virtual void         getTriangleNormals(unsigned t, TriangleNormals& out) const;
	virtual void         getTriangleMapping(unsigned t, TriangleMapping& out) const;

private:
	const FCDGeometryMesh* mesh;
	int lightmapUvChannel;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada load

// Doesn't create mesh copy, stores only pointer, so it depends on original collada mesh.
RRMeshCollada::RRMeshCollada(const FCDGeometryMesh* _mesh, int _lightmapUvChannel)
{
	mesh = _mesh;
	lightmapUvChannel = _lightmapUvChannel;

#ifdef VERIFY
	verify();
#endif
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRChanneledData

bool getTriangleVerticesData(const FCDGeometryMesh* mesh, FUDaeGeometryInput::Semantic semantic, unsigned index, unsigned floatsPerVertexExpected, unsigned itemIndex, void* itemData, unsigned itemSize)
{
	RR_ASSERT(itemSize==12*floatsPerVertexExpected);
	FCDGeometrySourceConstList sources;
	mesh->FindSourcesByType(semantic,sources);
	if(sources.size()<=index)
	{
		return false;
	}
	const FCDGeometrySource* source = sources[index];

	if(!source)
	{
		return false;
	}
	const float* data = source->GetData();
	size_t dataCount = source->GetDataCount();
	unsigned floatsPerVertexPresent = source->GetStride();
	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			const FCDGeometryPolygonsInput* polygonsInput = polygons->FindInput(source);
			if(polygonsInput)
			{
				LIMITED_TIMES(10,RR_ASSERT(polygons->TestPolyType()==3)); // this is expensive check, do it only few times
				size_t relativeIndex = itemIndex - polygons->GetFaceOffset();
				if(relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
				{
					const uint32* indices = polygonsInput->GetIndices();
					if(indices)
					{
						RR_ASSERT(relativeIndex*3+2<polygonsInput->GetIndexCount());
						float* out = (float*)itemData;
						for(unsigned j=0;j<3;j++)
						{
							for(unsigned k=0;k<floatsPerVertexExpected;k++)
							{
								unsigned dataIndex = indices[relativeIndex*3+j]*floatsPerVertexPresent+k;
								if(dataIndex>=dataCount)
								{
									LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Out of range indices in Collada file.\n"));
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
	RR_ASSERT(0);
	return false;
}

void RRMeshCollada::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
			if(numItems) *numItems = RRMeshCollada::getNumTriangles();
			if(itemSize) *itemSize = sizeof(RRVec2[3]);
			return;

		default:
			// unsupported channel
			RRMesh::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRMeshCollada::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		RR_ASSERT(0);
		return false;
	}
	switch(channelId)
	{
		case rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV:
			return getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,0,2,itemIndex,itemData,itemSize);

		default:
			// unsupported channel
			return RRMesh::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
// RRMeshCollada implements RRMesh

unsigned RRMeshCollada::getNumVertices() const
{
	const FCDGeometrySource* source = mesh->GetVertexSource(0);
	if(!source)
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
	if(!source)
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
	if(t>=RRMeshCollada::getNumTriangles()) 
	{
		RR_ASSERT(0);
		return;
	}
	const FCDGeometrySource* source = mesh->FindSourceByType(FUDaeGeometryInput::POSITION);
	if(!source)
	{
		RR_ASSERT(0);
		return;
	}
	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(!polygons)
		{
			RR_ASSERT(0);
			return;
		}
		if(t<polygons->GetFaceCount())
		{
			const FCDGeometryPolygonsInput* polygonsInput = polygons->FindInput(source);
			if(polygonsInput)
			{
				const uint32* indices = polygonsInput->GetIndices();
				if(indices)
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
	RR_ASSERT(0);
}

void RRMeshCollada::getTriangleNormals(unsigned t, TriangleNormals& out) const
{
	RRVec3 normal[3];
	RRVec3 tangent[3];
	RRVec3 bitangent[3];
	if(!getTriangleVerticesData(mesh,FUDaeGeometryInput::NORMAL,0,3,t,&normal,sizeof(normal)))
	{
		// all data generated
		RRMesh::getTriangleNormals(t,out);
	}
	else
	if(getTriangleVerticesData(mesh,FUDaeGeometryInput::GEOTANGENT,0,3,t,&tangent,sizeof(tangent)) && getTriangleVerticesData(mesh,FUDaeGeometryInput::GEOBINORMAL,0,3,t,&bitangent,sizeof(bitangent)))
	{
		// all data from collada
		for(unsigned v=0;v<3;v++)
		{
			out.vertex[v].normal = normal[v];
			out.vertex[v].tangent = tangent[v];
			out.vertex[v].bitangent = bitangent[v];
		}
	}
	else
	{
		// normals from collada, tangent+bitangent generated
		for(unsigned v=0;v<3;v++)
		{
			out.vertex[v].normal = normal[v];
			out.vertex[v].buildBasisFromNormal();
		}
	}
}

void RRMeshCollada::getTriangleMapping(unsigned t, TriangleMapping& out) const
{
	// if collada contains no unwrap,
	if(lightmapUvChannel<0 || !getTriangleVerticesData(mesh,FUDaeGeometryInput::TEXCOORD,lightmapUvChannel,2,t,&out,sizeof(out)))
	{
		// fallback to autogenerated one
		LIMITED_TIMES(1,RRReporter::report(WARN,"RRObjectCollada: No unwrap in %d th TEXCOORD, fallback to automatic.\n",lightmapUvChannel));
		RRMesh::getTriangleMapping(t,out);
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageCache

class ImageCache
{
public:
	RRBuffer* load(fstring filename, const char* cubeSideName[6], bool flipV, bool flipH)
	{
		Cache::iterator i = cache.find(filename);
		if(i!=cache.end())
		{
			return i->second;
		}
		else
		{
			return cache[filename] = RRBuffer::load(filename,cubeSideName,flipV,flipH);
		}
	}
	~ImageCache()
	{
		for(Cache::iterator i=cache.begin();i!=cache.end();i++)
		{
			delete i->second;
		}
	}
protected:
	typedef std::map<fstring,RRBuffer*> Cache;
	Cache cache;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectCollada

// See RRObject documentation for details
// on individual member functions.

class RRObjectCollada : public RRObject
{
public:
	RRObjectCollada(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, const RRCollider* acollider, const char* pathToTextures, bool stripPaths, ImageCache* imageCache);
	RRObjectIllumination*      getIllumination();
	virtual ~RRObjectCollada();

	// RRChanneledData
	virtual void               getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const;
	virtual bool               getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const;

	// RRObject
	virtual const RRCollider*  getCollider() const;
	virtual const RRMaterial*  getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const;
	virtual void               getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out) const;
	virtual const RRMatrix3x4* getWorldMatrix();
	virtual const RRMatrix3x4* getInvWorldMatrix();

private:
	const FCDSceneNode*        node;
	const FCDGeometryInstance* geometryInstance;

	// copy of object's material properties
	struct MaterialInfo
	{
		RRMaterial             material;
		rr::RRBuffer*          diffuseTexture;
		bool                   transparencyInDiffuseTextureAlpha;
	};
	typedef std::map<const FCDEffectStandard*,MaterialInfo> MaterialCache;
	MaterialCache              materialCache;
	ImageCache*                imageCache;
	void                       updateMaterials(const char* pathToTextures, bool stripPaths);
	const MaterialInfo*        getTriangleMaterialInfo(unsigned t) const;

	// collider for ray-mesh collisions
	const RRCollider*          collider;

	// copy of object's transformation matrices
	RRMatrix3x4                worldMatrix;
	RRMatrix3x4                invWorldMatrix;

	// indirect illumination (ambient maps etc)
	RRObjectIllumination*      illumination;
};

void getNodeMatrices(const FCDSceneNode* node, rr::RRMatrix3x4& worldMatrix, rr::RRMatrix3x4& invWorldMatrix)
{
	FMMatrix44 world = node->CalculateWorldTransform();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			worldMatrix.m[i][j] = world.m[j][i];
	const FMMatrix44 inv = world.Inverted();
	for(unsigned i=0;i<3;i++)
		for(unsigned j=0;j<4;j++)
			invWorldMatrix.m[i][j] = world.m[j][i];
}

RRObjectCollada::RRObjectCollada(const FCDSceneNode* _node, const FCDGeometryInstance* _geometryInstance, const RRCollider* _collider, const char* _pathToTextures, bool _stripPaths, ImageCache* _imageCache)
{
	RR_ASSERT(_node);
	RR_ASSERT(_collider);
	RR_ASSERT(_imageCache);
	node = _node;
	geometryInstance = _geometryInstance;
	collider = _collider;
	imageCache = _imageCache;

	// create illumination
	illumination = new rr::RRObjectIllumination(collider->getMesh()->getNumVertices());

	// create transformation matrices
	getNodeMatrices(node,worldMatrix,invWorldMatrix);

	// create material cache
	updateMaterials(_pathToTextures,_stripPaths);
}

void RRObjectCollada::getChannelSize(unsigned channelId, unsigned* numItems, unsigned* itemSize) const
{
	switch(channelId)
	{
		case rr::RRMesh::CHANNEL_TRIANGLE_DIFFUSE_TEX:
			if(numItems) *numItems = getCollider()->getMesh()->getNumTriangles();
			if(itemSize) *itemSize = sizeof(rr::RRBuffer*);
			return;

		default:
			// unsupported channel
			RRObject::getChannelSize(channelId,numItems,itemSize);
	}
}

bool RRObjectCollada::getChannelData(unsigned channelId, unsigned itemIndex, void* itemData, unsigned itemSize) const
{
	if(!itemData)
	{
		RR_ASSERT(0);
		return false;
	}
	switch(channelId)
	{
		case rr::RRMesh::CHANNEL_TRIANGLE_DIFFUSE_TEX:
			{
			if(itemIndex>=getCollider()->getMesh()->getNumTriangles())
			{
				RR_ASSERT(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef rr::RRBuffer* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				RR_ASSERT(0);
				return false;
			}
			const MaterialInfo* mi = getTriangleMaterialInfo(itemIndex);
			*out = mi ? mi->diffuseTexture : NULL;
			return true;
			}

		case rr::RRMesh::CHANNEL_TRIANGLE_OBJECT_ILLUMINATION:
			{
			if(itemIndex>=getCollider()->getMesh()->getNumTriangles())
			{
				RR_ASSERT(0); // legal, but shouldn't happen in well coded program
				return false;
			}
			typedef RRObjectIllumination* Out;
			Out* out = (Out*)itemData;
			if(sizeof(*out)!=itemSize)
			{
				RR_ASSERT(0);
				return false;
			}
			*out = illumination;
			return true;
			}

		default:
			// unsupported channel
			return RRObject::getChannelData(channelId,itemIndex,itemData,itemSize);
	}
}

const RRCollider* RRObjectCollada::getCollider() const
{
	return collider;
}

fstring getTriangleMaterialSymbol(const FCDGeometryMesh* mesh, unsigned triangle)
{
	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			size_t relativeIndex = triangle - polygons->GetFaceOffset();
			if(relativeIndex>=0 && relativeIndex<polygons->GetFaceCount())
			{
				return polygons->GetMaterialSemantic();
			}
		}
	}
	RR_ASSERT(0);
	return NULL;
}

RRVec3 colorToColor(FMVector4 color)
{
	return RRVec3(color.x,color.y,color.z);
}

RRReal colorToFloat(FMVector4 color)
{
	return (color.x+color.y+color.z)*0.333f;
}

void RRObjectCollada::updateMaterials(const char* pathToTextures, bool stripPaths)
{
	if(!geometryInstance)
	{
		return;
	}

	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return;
	}

	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return;
	}

	for(size_t i=0;i<mesh->GetPolygonsCount();i++)
	{
		const FCDGeometryPolygons* polygons = mesh->GetPolygons(i);
		if(polygons)
		{
			const fstring symbol = polygons->GetMaterialSemantic();

			const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
			if(!materialInstance)
			{
				continue;
			}

			const FCDMaterial* material = materialInstance->GetMaterial();
			if(!material)
			{
				continue;
			}

			const FCDEffect* effect = material->GetEffect();
			if(!effect)
			{
				continue;
			}

			const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
			if(!effectProfile)
			{
				continue;
			}
			const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);

			// Write RRMaterials into cache.
			// Why?
			// 1) RRObject requires that we return always the same RRMaterial for the same triangle
			//    and that pointer stays valid for whole RRObject life.
			// 2) getTriangleMaterial() is const and thread safe,
			//    so cache can't be filled lazily, it must be filled in constructor.
			MaterialCache::const_iterator i = materialCache.find(effectStandard);
			if(i==materialCache.end())
			{
				MaterialInfo mi;
				mi.material.reset(false); // 1-sided with photons hitting back side deleted
				mi.material.diffuseReflectance = colorToColor(effectStandard->GetDiffuseColor());
				mi.material.diffuseEmittance = colorToColor(effectStandard->GetEmissionFactor() * effectStandard->GetEmissionColor());
				mi.material.specularReflectance = colorToFloat(effectStandard->GetSpecularFactor() * effectStandard->GetSpecularColor());
				mi.material.specularTransmittance = (effectStandard->GetTransparencyMode()==FCDEffectStandard::A_ONE)
					? RRVec3( 1 - effectStandard->GetTranslucencyFactor() * effectStandard->GetTranslucencyColor().w )
					: colorToColor( effectStandard->GetTranslucencyFactor() * effectStandard->GetTranslucencyColor() );
				mi.material.refractionIndex = effectStandard->GetIndexOfRefraction();
				/* disable light paths not supported by lighting model (but is it what user expects?)
				switch(effectStandard->GetLightingType())
				{
					case FCDEffectStandard::CONSTANT:
						mi.material.diffuseReflectance = rr::RRVec3(0);
						mi.material.specularReflectance = 0;
						break;
					case FCDEffectStandard::LAMBERT:
						mi.material.specularReflectance = rr::RRVec3(0);
						break;
					case FCDEffectStandard::PHONG:
						break;
					case FCDEffectStandard::BLINN:
						break;
				}*/
				const FCDTexture* diffuseTexture = effectStandard->GetTextureCount(FUDaeTextureChannel::DIFFUSE) ? effectStandard->GetTexture(FUDaeTextureChannel::DIFFUSE,0) : NULL;
				const FCDTexture* transparentTexture = effectStandard->GetTextureCount(FUDaeTextureChannel::TRANSPARENT) ? effectStandard->GetTexture(FUDaeTextureChannel::TRANSPARENT,0) : NULL;
				mi.diffuseTexture = NULL;
				mi.transparencyInDiffuseTextureAlpha = false;
				if(diffuseTexture)
				{
					const FCDImage* diffuseImage = diffuseTexture->GetImage();
					if(diffuseImage)
					{
						fstring strippedName = diffuseImage->GetFilename();
						if(stripPaths)
						{
							while(strippedName.contains('/') || strippedName.contains('\\')) strippedName.pop_front();
						}
						strippedName.insert(0,pathToTextures);
						mi.diffuseTexture = imageCache->load(strippedName.c_str(),NULL,false,false);
						if(mi.diffuseTexture)
						{
							// compute average diffuse texture color
							enum {size = 8};
							rr::RRVec4 avg = rr::RRVec4(0);
							for(unsigned i=0;i<size;i++)
								for(unsigned j=0;j<size;j++)
								{
									avg += mi.diffuseTexture->getElement(rr::RRVec3(i/(float)size,j/(float)size,0));
								}
							avg /= size*size;
							// rgb is diffuse reflectance
							mi.material.diffuseReflectance = avg;
							// alpha could be transparency
							if(transparentTexture && transparentTexture->GetImage()==diffuseTexture->GetImage() && effectStandard->GetTransparencyMode()==FCDEffectStandard::A_ONE)
							{
								mi.transparencyInDiffuseTextureAlpha = true;
								mi.material.specularTransmittance = rr::RRVec3(1-avg[3]);
								if(effectStandard->GetTranslucencyFactor()!=1)
									LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Translucency factor combined with texture ignored by Collada adapter.\n"));
								// Enables per-pixel materials(diffuse reflectance and transparency) for solver.
								// It makes calculation much slower, so enable it only when necessary.
								// It usually pays off for textures with strongly varying per-pixel alpha (e.g. trees).
								// Here, for simplicity, we enable it for textures with at least 1% transparency in texture.
								if(mi.material.specularTransmittance.avg()>0.01f)
									mi.material.sideBits[0].pointDetails = 1; // our material is 1sided, so set it for side 0 (front)
							}
						}
					}
				}
				if(transparentTexture && !mi.transparencyInDiffuseTextureAlpha)
				{
					if(transparentTexture->GetImage()==diffuseTexture->GetImage())
						LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"RGB tranparency texture ignored by Collada adapter.\n"))
					else
						LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Tranparency texture other than diffuse texture ignored by Collada adapter.\n"));
				}
				mi.material.name = _strdup(effect->GetName().c_str());
#ifdef VERIFY
				if(mi.material.validate())
					RRReporter::report(WARN,"Material adjusted to physically valid.\n");
#else
				mi.material.validate();
#endif
				materialCache[effectStandard] = mi;
			}
		}
	}
}

const RRObjectCollada::MaterialInfo* RRObjectCollada::getTriangleMaterialInfo(unsigned t) const
{
	// lets have some abstract layered fun

	if(!geometryInstance)
	{
		return NULL;
	}

	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return NULL;
	}

	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return NULL;
	}

	const fstring symbol = getTriangleMaterialSymbol(mesh,t);

	const FCDMaterialInstance* materialInstance = geometryInstance->FindMaterialInstance(symbol);
	if(!materialInstance)
	{
		return NULL;
	}

	const FCDMaterial* material = materialInstance->GetMaterial();
	if(!material)
	{
		return NULL;
	}

	const FCDEffect* effect = material->GetEffect();
	if(!effect)
	{
		return NULL;
	}

	const FCDEffectProfile* effectProfile = effect->FindProfile(FUDaeProfileType::COMMON);
	if(!effectProfile)
	{
		return NULL;
	}
	const FCDEffectStandard* effectStandard = static_cast<const FCDEffectStandard*>(effectProfile);

	MaterialCache::const_iterator i = materialCache.find(effectStandard);
	if(i==materialCache.end())
	{
		RR_ASSERT(0);
		return NULL;
	}
	return &i->second;
}

const RRMaterial* RRObjectCollada::getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
{
	const MaterialInfo* materialInfo = getTriangleMaterialInfo(t);
	return materialInfo ? &materialInfo->material : NULL;
}

void RRObjectCollada::getPointMaterial(unsigned t,RRVec2 uv,RRMaterial& out) const
{
	// When point materials are used, this is critical place for performance
	// of updateLightmap[s]().
	// getChannelData() called here is very slow.
	// In your implementations, prefer simple lookups, avoid for cycles.
	// Use profiler to see what percentage of time is spent in getPointMaterial().
	const MaterialInfo* materialInfo = getTriangleMaterialInfo(t);
	const RRMaterial* material = materialInfo ? &materialInfo->material : NULL;
	if(material)
	{
		out = *material;
	}
	else
	{
		out.reset(false);
	}
	// getting coords for diffuse texture
	rr::RRVec2 mapping[3];
	getCollider()->getMesh()->getChannelData(rr::RRMesh::CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV,t,mapping,sizeof(mapping));
	uv = mapping[0]*(1-uv[0]-uv[1]) + mapping[1]*uv[0] + mapping[2]*uv[1];
	// getting pixel from texture
	RR_ASSERT(materialInfo->diffuseTexture);
	RRVec4 rgba = materialInfo->diffuseTexture->getElement(RRVec3(uv[0],uv[1],0));
	// diffuse color
	out.diffuseReflectance = rgba;
	// alpha/transparency
	if(materialInfo->transparencyInDiffuseTextureAlpha)
	{
		out.specularTransmittance = rr::RRVec3(1-rgba[3]);
		if(rgba[3]==0)
			out.sideBits[0].catchFrom = out.sideBits[1].catchFrom = 0;
	}
}

const RRMatrix3x4* RRObjectCollada::getWorldMatrix()
{
	return &worldMatrix;
}

const RRMatrix3x4* RRObjectCollada::getInvWorldMatrix()
{
	return &invWorldMatrix;
}

RRObjectIllumination* RRObjectCollada::getIllumination()
{
	return illumination;
}

RRObjectCollada::~RRObjectCollada()
{
	for(MaterialCache::iterator i=materialCache.begin();i!=materialCache.end();i++)
	{
		// we created it in updateMaterials() and stored in const char* so no one can edit it
		// now it's time to free it
		free((char*)(i->second.material.name));

		// don't delete textures loaded via imageCache
		//SAFE_DELETE(i->second.diffuseTexture);
	}
	delete illumination;
	// don't delete collider and mesh, we haven't created them
}


//////////////////////////////////////////////////////////////////////////////
//
// ObjectsFromFCollada

class ObjectsFromFCollada : public rr::RRObjects
{
public:
	ObjectsFromFCollada(FCDocument* document, const char* pathToTextures, bool stripPaths);
	virtual ~ObjectsFromFCollada();

private:
	const RRCollider*          newColliderCached(const FCDGeometryMesh* mesh, int lightmapUvChannel);
	RRObjectCollada*           newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, int lightmapUvChannel, const char* pathToTextures, bool stripPaths, ImageCache* imageCache);
	void                       addNode(const FCDSceneNode* node, int lightmapUvChannel, const char* pathToTextures, bool stripPaths, ImageCache* imageCache);

	// collider and mesh cache, for instancing
	typedef std::map<const FCDGeometryMesh*,const RRCollider*> ColliderCache;
	ColliderCache              colliderCache;
	ImageCache                 imageCache;
};

// Creates new RRCollider from FCDGeometryMesh.
// Caching on, first query creates collider, second query reads it from cache.
const RRCollider* ObjectsFromFCollada::newColliderCached(const FCDGeometryMesh* mesh, int lightmapUvChannel)
{
	if(!mesh)
	{
		RR_ASSERT(0);
		return NULL;
	}
	ColliderCache::iterator i = colliderCache.find(mesh);
	if(i!=colliderCache.end())
	{
		return (*i).second;
	}
	else
	{
		return colliderCache[mesh] = RRCollider::create(new RRMeshCollada(mesh, lightmapUvChannel),RRCollider::IT_LINEAR);
	}
}

// Creates new RRObject from FCDEntityInstance.
// Always creates, no caching (only internal caching of colliders and meshes).
RRObjectCollada* ObjectsFromFCollada::newObject(const FCDSceneNode* node, const FCDGeometryInstance* geometryInstance, int lightmapUvChannel, const char* pathToTextures, bool stripPaths, ImageCache* imageCache)
{
	if(!geometryInstance)
	{
		return NULL;
	}
	const FCDGeometry* geometry = static_cast<const FCDGeometry*>(geometryInstance->GetEntity());
	if(!geometry)
	{
		return NULL;
	}
	const FCDGeometryMesh* mesh = geometry->GetMesh();
	if(!mesh)
	{
		return NULL;
	}
	const RRCollider* collider = newColliderCached(mesh, lightmapUvChannel);
	if(!collider)
	{
		return NULL;
	}
	return new RRObjectCollada(node,geometryInstance,collider,pathToTextures,stripPaths,imageCache);
}

// Adds all instances from node and his subnodes to 'objects'.
void ObjectsFromFCollada::addNode(const FCDSceneNode* node, int lightmapUvChannel, const char* pathToTextures, bool stripPaths, ImageCache* imageCache)
{
	if(!node)
		return;
	// add instances from node
	for(size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if(entityInstance->GetEntityType()==FCDEntity::GEOMETRY)
		{
			const FCDGeometryInstance* geometryInstance = static_cast<const FCDGeometryInstance*>(entityInstance);
			RRObjectCollada* object = newObject(node,geometryInstance,lightmapUvChannel,pathToTextures,stripPaths,imageCache);
			if(object)
			{
				push_back(RRIlluminatedObject(object,object->getIllumination()));
			}
		}
	}
	// add children
	for(size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if(child)
		{
			addNode(child,lightmapUvChannel,pathToTextures,stripPaths,imageCache);
		}
	}
}

ObjectsFromFCollada::ObjectsFromFCollada(FCDocument* document, const char* pathToTextures, bool stripPaths)
{
	if(!document)
		return;

	// normalize geometry
	FCDocumentTools::StandardizeUpAxisAndLength(document,FMVector3(0,1,0),1);

	// guess where is mapping for lightmaps
	//  has textures -> lmaps in second uv channel, doesn't have textures -> lmaps in first uv channel
	FCDImageLibrary* imageLibrary = document->GetImageLibrary();
	int lightmapUvChannel = (imageLibrary && !imageLibrary->IsEmpty()) ? 1 : 0;

	// triangulate all polygons
	FCDGeometryLibrary* geometryLibrary = document->GetGeometryLibrary();
	for(size_t i=0;i<geometryLibrary->GetEntityCount();i++)
	{
		FCDGeometry* geometry = static_cast<FCDGeometry*>(geometryLibrary->GetEntity(i));
		FCDGeometryMesh* mesh = geometry->GetMesh();
		if(mesh)
		{
			FCDGeometryPolygonsTools::Triangulate(mesh);
			FCDGeometryPolygonsTools::GenerateUniqueIndices(mesh);
		}
	}

	// import data
	const FCDSceneNode* root = document->GetVisualSceneInstance();
	if(!root) RRReporter::report(WARN,"RRObjectCollada: No visual scene instance found.\n");
	addNode(root,lightmapUvChannel,pathToTextures,stripPaths,&imageCache);
}

ObjectsFromFCollada::~ObjectsFromFCollada()
{
	// delete objects
	for(unsigned i=0;i<size();i++)
	{
		// no need to delete illumination separately,
		//  object created it, object deletes it
		//delete (*this)[i].illumination;
		delete (*this)[i].object;
	}
	// delete meshes and colliders (stored in cache)
	for(ColliderCache::iterator i = colliderCache.begin(); i!=colliderCache.end(); i++)
	{
		const RRCollider* collider = (*i).second;
		delete collider->getMesh();
		delete collider;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// LightsFromFCollada

class LightsFromFCollada : public rr::RRLights
{
public:
	LightsFromFCollada(FCDocument* document);
	void addNode(const FCDSceneNode* node);
	virtual ~LightsFromFCollada();
};

LightsFromFCollada::LightsFromFCollada(FCDocument* document)
{
	if(!document)
		return;

	// normalize geometry
	FCDocumentTools::StandardizeUpAxisAndLength(document,FMVector3(0,1,0),1);

	// import all lights
	addNode(document->GetVisualSceneInstance());
}

void LightsFromFCollada::addNode(const FCDSceneNode* node)
{
	if(!node)
		return;
	// add instances from node
	for(size_t i=0;i<node->GetInstanceCount();i++)
	{
		const FCDEntityInstance* entityInstance = node->GetInstance(i);
		if(entityInstance->GetEntityType()==FCDEntity::LIGHT)
		{
			const FCDLight* light = static_cast<const FCDLight*>(entityInstance->GetEntity());
			if(light)
			{
				// get position and direction
				rr::RRMatrix3x4 worldMatrix;
				rr::RRMatrix3x4 invWorldMatrix;
				getNodeMatrices(node,worldMatrix,invWorldMatrix);
				rr::RRVec3 position = invWorldMatrix.transformedPosition(rr::RRVec3(0));
				rr::RRVec3 direction = invWorldMatrix.transformedDirection(rr::RRVec3(0,0,-1));

				// create RRLight
				rr::RRVec3 color = RRVec3(light->GetColor()->x,light->GetColor()->y,light->GetColor()->z)*light->GetIntensity();
				rr::RRVec3 polynom = rr::RRVec3(light->GetConstantAttenuationFactor(),light->GetLinearAttenuationFactor(),light->GetQuadraticAttenuationFactor());
				switch(light->GetLightType())
				{
				case FCDLight::POINT:
					push_back(rr::RRLight::createPointLightPoly(position,color,polynom));
					break;
				case FCDLight::SPOT:
					push_back(rr::RRLight::createSpotLightPoly(position,color,polynom,direction,light->GetOuterAngle(),light->GetFallOffAngle(),1));
					break;
				case FCDLight::DIRECTIONAL:
					push_back(rr::RRLight::createDirectionalLight(direction,color,false));
					break;
				}
			}
		}
	}
	// add children
	for(size_t i=0;i<node->GetChildrenCount();i++)
	{
		const FCDSceneNode* child = node->GetChild(i);
		if(child)
		{
			addNode(child);
		}
	}
}

LightsFromFCollada::~LightsFromFCollada()
{
	// delete lights
	for(unsigned i=0;i<size();i++)
		delete (*this)[i];
}


//////////////////////////////////////////////////////////////////////////////
//
// main

rr::RRObjects* adaptObjectsFromFCollada(FCDocument* document, const char* pathToTextures, bool stripPaths)
{
	return new ObjectsFromFCollada(document,pathToTextures,stripPaths);
}

rr::RRLights* adaptLightsFromFCollada(class FCDocument* document)
{
	return new LightsFromFCollada(document);
}

#else

// stub - for quickly disabled collada support
#include "RRObjectCollada.h"
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document)
{
	return NULL;
}
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document)
{
	return NULL;
}

#endif
