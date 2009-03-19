// --------------------------------------------------------------------------
// Lightsprint adapters for Gamebryo scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_GAMEBRYO

// TODO
// - test LODs (implemented but not tested)
// - support FLOAT16_n vertex attributes

// This constant is used to scale geometry for Lightsprint.
// Lightsprint recommended unit is meter.
// Gamebryo unit depends on you. For meters, use 1, for centimeters 0.01f.
#define SCALE_GEOMETRY 0.1f

// Define to respect Gamebryo settings that disable lighting or shadowing.
// Comment out to make everything lit and shadows-casting.
#define SUPPORT_DISABLED_LIGHTING_SHADOWING

#include <NiMain.h>
#include <NiMesh.h>
#include <NiDataStreamPrimitiveLock.h>
#include "NiMaterialDetector.h"
#include "RRObjectGamebryo.h"

#pragma comment(lib,"NiSystem")
#pragma comment(lib,"NiFloodgate")
#pragma comment(lib,"NiMain")
#pragma comment(lib,"NiMesh")
#pragma comment(lib,"NiAnimation")

// lightmap utility library, part of Gamebryo GI package 
#include <NiLightMapUtility.h>
#pragma comment(lib,"NiLightMapUtility")
#pragma comment(lib,"NiLightMapMaterial")

// .gsa load
#include <NiEntity.h>
#pragma comment(lib,"NiEntity")
#pragma comment(lib,"TinyXML")
// necessary for processing certain .gsa nodes that would be otherwise ignored
#include <NiCollision.h>
#pragma comment(lib,"NiCollision")
#include <NiPortal.h>
#pragma comment(lib,"NiPortal")
#include <NiParticle.h>
#pragma comment(lib,"NiParticle")

// cache
#include <map>
#include <list>
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
#include <vector>
#endif

#include <NiLicense.h>

NiEmbedGamebryoLicenseCode;

using namespace rr;


////////////////////////////////////////////////////////////////////////////
//
// utility functions

inline RRVec2 convertUv(const NiPoint2& v)
{
	return RRVec2(v.x, 1-v.y);
}

inline RRVec3 convertLocalPos(const NiPoint3& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertLocalDir(const NiPoint3& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertWorldPos(const NiPoint3& v)
{
	return RRVec3(v.x, v.z, -v.y);
}

inline RRVec3 convertWorldDir(const NiPoint3& v)
{
	return RRVec3(v.x, v.z, -v.y);
}

inline RRVec3 convertColor_(const NiPoint3& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertColor(const NiColor& c)
{
	return RRVec3(c.r, c.g, c.b);
}

RRMatrix3x4 convertMatrix(const NiTransform& transform)
{
	RRMatrix3x4 wm;
	NiPoint3 row;
	NiPoint3 trans = transform.m_Translate;
	NiMatrix3 rot = transform.m_Rotate;
	float scale1 = SCALE_GEOMETRY;
	float scale2 = SCALE_GEOMETRY * transform.m_fScale;

	rot.GetCol(0, row);
	wm.m[0][0] = scale2*row.x;
	wm.m[1][0] = scale2*row.z;
	wm.m[2][0] = -scale2*row.y;

	rot.GetCol(1, row);
	wm.m[0][1] = scale2*row.x;
	wm.m[1][1] = scale2*row.z;
	wm.m[2][1] = -scale2*row.y;

	rot.GetCol(2, row);
	wm.m[0][2] = scale2*row.x;
	wm.m[1][2] = scale2*row.z;
	wm.m[2][2] = -scale2*row.y;

	wm.m[0][3] = scale1*trans.x;
	wm.m[1][3] = scale1*trans.z;
	wm.m[2][3] = -scale1*trans.y;

	return wm;
}

RRBuffer* convertTexture(NiPixelData* pixelData)
{
	if (!pixelData)
	{
		return NULL;
	}
	NIASSERT(pixelData->GetPixelFormat().GetFormat()==NiPixelFormat::FORMAT_RGBA);
	return RRBuffer::create(BT_2D_TEXTURE,pixelData->GetWidth(),pixelData->GetHeight(),1,BF_RGBA,true,pixelData->GetPixels());
}

//! Converts _add and then subtracts sub from result.
RRBuffer* convertTextureAndSubtract(NiPixelData* _add, RRBuffer* _sub)
{
	RRBuffer* add = convertTexture(_add);
	if (!add)
	{
		return NULL;
	}
	if (!_sub)
	{
		return add;
	}
	unsigned w = add->getWidth();
	unsigned h = add->getHeight();
	if (_sub->getWidth()!=w || _sub->getHeight()!=h)
	{
		NIASSERT(0);
	}
	else
	{
		for (unsigned i=0;i<w*h;i++)
		{
			RRVec4 a = add->getElement(i);
			RRVec4 s = _sub->getElement(i);
			add->setElement(i,RRVec4(RRVec3(a-s),a[3]));
		}
	}
	return add;
}

//! Texcoord channel numbers.
enum Channel
{
	CH_LIGHTMAP,
	CH_DIFFUSE,
	CH_SPECULAR,
	CH_EMISSIVE,
	CH_COUNT
};


////////////////////////////////////////////////////////////////////////////
//
// RRMeshGamebryo

class RRMeshGamebryo : public RRMesh
{
public:
	RRMeshGamebryo(NiMesh* _mesh)
	{
		mesh = _mesh;
		numVertices = mesh->GetVertexCount();
		numTriangles = mesh->GetTotalPrimitiveCount();

		// iterators for all getXxx() functions are created here
		// TODO support also F_FLOAT16_4
		//  You can identify the format of a data stream by looking at the NiDataStreamElement objects associated with a given NiDataStream object.
		numSubmeshes = _mesh->GetSubmeshCount();
		submeshData = new SubmeshData[numSubmeshes];
		NiDataStreamPrimitiveLock kLockIndex(mesh, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockPosition(mesh, NiCommonSemantics::POSITION(), 0, NiDataStreamElement::F_FLOAT32_3, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockNormal(mesh, NiCommonSemantics::NORMAL(), 0, NiDataStreamElement::F_FLOAT32_3, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockTangent(mesh, NiCommonSemantics::TANGENT(), 0, NiDataStreamElement::F_FLOAT32_3, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockBitangent(mesh, NiCommonSemantics::BINORMAL(), 0, NiDataStreamElement::F_FLOAT32_3, NiDataStream::LOCK_READ);
		unsigned lightmapTexcoord = (unsigned)NiLightMapUtility::GetLightMapUVSetIndex(_mesh);
		unsigned diffuseTexcoord = UINT_MAX;
		unsigned specularTexcoord = UINT_MAX;
		unsigned emissiveTexcoord = UINT_MAX;
		{
			NiTexturingProperty* pkTexProp = (NiTexturingProperty*)mesh->GetProperty(NiProperty::TEXTURING);
			if (pkTexProp)
			{
				NiTexturingProperty::Map* pkBaseMap = pkTexProp->GetBaseMap();
				if (pkBaseMap)
				{
					diffuseTexcoord = pkBaseMap->GetTextureIndex();
				}
				NiTexturingProperty::Map* pkSpecularMap = pkTexProp->GetGlossMap();
				if (pkSpecularMap)
				{
					specularTexcoord = pkSpecularMap->GetTextureIndex();
				}
				NiTexturingProperty::Map* pkGlowMap = pkTexProp->GetGlowMap();
				if (pkGlowMap)
				{
					emissiveTexcoord = pkGlowMap->GetTextureIndex();
				}
			}
		}
		NiDataStreamElementLock kLockLightmapUv(mesh, NiCommonSemantics::TEXCOORD(), lightmapTexcoord, NiDataStreamElement::F_FLOAT32_2, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockDiffuseUv(mesh, NiCommonSemantics::TEXCOORD(), diffuseTexcoord, NiDataStreamElement::F_FLOAT32_2, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockSpecularUv(mesh, NiCommonSemantics::TEXCOORD(), specularTexcoord, NiDataStreamElement::F_FLOAT32_2, NiDataStream::LOCK_READ);
		NiDataStreamElementLock kLockEmissiveUv(mesh, NiCommonSemantics::TEXCOORD(), emissiveTexcoord, NiDataStreamElement::F_FLOAT32_2, NiDataStream::LOCK_READ);
		hasPosition = kLockPosition.IsLocked();
		hasNormal = kLockNormal.IsLocked();
		hasTangents = kLockTangent.IsLocked() && kLockBitangent.IsLocked();
		hasTexcoord[CH_LIGHTMAP] = kLockLightmapUv.IsLocked();
		hasTexcoord[CH_DIFFUSE] = kLockDiffuseUv.IsLocked();
		hasTexcoord[CH_SPECULAR] = kLockSpecularUv.IsLocked();
		hasTexcoord[CH_EMISSIVE] = kLockEmissiveUv.IsLocked();
		if (hasPosition)
		{
			for (unsigned submesh=0; submesh<numSubmeshes; submesh++)
			{
				// indices
				submeshData[submesh].numTriangles = kLockIndex.count(submesh);
				if (kLockIndex.Has16BitIndexBuffer())
				{
					submeshData[submesh].has16BitIndexBuffer = true;
					submeshData[submesh].has32BitIndexBuffer = false;
					submeshData[submesh].iterIndex16 = kLockIndex.BeginIndexed16(submesh);
					NiUInt32 uiNumIndicesPerPrimitive = (*submeshData[submesh].iterIndex16).count();
					NIASSERT(uiNumIndicesPerPrimitive==3);
				}
				else
				if (kLockIndex.Has32BitIndexBuffer())
				{
					submeshData[submesh].has16BitIndexBuffer = false;
					submeshData[submesh].has32BitIndexBuffer = true;
					submeshData[submesh].iterIndex32 = kLockIndex.BeginIndexed32(submesh);
					NiUInt32 uiNumIndicesPerPrimitive = (*submeshData[submesh].iterIndex32).count();
					NIASSERT(uiNumIndicesPerPrimitive==3);
				}
				else
				{
					submeshData[submesh].has16BitIndexBuffer = false;
					submeshData[submesh].has32BitIndexBuffer = false;
				}

				// vertices
				submeshData[submesh].numVertices = kLockPosition.count(submesh);
				submeshData[submesh].iterPosition = kLockPosition.begin<NiPoint3>(submesh);
				if (hasNormal)
				{
					submeshData[submesh].iterNormal = kLockNormal.begin<NiPoint3>(submesh);
				}
				if (hasTangents)
				{
					submeshData[submesh].iterTangent = kLockTangent.begin<NiPoint3>(submesh);
					submeshData[submesh].iterBitangent = kLockBitangent.begin<NiPoint3>(submesh);
				}
				if (hasTexcoord[CH_LIGHTMAP])
				{
					submeshData[submesh].iterTexcoord[CH_LIGHTMAP] = kLockLightmapUv.begin<NiPoint2>(submesh);
				}
				if (hasTexcoord[CH_DIFFUSE])
				{
					submeshData[submesh].iterTexcoord[CH_DIFFUSE] = kLockDiffuseUv.begin<NiPoint2>(submesh);
				}
				if (hasTexcoord[CH_SPECULAR])
				{
					submeshData[submesh].iterTexcoord[CH_SPECULAR] = kLockSpecularUv.begin<NiPoint2>(submesh);
				}
				if (hasTexcoord[CH_EMISSIVE])
				{
					submeshData[submesh].iterTexcoord[CH_EMISSIVE] = kLockEmissiveUv.begin<NiPoint2>(submesh);
				}
			}
		}
		else
		{
			numTriangles = 0;
			numVertices = 0;
		}
	}

	virtual ~RRMeshGamebryo()
	{
		delete[] submeshData;
	}

	virtual unsigned getNumVertices() const
	{
		return numVertices;
	}
	virtual void getVertex(unsigned v, Vertex& out) const
	{
		NIASSERT(v<numVertices);
		for (unsigned submesh=0; submesh<numSubmeshes; submesh++)
		{
			if (v<submeshData[submesh].numVertices)
			{
				out = convertLocalPos(submeshData[submesh].iterPosition[v]);
				return;
			}
			v -= submeshData[submesh].numVertices;
		}
		// v out of range ?
		NIASSERT(0);
	}

	virtual unsigned getNumTriangles() const
	{
		return numTriangles;
	}
	virtual void getTriangle(unsigned t, Triangle& out) const
	{
		NIASSERT(t<numTriangles);
		unsigned numVerticesInSkippedSubmeshes = 0;
		for (unsigned submesh=0; submesh<numSubmeshes; submesh++)
		{
			if (t<submeshData[submesh].numTriangles)
			{
				if (submeshData[submesh].has16BitIndexBuffer)
				{
					// indexed 16bit
					out[0] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex16[t][0];
					out[1] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex16[t][1];
					out[2] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex16[t][2];
				}
				else
				if (submeshData[submesh].has32BitIndexBuffer)
				{
					// indexed 32bit
					out[0] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex32[t][0];
					out[1] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex32[t][1];
					out[2] = numVerticesInSkippedSubmeshes + submeshData[submesh].iterIndex32[t][2];
				}
				else
				{
					// nonindexed
					out[0] = numVerticesInSkippedSubmeshes + t*3;
					out[1] = numVerticesInSkippedSubmeshes + t*3+1;
					out[2] = numVerticesInSkippedSubmeshes + t*3+2;
				}
				return;
			}
			t -= submeshData[submesh].numTriangles;
			numVerticesInSkippedSubmeshes += submeshData[submesh].numVertices;
		}
		// v out of range ?
		NIASSERT(0);
	}

	virtual void getTriangleNormals(unsigned t, TriangleNormals& out) const
	{
		NIASSERT(t<numTriangles);
		if (!hasNormal)
		{
			// mesh doesn't contain normals, autogenerate them
			RRMesh::getTriangleNormals(t,out);
			return;
		}
		Triangle triangleIndices;
		RRMeshGamebryo::getTriangle(t,triangleIndices);
		for (unsigned i=0; i<3; i++)
		{
			unsigned v = triangleIndices[i];
			NIASSERT(v<numVertices);
			for (unsigned submesh=0; submesh<numSubmeshes; submesh++)
			{
				if (v<submeshData[submesh].numVertices)
				{
					out.vertex[i].normal = convertLocalDir(submeshData[submesh].iterNormal[v]);
					if (hasTangents)
					{
						out.vertex[i].tangent = convertLocalDir(submeshData[submesh].iterTangent[v]);
						out.vertex[i].bitangent = convertLocalDir(submeshData[submesh].iterBitangent[v]);
					}
					else
					{
						// mesh doesn't contain tangents or bitangents, autogenerate both
						out.vertex[i].buildBasisFromNormal();
					}
					goto one_third_done;
				}
				v -= submeshData[submesh].numVertices;
			}
			// vertex out of range?
			RRMesh::getTriangleNormals(t,out);
			return;
			one_third_done:;
		}
	}

	virtual bool getTriangleMapping(unsigned t, TriangleMapping& out, unsigned channel) const
	{
		if (t>=numTriangles)
		{
			NIASSERT(t<numTriangles);
			return false;
		}
		if (!hasTexcoord[channel])
		{
			if (channel==CH_LIGHTMAP)
			{
				// mesh doesn't contain unwrap, autogenerate it
				return RRMesh::getTriangleMapping(t,out,0);
			}
			return false;
		}
		Triangle triangleIndices;
		RRMeshGamebryo::getTriangle(t,triangleIndices);
		for (unsigned i=0; i<3; i++)
		{
			unsigned v = triangleIndices[i];
			NIASSERT(v<numVertices);
			for (unsigned submesh=0; submesh<numSubmeshes; submesh++)
			{
				if (v<submeshData[submesh].numVertices)
				{
					out.uv[i] = convertUv(submeshData[submesh].iterTexcoord[channel][v]);
					goto one_third_done;
				}
				v -= submeshData[submesh].numVertices;
			}
			// vertex out of range?
			return false;
			one_third_done:;
		}
		return true;
	}

	NiMesh* mesh;
private:
	unsigned numVertices;
	unsigned numTriangles;

	// iterators created in constructor and used in getXxx() functions
	// it's more complicated but faster than creating them temporarily in getXxx()
	struct SubmeshData
	{
		// indices
		unsigned numTriangles;
		bool has16BitIndexBuffer;
		bool has32BitIndexBuffer;
		NiIndexedPrimitiveIterator16 iterIndex16;
		NiIndexedPrimitiveIterator32 iterIndex32;
		// vertices
		unsigned numVertices;
		NiTStridedRandomAccessIterator<NiPoint3> iterPosition;
		NiTStridedRandomAccessIterator<NiPoint3> iterNormal;
		NiTStridedRandomAccessIterator<NiPoint3> iterTangent;
		NiTStridedRandomAccessIterator<NiPoint3> iterBitangent;
		NiTStridedRandomAccessIterator<NiPoint2> iterTexcoord[CH_COUNT];

		SubmeshData() : iterIndex16((NiUInt16*)0,0,0), iterIndex32((NiUInt32*)0,0,0) {}
	};
	unsigned numSubmeshes;
	SubmeshData* submeshData;
	bool hasPosition;
	bool hasNormal;
	bool hasTangents;
	bool hasTexcoord[CH_COUNT];
};


// extension used by cache
class RRObjectGamebryoBase : public RRObject
{
public:
	unsigned meshIndex;
	NiMesh* mesh;
};


#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
////////////////////////////////////////////////////////////////////////////
//
// GamebryoLightCache

//! Per-light acceleration structure, resolves shadow casting/receiving queries in a few CPU ticks.
class GamebryoLightCache
{
public:
	NiDynamicEffect* gamebryoLight;

	//! Initializes cache, lights don't cast shadows until you call update().
	GamebryoLightCache(NiDynamicEffect* _gamebryoLight)
	{
		gamebryoLight = _gamebryoLight;
	}

	//! Updates our internal copy of Gamebryo state.
	void update(RRObjects& objects)
	{
		isCaster.clear();
		isCaster.resize(objects.size());
		isReceiver.clear();
		isReceiver.resize(objects.size());
		if (gamebryoLight)
		{
			NiShadowGenerator* shadowGenerator = gamebryoLight->GetShadowGenerator();
			if (shadowGenerator && shadowGenerator->GetActive())
			{
				// Cache casters.
				NiRenderObjectList casterList;
				shadowGenerator->GetCasterGeometryList(casterList);
				while (!casterList.IsEmpty())
				{
					NiRenderObject* renderObject = casterList.RemoveHead();
					for (unsigned i=0;i<objects.size();i++)
					{
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i].object);
						if (object->mesh==renderObject)
						{
							RR_ASSERT(object->meshIndex<isCaster.size());
							isCaster[object->meshIndex] = true;
						}
					};
				}
				// Cache receivers.
				NiRenderObjectList receiverList;
				shadowGenerator->GetReceiverGeometryList(receiverList);
				while (!receiverList.IsEmpty())
				{
					NiRenderObject* renderObject = receiverList.RemoveHead();
					for (unsigned i=0;i<objects.size();i++)
					{
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i].object);
						if (object->mesh==renderObject)
						{
							RR_ASSERT(object->meshIndex<isReceiver.size());
							isReceiver[object->meshIndex] = true;
						}
					}
				}
			}
		}
	}

	//! Are given parameters caster and receiver for this light?
	//! Purpose of cache = quick answer to this question.
	bool areCasterAndReceiver(const RRObjectGamebryoBase* caster, const RRObjectGamebryoBase* receiver) const
	{
		return isCaster[caster->meshIndex] && isReceiver[receiver->meshIndex];
	}

private:
	std::vector<bool> isCaster;
	std::vector<bool> isReceiver;
};

#endif // SUPPORT_DISABLED_LIGHTING_SHADOWING


////////////////////////////////////////////////////////////////////////////
//
// Material detection
//
// Non cached, to be called at most once per mesh.

static RRMaterial detectMaterial(NiMesh* mesh, float emissiveMultiplier)
{
	RRMaterial material;
	// detect material properties
	NiStencilProperty* pkStencilProperty = NiDynamicCast(NiStencilProperty, mesh->GetProperty(NiProperty::STENCIL));
	NiStencilProperty::DrawMode drawMode = pkStencilProperty ? pkStencilProperty->GetDrawMode() : NiStencilProperty::DRAW_CCW;
	switch(drawMode)
	{
		case NiStencilProperty::DRAW_CCW:
			// visible from front side
			material.reset(false);
			break;
		case NiStencilProperty::DRAW_CW:
			// visible from back side
			{
				material.reset(false);
				RRSideBits sb = material.sideBits[0];
				material.sideBits[0] = material.sideBits[1];
				material.sideBits[1] = sb;
			}
			break;
		case NiStencilProperty::DRAW_BOTH:
			// visible from both sides
			material.reset(true);
			break;
		case NiStencilProperty::DRAW_CCW_OR_BOTH:
			// visible at least from front side (camera never gets behind face to see its back side)
			material.reset(true);
			break;
		default:
			NIASSERT(0);
	}
	// use material detector if available
	NiMaterialDetector* materialDetector = NiMaterialDetector::GetInstance();
	if (materialDetector)
	{
		NiMaterialPointValuesPtr spColorValues = materialDetector->CreatePointMaterialTextures(mesh);
		material.diffuseEmittance.texture = convertTexture(spColorValues->m_spEmittanceTexture);
		material.diffuseEmittance.texcoord = CH_EMISSIVE;
		material.diffuseReflectance.texture = convertTextureAndSubtract(spColorValues->m_spDiffuseTexture,material.diffuseEmittance.texture);
		material.diffuseReflectance.texcoord = CH_DIFFUSE;
		material.specularReflectance.texture = convertTextureAndSubtract(spColorValues->m_spSpecularTexture,material.diffuseEmittance.texture);
		material.specularReflectance.texcoord = CH_SPECULAR;
		material.specularTransmittance.texture = convertTextureAndSubtract(spColorValues->m_spTransmittedTexture,material.diffuseEmittance.texture);
		material.specularTransmittance.texcoord = CH_DIFFUSE; // transmittance has its own texture, but uv is shared with diffuse
		material.specularTransmittanceInAlpha = false;
		material.lightmapTexcoord = CH_LIGHTMAP;
		material.diffuseEmittance.multiplyAdd(RRVec4(emissiveMultiplier),RRVec4(0)); // must be done after all subtractions
		RRScaler* scaler = RRScaler::createFastRgbScaler();
		material.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
		delete scaler;
		// optional - corrects invalid properties
		//material.validate();
	}
	material.updateSideBitsFromColors();
	material.name = mesh->GetActiveMaterial()->GetName();
	return material;
}


////////////////////////////////////////////////////////////////////////////
//
// MaterialCacheGamebryo
//
// In Gamebryo, each mesh has unique material.
// However, after closer inspection, some materials are identical.
// MaterialCacheGamebryo groups meshes with identical material and creates only one RRMaterial per group of meshes, to save memory and time.
// MaterialCacheGamebryo::getMaterial() replaces detectMaterial().

class MaterialCacheGamebryo
{
public:
	MaterialCacheGamebryo(float _emissiveMultiplier)
	{
		defaultMaterial.reset(false);
		emissiveMultiplier = _emissiveMultiplier;
	}
	// Looks for material in cache. Not found -> creates new material and stores it in cache.
	// Called once per mesh.
	RRMaterial* getMaterial(NiMesh* mesh)
	{
		if (!mesh)
		{
			return &defaultMaterial;
		}
		// quickly search in map by NiMaterial
		SlowCache* slowCache;
		{
			FastCache::iterator i = fastCache.find(mesh->GetActiveMaterial());
			if (i!=fastCache.end())
			{
				//RRReporter::report(INF1,"1 in cache\n");
				slowCache = &(*i).second;
			}
			else
			{
				//RRReporter::report(INF1,"1 new\n");
				slowCache = &( fastCache[mesh->GetActiveMaterial()] );
			}
		}
		// slowly scan whole list by isEqualMaterial
		for (SlowCache::iterator i=slowCache->begin();i!=slowCache->end();++i)
		{
			if (isEqualMaterial(mesh,(*i).mesh))
			{
				//RRReporter::report(INF1,"2 in cache\n");
				return &(*i).material;
			}
		}
		//RRReporter::report(INF1,"2 new\n");
		// push CacheElement, then assign material
		// (RRMaterial must not be assigned to temporary CacheElement, becuase ~CacheElement at the end of this scope would delete material textures)
		slowCache->push_front(CacheElement(mesh));
		return &( slowCache->begin()->material = detectMaterial(mesh,emissiveMultiplier) );
	}
private:
	RRMaterial defaultMaterial;
	float emissiveMultiplier;

	struct CacheElement
	{
		const NiMesh* mesh;
		RRMaterial material;

		CacheElement(const NiMesh* _mesh)
		{
			mesh = _mesh;
		}
		~CacheElement()
		{
			// clean what we allocated in detectMaterial()
			RR_SAFE_DELETE(material.diffuseReflectance.texture);
			RR_SAFE_DELETE(material.specularReflectance.texture);
			RR_SAFE_DELETE(material.specularTransmittance.texture);
			RR_SAFE_DELETE(material.diffuseEmittance.texture);
		}
	};
	typedef std::list<CacheElement> SlowCache;
	typedef std::map<const NiMaterial*,SlowCache> FastCache;
	FastCache fastCache;

	static bool isEqualMaterial(const NiMesh* mesh1, const NiMesh* mesh2)
	{
		// proper use of FastCache ensures that only meshes with identical active material are compared
		RR_ASSERT(mesh1->GetActiveMaterial()==mesh2->GetActiveMaterial());

		NiPropertyState* ps1 = mesh1->GetPropertyState();
		NiPropertyState* ps2 = mesh2->GetPropertyState();
		#define TEST_FAST(prop) if (!ps1->Get##prop()->IsEqualFast(*ps2->Get##prop())) return false;
		#define TEST_SLOW(prop) if (!ps1->Get##prop()->IsEqual    ( ps2->Get##prop())) return false;
		TEST_FAST(Alpha);
		TEST_FAST(Dither);
		TEST_FAST(Fog);
		TEST_FAST(Material);
		TEST_FAST(RendererSpecific);
		TEST_FAST(Shade);
		TEST_FAST(Specular);
		TEST_FAST(Stencil);
		TEST_FAST(VertexColor);
		TEST_FAST(Wireframe);
		TEST_FAST(ZBuffer);
		TEST_SLOW(Texturing); // fast test does not work for Texturing, always sees difference
		return true;
	}
};


////////////////////////////////////////////////////////////////////////////
//
// RRObjectGamebryo

class RRObjectGamebryo : public RRObjectGamebryoBase
{
public:
	// creates new RRObject from NiMesh
	// return NULL for meshes of unsupported formats
	static RRObjectGamebryo* create(NiMesh* _mesh, RRObject::LodInfo _lodInfo, MaterialCacheGamebryo& _materialCache, bool& _aborting)
	{
		if (!_mesh)
		{
			return NULL;
		}
		if (_aborting)
		{
			return NULL;
		}
		if (!NiLightMapUtility::IsLightMapMesh(_mesh))
		{
			return NULL;
		}

		_mesh->UpdateEffects();
		RRMesh* mesh = new RRMeshGamebryo(_mesh);
		if (mesh->getNumTriangles()==0)
		{
			delete mesh;
			return NULL;
		}
		//mesh->checkConsistency();
		const RRCollider* collider = RRCollider::create(mesh, RRCollider::IT_LINEAR, _aborting);
		if (!collider)
		{
			delete mesh;
			return NULL;
		}
		return new RRObjectGamebryo(_mesh, collider, _lodInfo, _materialCache);
	}
	virtual ~RRObjectGamebryo()
	{
		delete illumination;
		delete collider->getMesh();
		delete collider;
	}

	RRObjectIllumination* getIllumination()
	{
		return illumination;
	}
	virtual const RRCollider* getCollider() const
	{
		return collider;
	}
	virtual const RRMatrix3x4* getWorldMatrix()
	{
		return &worldMatrix;
	}

	virtual const RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
	{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
		// Support for disabled lighting or shadowing.
		if (light)
		{
			NiDynamicEffectState* dynamicEffectState = mesh->GetEffectState();
			if (!dynamicEffectState)
			{
				// This mesh is not lit and it does not cast shadows.
				return NULL;
			}
			const GamebryoLightCache* gamebryoLightCache = (GamebryoLightCache*)light->customData;
			if (!gamebryoLightCache)
			{
				// This light was added manually, it's not adapted from Gamebryo.
				// It illuminates all objects / casts all shadows.
				return material;
			}
			if (receiver)
			{
				// Does this mesh cast shadows from light to receiver?
				if (gamebryoLightCache->areCasterAndReceiver(this,(RRObjectGamebryo*)receiver))
				{
					// Yes, this mesh casts shadow from light to receiver.
					return material;
				}
				// No, this mesh doesn't cast shadow from light to receiver.
				return NULL;
			}
			else
			{
				// Is this mesh lit by light?
				NiDynEffectStateIter relevantLightIter = dynamicEffectState->GetLightHeadPos();
				NiLight* relevantLight;
				while (relevantLight=dynamicEffectState->GetNextLight(relevantLightIter))
				{
					if (relevantLight==gamebryoLightCache->gamebryoLight)
					{
						// Yes, light affects this mesh.
						return material;
					}
				}
				// No, light does not affect this mesh.
				return NULL;
			}
		}
		else
#endif // SUPPORT_DISABLED_LIGHTING_SHADOWING
		// Standard unconditional query: what is this triangle's material?
		{
			return material;
		}
	}

	virtual void getTriangleLod(unsigned t, RRObject::LodInfo& out) const
	{
		out = lodInfo;
	}

private:
	RRObjectGamebryo(NiMesh* _mesh, const RRCollider* _collider, RRObject::LodInfo _lodInfo, MaterialCacheGamebryo& _materialCache)
	{
		NIASSERT(_mesh);
		NIASSERT(_collider);
		mesh = _mesh;
		meshIndex = 0;
		collider = _collider;
		lodInfo = _lodInfo;
		worldMatrix = convertMatrix(mesh->GetWorldTransform());
		illumination = new RRObjectIllumination(collider->getMesh()->getNumVertices());
		material = _materialCache.getMaterial(mesh);
	}

	const RRCollider* collider;
	RRObjectIllumination* illumination;
	RRMatrix3x4 worldMatrix;
	RRMaterial* material;
	LodInfo lodInfo;
};


////////////////////////////////////////////////////////////////////////////
//
// RRObjectsGamebryo

class RRObjectsGamebryo : public RRObjects
{
public:
	RRObjectsGamebryo(NiScene* _pkEntityScene, bool& _aborting, float _emissiveMultiplier)
		: materialCache(_emissiveMultiplier)
	{
		pkEntityScene = _pkEntityScene;
		RRObject::LodInfo lodInfo;
		lodInfo.base = 0; // start hierarchy traversal with base 0 marking we are not in LOD
		lodInfo.level = 0;
		unsigned uiCount = pkEntityScene->GetEntityCount();
		for (unsigned int uiEntity=0; uiEntity < uiCount; uiEntity++)
		{
			NiEntityInterface* pkEntity = pkEntityScene->GetEntityAt(uiEntity);
			const char* pcName = pkEntity->GetName();
			unsigned int uiSceneRootCount;
			NiFixedString kSceneRootPointer = "Scene Root Pointer";
			NIASSERT(pkEntity);
			if (pkEntity->GetElementCount(kSceneRootPointer, uiSceneRootCount))
			{
	 			for (unsigned int ui = 0; ui < uiSceneRootCount; ui++)
				{
					NiObject* pkObject;
					if (pkEntity->GetPropertyData( kSceneRootPointer, pkObject, ui))
					{
						NiAVObject* pkRoot = NiDynamicCast(NiAVObject, pkObject);
						if (pkRoot)
						{
							addNode(pkRoot,lodInfo,_aborting);
						}
					}
				}
			}
		}
	}

	virtual ~RRObjectsGamebryo()
	{
		for (unsigned int i=0; i<size(); i++)
		{
			delete (*this)[i].object;
		}
	}

	virtual void recommendLayerParameters(RRObjects::LayerParameters& layerParameters) const
	{
		class LightmapFunctor : public NiVisitLightMapMeshFunctor
		{
		public:
			virtual bool operator() (NiMesh* pkMesh, NiLightMapMeshProperties kProps)
			{
				if (pkMesh==object->mesh)
				{
					NiString kDirectoryName = NiString(layerParameters->suggestedPath);
					size_t pathlen = strlen(layerParameters->suggestedPath);
					if (pathlen
						&& layerParameters->suggestedPath[pathlen-1]!='/'
						&& layerParameters->suggestedPath[pathlen-1]!='\\') kDirectoryName += "/";
					kDirectoryName += kProps.m_pcEntityDirectory;
					if (!NiFile::DirectoryExists(kDirectoryName))
					{
						NiFile::CreateDirectoryRecursive(kDirectoryName);
						if (!NiFile::DirectoryExists(kDirectoryName))
						{
							RRReporter::report(WARN,"Light map directory \"%s\" cannot be created for file \"%s\".", kDirectoryName, kProps.m_pcLightMapFilename);
						}
					}
					free(layerParameters->actualFilename);
					layerParameters->actualFilename = _strdup(kDirectoryName + "/" + kProps.m_pcLightMapFilename + "." + layerParameters->suggestedExt);
					return true;
				}
				return false;
			}
			LayerParameters* layerParameters;
			RRObjectGamebryo* object;
		};

		if ((unsigned)layerParameters.objectIndex>size())
		{
			RRReporter::report(ERRO,"recommendLayerParameters(): objectIndex out of range\n");
			return;
		}

		// fill filename
		RR_SAFE_FREE(layerParameters.actualFilename);
		LightmapFunctor lightmapFunctor;
		lightmapFunctor.layerParameters = &layerParameters;
		lightmapFunctor.object = (RRObjectGamebryo*)(*this)[layerParameters.objectIndex].object;
		NiLightMapUtility::VisitLightMapMeshes(pkEntityScene,lightmapFunctor);

		// fill size, type, format
		int w,h;
		NiLightMapUtility::GetLightMapResolution(w,h,lightmapFunctor.object->mesh,layerParameters.suggestedPixelsPerWorldUnit,layerParameters.suggestedMinMapSize,layerParameters.suggestedMaxMapSize);
		layerParameters.actualWidth = w;
		layerParameters.actualHeight = h;
		layerParameters.actualType = BT_2D_TEXTURE;
		layerParameters.actualFormat = BF_RGB;
		layerParameters.actualScaled = true;
	}

private:
	// Adds all instances from node and his subnodes to 'objects'.
	// lodInfo.base==0 marks we are not in LOD
	void addNode(const NiAVObject* object, RRObject::LodInfo lodInfo, bool& aborting)
	{
		if (!object)
		{
			return;
		}

		if (aborting)
		{
			return;
		}

		// recurse into children
		if (NiIsKindOf(NiNode,object))
		{
			NiNode* node = (NiNode*)object;
			for (unsigned int i=0; i<node->GetArrayCount(); i++)
			{
				// mark nodes below NiLODNode as LODs
				if (NiIsKindOf(NiLODNode,object))
				{
					// this is LOD, give all LODs the same base and unique level
					lodInfo.base = object;
					lodInfo.level = i; // TODO: is GetAt(0) level 0?
				}
				// add node
				addNode(node->GetAt(i),lodInfo,aborting);
			}
		}
		// adapt single node
		if (NiIsExactKindOf(NiMesh,object))
		{
			if (!lodInfo.base)
			{
				// this is not LOD, give it unique base and level 0
				// (only for this mesh, must not be propagated into children)
				lodInfo.base = object;
				lodInfo.level = 0;
			}
			RRObjectGamebryo* rrObject = RRObjectGamebryo::create((NiMesh*)object,lodInfo,materialCache,aborting);
			if (rrObject)
			{
				push_back(RRIlluminatedObject(rrObject, rrObject->getIllumination()));
			}
		}
	}

	MaterialCacheGamebryo materialCache;
	NiScene* pkEntityScene; // used only to query lightmap names
};


////////////////////////////////////////////////////////////////////////////
//
// RRLightsGamebryo

class RRLightsGamebryo : public RRLights
{
public:
	RRLightsGamebryo(NiScene* scene)
	{
		class AddLightFunctor : public NiVisitLightMapLightFunctor
		{
		public:
			virtual bool operator() (NiLight* pkLight, NiLightMapLightProperties kProps = NiLightMapLightProperties())
			{
				lights->addLight(pkLight, kProps);
				return true;
			}
			RRLightsGamebryo* lights;
		};

		AddLightFunctor addLightFunctor;
		addLightFunctor.lights = this;
		NiLightMapUtility::VisitLightMapLights(scene,addLightFunctor);
	}

	void addLight(NiLight* light, NiLightMapLightProperties props = NiLightMapLightProperties())
	{
		if (light->GetSwitch())
		{
			light->Update(0.f);

			// diffuse color: Lightsprint supports only one light color, it's taken from Gamebryo diffuse color
			// ambient color: we ignore ambient lights, level designers should remove them. GI replaces ambient
			// specular color: we ignore it in hope it's rougly the same as diffuse color. this may be changed in future

			RRLight* rrLight = NULL;

			// directional light
			if (NiIsExactKindOf(NiDirectionalLight, light))
			{
				NiDirectionalLight* directionalLight = (NiDirectionalLight*) light;

				RRVec3 dir = convertWorldDir(directionalLight->GetWorldDirection());
				RRVec3 color = convertColor(directionalLight->GetDiffuseColor()) * directionalLight->GetDimmer();

				rrLight = RRLight::createDirectionalLight(dir, color, false);
			}

			// point light
			else if (NiIsExactKindOf(NiPointLight, light))
			{
				NiPointLight* pointLight = (NiPointLight*) light;

				RRVec3 pos = convertWorldPos(pointLight->GetWorldLocation()) * SCALE_GEOMETRY;
				RRVec3 color = convertColor(pointLight->GetDiffuseColor()) * pointLight->GetDimmer();
				RRVec4 poly(pointLight->GetConstantAttenuation(),pointLight->GetLinearAttenuation()*SCALE_GEOMETRY,pointLight->GetQuadraticAttenuation()*SCALE_GEOMETRY*SCALE_GEOMETRY,1);

				rrLight = RRLight::createPointLightPoly(pos, color, poly);
			}

			// spot light
			else if (NiIsExactKindOf(NiSpotLight, light))
			{
				NiSpotLight* spotLight = (NiSpotLight*) light;

				RRVec3 pos = convertWorldPos(spotLight->GetWorldLocation()) * SCALE_GEOMETRY;
				RRVec3 dir = convertWorldDir(spotLight->GetWorldDirection());
				RRVec3 color = convertColor(spotLight->GetDiffuseColor()) * spotLight->GetDimmer();
				RRVec4 poly(spotLight->GetConstantAttenuation(),spotLight->GetLinearAttenuation()*SCALE_GEOMETRY,spotLight->GetQuadraticAttenuation()*SCALE_GEOMETRY*SCALE_GEOMETRY,1);
				float innerAngle = 0.0174533f * spotLight->GetInnerSpotAngle();
				float outerAngle = 0.0174533f * spotLight->GetSpotAngle();
				float spotExponent = spotLight->GetSpotExponent();

				rrLight = RRLight::createSpotLightPoly(pos, color, poly, dir, outerAngle, outerAngle-innerAngle, spotExponent);
			}

			// common light properties
			if (rrLight)
			{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
				rrLight->customData = new GamebryoLightCache(light);
				NiShadowGenerator* shadowGenerator = light->GetShadowGenerator();
				rrLight->castShadows = shadowGenerator && shadowGenerator->GetActive();
#else
				rrLight->castShadows = true;
#endif
				push_back(rrLight);
			}
		}
	}

	virtual ~RRLightsGamebryo()
	{
		for (unsigned int i=0; i<size(); i++)
		{
			delete (*this)[i];
		}
	}
};


////////////////////////////////////////////////////////////////////////////
//
// RRSceneGamebryo

//! Lightsprint interface for Gamebryo scene in .gsa file.
class RRSceneGamebryo : public RRScene
{
public:
	//! Imports scene from .gsa file.
	//! \param filename
	//!  Scene file name.
	//! \param initGamebryo
	//!  True to treat Gamebryo as completely unitialized and initialize/shutdown it.
	//!  To be used when importing .gsa into generic Lightsprint samples.
	//!  \n False to skip initialization/shutdown sequence,
	//!  to be used when running Lightsprint code from Gamebryo app, with Gamebryo already initialized.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	RRSceneGamebryo(const char* filename, bool initGamebryo, bool& aborting, float emissiveMultiplier = 1);
	virtual ~RRSceneGamebryo();
	
	//! Loader suitable for RRScene::registerLoader().
	static RRScene* load(const char* filename, float scale, bool stripPaths, bool* aborting, float emissiveMultiplier)
	{
		bool not_aborting = false;
		return new RRSceneGamebryo(filename,false,aborting ? *aborting : not_aborting,emissiveMultiplier);
	}

	virtual const RRObjects* getObjects() {return objects;}
	virtual const RRLights* getLights() {return lights;}

protected:
	void updateCastersReceiversCache();

	RRObjects*                objects;
	RRLights*                 lights;
	bool                      initGamebryo;
	class NiScene*            pkEntityScene;
};

RRSceneGamebryo::RRSceneGamebryo(const char* _filename, bool _initGamebryo, bool& _aborting, float _emissiveMultiplier)
{
	//RRReportInterval report(INF1,"Loading scene %s...\n",_filename); already reported one level up
	objects = NULL;
	lights = NULL;
	initGamebryo = _initGamebryo;

	// Gamebryo init
	if (initGamebryo)
	{
		RRReportInterval report(INF2,"Gamebryo init...\n");

		WNDCLASS wc;
		wc.style         = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = DefWindowProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = NULL;
		wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName  = 0;
		wc.lpszClassName = "a";
		RegisterClass(&wc);

		RECT rect = {0, 0, 100, 100};
		DWORD dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX;
		AdjustWindowRect(&rect, dwStyle, false);
		HWND hWnd = CreateWindow("a", "a", dwStyle, 0, 0, rect.right-rect.left, rect.bottom-rect.top, NULL, NULL, NULL, NULL);

		NiInit();
		NiMaterialDetector::Init(hWnd);
		NiShadowManager::Initialize();
		NIASSERT(NiDataStream::GetFactory());
		NiDataStream::GetFactory()->SetCallback(NiDataStreamFactory::ForceCPUReadAccessCallback);
		NiImageConverter::SetImageConverter(NiNew NiDevImageConverter);
		NiTexture::SetMipmapByDefault(true);
		NiSourceTexture::SetUseMipmapping(true);
	}

	// load .gsa
	NiEntityStreamingAscii kStream;
	if (!kStream.Load(_filename))
	{
		RRReporter::report(ERRO,"Scene %s not loaded.\n",_filename);
		return;
	}
	if (kStream.GetSceneCount() < 1)
	{
		RRReporter::report(ERRO,"Scene %s empty.\n",_filename);
		return;
	}
	pkEntityScene = kStream.GetSceneAt(0);
	pkEntityScene->IncRefCount();

	// update scene in memory
	NiExternalAssetManagerPtr spAsset = NiNew NiExternalAssetManager;
	spAsset->SetAssetFactory(NiFactories::GetAssetFactory());
	NiDefaultErrorHandlerPtr spError = NiNew NiDefaultErrorHandler;
	pkEntityScene->Update(0.0f, spError, spAsset);

    unsigned int uiCount = pkEntityScene->GetEntityCount();
    for (unsigned int uiEntity = 0; uiEntity < uiCount; uiEntity++)
    {
        NiEntityInterface* pkEntity = pkEntityScene->GetEntityAt(uiEntity);
        pkEntity->Update(NULL, 0.0, spError, spAsset);
    }

	// adapt lights
	lights = adaptLightsFromGamebryo(pkEntityScene);

	// adapt meshes
	objects = adaptObjectsFromGamebryo(pkEntityScene,_aborting,_emissiveMultiplier);

	updateCastersReceiversCache();

	if ((!objects || !objects->size()) && !lights)
	{
		RRReporter::report(WARN,"Scene %s empty.\n",_filename);
	}
}

RRSceneGamebryo::~RRSceneGamebryo()
{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
	for (unsigned i=0;lights && i<lights->size();i++)
	{
		delete (GamebryoLightCache*)((*lights)[i]->customData);
	}
#endif
	delete lights;
	delete objects;
	pkEntityScene->DecRefCount();

	// Gamebryo shutdown
	if (initGamebryo)
	{
		RRReportInterval report(INF2,"Gamebryo shutdown...\n");
		NiShadowManager::Shutdown();
		NiMaterialDetector::Shutdown();
		NiShutdown();
	}
}

void RRSceneGamebryo::updateCastersReceiversCache()
{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
	if (objects && lights)
	{
		// Reindex meshes.
		for (unsigned i=0;i<objects->size();i++)
		{
			((RRObjectGamebryo*)((*objects)[i].object))->meshIndex = i;
		}
		// Update cache in lights.
		for (unsigned i=0;i<lights->size();i++)
		{
			GamebryoLightCache* cache = (GamebryoLightCache*)((*lights)[i]->customData);
			cache->update(*objects);
		}
	}
#endif
}


////////////////////////////////////////////////////////////////////////////
//
// main

RRObjects* adaptObjectsFromGamebryo(NiScene* scene, bool& aborting, float emissiveMultiplier)
{
	return new RRObjectsGamebryo(scene,aborting,emissiveMultiplier);
}

RRLights* adaptLightsFromGamebryo(NiScene* scene)
{
	return new RRLightsGamebryo(scene);
}

void registerLoaderGamebryo()
{
	RRScene::registerLoader("gsa",RRSceneGamebryo::load);
}

#endif // SUPPORT_GAMEBRYO
