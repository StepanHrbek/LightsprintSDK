// --------------------------------------------------------------------------
// Lightsprint adapters for Gamebryo scene.
// Copyright (C) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------
#pragma unmanaged

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

#include "NiMaterialDetector.h" // includes NiVersion, defines GAMEBRYO_MAJOR_VERSION
#include "RRObjectGamebryo.h"

#include <NiMain.h>
#include <NiMesh.h>
#include <NiDataStreamPrimitiveLock.h>

#ifdef RR_IO_BUILD
	// building LightsprintIO, link dynamic, libraries don't have suffix
	#define LIB_SUFFIX
#else
	// building plugin, link dynamic, libraries have suffix
	#define LIB_SUFFIX NI_DLL_SUFFIX
#endif

#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"NiAnimation" LIB_SUFFIX)
#pragma comment(lib,"NiDX9Renderer" LIB_SUFFIX)
#pragma comment(lib,"NiFloodgate" LIB_SUFFIX)
#pragma comment(lib,"NiMain" LIB_SUFFIX)
#pragma comment(lib,"NiMesh" LIB_SUFFIX)
#pragma comment(lib,"NiSystem" LIB_SUFFIX)

#if GAMEBRYO_MAJOR_VERSION==2
	// Gamebryo GI Package (for Gamebryo 2.6)
	#include <NiLightMapUtility.h>
	#pragma comment(lib,"NiLightMapUtility")
	#pragma comment(lib,"NiLightMapMaterial")
#else
	// Lightspeed GI Package (for Gamebryo 3.x)
	#include "egmGI/GIService.h"
	#include "egmGI/MeshUtility.h"
	#pragma comment(lib,"egmGI" LIB_SUFFIX)
	#pragma comment(lib,"NiGIMaterial" LIB_SUFFIX)
	// additional libs required by Gamebryo 3.x
	#include "efd/ServiceManager.h"
	#pragma comment(lib,"ecr" LIB_SUFFIX)
	#pragma comment(lib,"egf" LIB_SUFFIX)
	#pragma comment(lib,"efd" LIB_SUFFIX)
	#pragma comment(lib,"NiInput" LIB_SUFFIX)
	#pragma comment(lib,"NiApplication")
	#include "NiApplication.h"
	NiApplication* NiApplication::Create() {return NULL;}
#endif

// .gsa load
#include <NiEntity.h>
#pragma comment(lib,"NiEntity" LIB_SUFFIX)
#if GAMEBRYO_MAJOR_VERSION==2
	#pragma comment(lib,"TinyXML")
#endif

// necessary for processing certain .gsa nodes that would be otherwise ignored
#include <NiCollision.h>
#pragma comment(lib,"NiCollision" LIB_SUFFIX)
#include <NiPortal.h>
#pragma comment(lib,"NiPortal" LIB_SUFFIX)
#include <NiParticle.h>
#pragma comment(lib,"NiParticle" LIB_SUFFIX)

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

inline RRVec3 convertWorldRotToDir(const NiPoint3& v)
{
	NiMatrix3 matrix;
	matrix.FromEulerAnglesXYZ(RR_DEG2RAD(-v.x),RR_DEG2RAD(v.y),RR_DEG2RAD(v.z));
	NiPoint3 dir = matrix*NiPoint3(1,0,0);
	return RRVec3(dir.x,-dir.z,dir.y);
}

inline RRVec3 convertColor_(const NiPoint3& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertColor(const NiColor& c)
{
	return RRVec3(c.r, c.g, c.b);
}

#if GAMEBRYO_MAJOR_VERSION==3
inline RRVec3 convertColor(const efd::Color& c)
{
	return RRVec3(c.r, c.g, c.b);
}
#endif

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

//! Converts _add and then subtracts _sub from result.
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

//! Texcoord channel numbers offered by this adapter.
//! When Lightsprint solver asks for data e.g. from CH_EMISSIVE channel,
//! adapter translates it to NiTexturingProperty::GetGlowMap()->GetTextureIndex()
//! and returns data from appropriate Gamebryo vertex buffer.
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
	RRMeshGamebryo(NiMesh* _mesh, unsigned _lightmapTexcoord)
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
		NiDataStreamElementLock kLockLightmapUv(mesh, NiCommonSemantics::TEXCOORD(), _lightmapTexcoord, NiDataStreamElement::F_FLOAT32_2, NiDataStream::LOCK_READ);
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
			// triangle out of range
			NIASSERT(t<numTriangles);
			return false;
		}
		if (channel>=CH_COUNT)
		{
			// we have no data for this channel
			return false;
		}
		if (!hasTexcoord[channel])
		{
			if (channel==CH_LIGHTMAP)
			{
				// mesh doesn't contain unwrap, autogenerate it
				return RRMesh::getTriangleMapping(t,out,0);
			}
			if (hasTexcoord[CH_DIFFUSE])
			{
				// it is possible that material detector found emissive or specular map, but there is no such
				// uv+map in mesh, try to fallback to diffuse uv
				channel = CH_DIFFUSE;
			}
			else
			{
				return false;
			}
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
#if GAMEBRYO_MAJOR_VERSION==3
		if (channel==CH_LIGHTMAP)
		{
			// Gamebryo 3.x inverted y
			out.uv[0][1] = 1-out.uv[0][1];
			out.uv[1][1] = 1-out.uv[1][1];
			out.uv[2][1] = 1-out.uv[2][1];
		}
#endif
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
#if GAMEBRYO_MAJOR_VERSION==2 // Although written for both 2.6 and 3.x, only 2.6 currently uses this class.
////////////////////////////////////////////////////////////////////////////
//
// GamebryoLightCache

//! Per-light acceleration structure, resolves shadow casting/receiving queries in a few CPU ticks.
//! Gamebryo 3.x Light entity has all shadows enabled/disabled at once so this structure is not necessary.
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
#if GAMEBRYO_MAJOR_VERSION==2
				// Cache casters.
				NiRenderObjectList casterList;
				shadowGenerator->GetCasterGeometryList(casterList);
				while (!casterList.IsEmpty())
				{
					NiRenderObject* renderObject = casterList.RemoveHead();
					for (unsigned i=0;i<objects.size();i++)
					{
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i]);
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
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i]);
						if (object->mesh==renderObject)
						{
							RR_ASSERT(object->meshIndex<isReceiver.size());
							isReceiver[object->meshIndex] = true;
						}
					}
				}
#else
				// Cache casters.
				NiAVObjectRawList casterList;
				shadowGenerator->GetCasterList(casterList);
				while (!casterList.IsEmpty())
				{
					NiAVObject* renderObject = casterList.RemoveHead();
					for (unsigned i=0;i<objects.size();i++)
					{
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i]);
						if (object->mesh==renderObject)
						{
							RR_ASSERT(object->meshIndex<isCaster.size());
							isCaster[object->meshIndex] = true;
						}
					};
				}
				// Cache receivers.
				NiAVObjectRawList receiverList;
				shadowGenerator->GetReceiverList(receiverList);
				while (!receiverList.IsEmpty())
				{
					NiAVObject* renderObject = receiverList.RemoveHead();
					for (unsigned i=0;i<objects.size();i++)
					{
						RRObjectGamebryoBase* object = (RRObjectGamebryoBase*)(objects[i]);
						if (object->mesh==renderObject)
						{
							RR_ASSERT(object->meshIndex<isReceiver.size());
							isReceiver[object->meshIndex] = true;
						}
					}
				}
#endif
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

#endif // GAMEBRYO_MAJOR_VERSION==2
#endif // SUPPORT_DISABLED_LIGHTING_SHADOWING


////////////////////////////////////////////////////////////////////////////
//
// Material detection
//
// Non cached, to be called at most once per mesh.

struct MaterialInputs
{
	NiMesh* mesh;
	float emissiveMultiplier;

	MaterialInputs(NiMesh* _mesh, float _emissiveMultiplier)
	{
		mesh = _mesh;
		emissiveMultiplier = _emissiveMultiplier;
	}
};

static RRMaterial* detectMaterial(MaterialInputs _materialInputs)
{
	RRMaterial* materialPtr = new RRMaterial;
	RRMaterial& material = *materialPtr;

	// detect material properties
	NiStencilProperty* pkStencilProperty = NiDynamicCast(NiStencilProperty, _materialInputs.mesh->GetProperty(NiProperty::STENCIL));
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
		NiMaterialPointValuesPtr spColorValues = materialDetector->CreatePointMaterialTextures(_materialInputs.mesh);
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
		material.diffuseEmittance.multiplyAdd(RRVec4(_materialInputs.emissiveMultiplier),RRVec4(0)); // must be done after all subtractions

		// get average colors from textures
		RRScaler* scaler = RRScaler::createFastRgbScaler();
		material.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
		delete scaler;

		// autodetect keying
		material.updateKeyingFromTransmittance();

		// optional - corrects invalid properties
		//material.validate();
	}
	// optimize material flags
	material.updateSideBitsFromColors();

	material.name = _materialInputs.mesh->GetActiveMaterial() ? _materialInputs.mesh->GetActiveMaterial()->GetName() : NULL;
	return materialPtr;
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
	MaterialCacheGamebryo()
	{
		defaultMaterial.reset(false);
	}
	// Looks for material in cache. Not found -> creates new material and stores it in cache.
	// Called once per mesh.
	RRMaterial* getMaterial(MaterialInputs _materialInputs)
	{
		if (!_materialInputs.mesh)
		{
			return &defaultMaterial;
		}
		// quickly search in map by NiMaterial
		SlowCache* slowCache;
		{
			FastCache::iterator i = fastCache.find(_materialInputs.mesh->GetActiveMaterial());
			if (i!=fastCache.end())
			{
				//RRReporter::report(INF1,"1 in cache\n");
				slowCache = &(*i).second;
			}
			else
			{
				//RRReporter::report(INF1,"1 new\n");
				slowCache = &( fastCache[_materialInputs.mesh->GetActiveMaterial()] );
			}
		}
		// slowly scan whole list by isEqualMaterial
		for (SlowCache::iterator i=slowCache->begin();i!=slowCache->end();++i)
		{
			if (isEqualMaterial(_materialInputs,(*i).materialInputs))
			{
				//RRReporter::report(INF1,"2 in cache\n");
				return (*i).material;
			}
		}
		//RRReporter::report(INF1,"2 new\n");
		// push CacheElement, then assign material
		// (RRMaterial must not be assigned to temporary CacheElement, becuase ~CacheElement at the end of this scope would delete material textures)
		slowCache->push_front(CacheElement(_materialInputs));
		return slowCache->begin()->material = detectMaterial(_materialInputs);
	}
private:
	RRMaterial defaultMaterial;

	struct CacheElement
	{
		MaterialInputs materialInputs;
		RRMaterial* material;

		CacheElement(MaterialInputs _materialInputs)
			: materialInputs(_materialInputs)
		{
			material = NULL;
		}
		CacheElement& operator =(const CacheElement& a)
		{
			// added only because of assert, default operator would work fine
			// assignment is allowed only if material is NULL
			// copying non-NULL material would lead to double delete
			RR_ASSERT(!a.material);
			material = NULL;
			materialInputs = a.materialInputs;
		}
		~CacheElement()
		{
			delete material;
		}
	};
	typedef std::list<CacheElement> SlowCache;
	typedef std::map<const NiMaterial*,SlowCache> FastCache;
	FastCache fastCache;

	static bool isEqualMaterial(const MaterialInputs& mesh1, const MaterialInputs& mesh2)
	{
		if (mesh1.emissiveMultiplier!=mesh2.emissiveMultiplier) return false;

		// proper use of FastCache ensures that only meshes with identical active material are compared
		RR_ASSERT(mesh1.mesh->GetActiveMaterial()==mesh2.mesh->GetActiveMaterial());

		NiPropertyState* ps1 = mesh1.mesh->GetPropertyState();
		NiPropertyState* ps2 = mesh2.mesh->GetPropertyState();
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
// PropertyEnum

//! Denser and safer representation of Toolbench string enums.
enum PropertyEnum
{
	PE_INHERIT_FROM_LIGHTSPRINT_SCENE = 0,
	PE_VERTICES,
	PE_TEXTURE,
	PE_COMPRESSED_TEXTURE,
	PE_NON_DIRECTIONAL,
	PE_DIRECTIONAL,
	PE_RESOLUTION_CALCULATED,
	PE_RESOLUTION_FIXED,
	PE_TARGET_NONE // doesn't have Toolbench analog, we set this in case of error to disable lightmap build
};

#if GAMEBRYO_MAJOR_VERSION==3
//! Converts Toolbench enum to our enum. Sets out only if propertyString is valid.
bool getPropertyEnum(efd::utf8string propertyString, PropertyEnum& out)
{
	const char* propertyStrings[] =
	{
		"Inherit from LightsprintScene",
		"Vertices",
		"Texture",
		"Compressed Texture",
		"Non-directional",
		"Directional/RNM",
		"Calculate from LsPixelsPerWorldUnit*LsResolutionMultiplier",
		"Use Fixed Resolution"
	};
	for (unsigned i=0;i<7;i++)
	{
		if (!strcmp(propertyString.c_str(),propertyStrings[i]))
		{
			out = (PropertyEnum)i;
			return true;
		}
	}
	RRReporter::report(WARN,"Enum has unexpected value %s.\n",propertyString.c_str());
	return false;
};
#endif


////////////////////////////////////////////////////////////////////////////
//
// PerSceneSettings

//! In Toolbench, we read settings from LightsprintScene entity or keep defaults.
//! In .gsa, we make up some settings.
struct PerSceneSettings
{
	PropertyEnum lsDefaultBakeTarget;
	PropertyEnum lsDefaultBakeDirectionality;
	float lsPixelsPerWorldUnit;
	float lsEmissiveMultiplier;
	// and lsEnvironmentXxx, but we don't need it here

	PerSceneSettings()
	{
		lsDefaultBakeTarget = PE_VERTICES;
		lsDefaultBakeDirectionality = PE_NON_DIRECTIONAL;
		lsPixelsPerWorldUnit = 0.1f;
		lsEmissiveMultiplier = 1;
	}
#if GAMEBRYO_MAJOR_VERSION==3
	void readFrom(egf::Entity* entity)
	{
		efd::utf8string str;

		entity->GetPropertyValue("LsDefaultBakeTarget", str);
		getPropertyEnum(str,lsDefaultBakeTarget);

		entity->GetPropertyValue("LsDefaultBakeDirectionality", str);
		getPropertyEnum(str,lsDefaultBakeDirectionality);

		entity->GetPropertyValue("LsPixelsPerWorldUnit", lsPixelsPerWorldUnit);
		RR_CLAMP(lsPixelsPerWorldUnit,1e-6f,1e6f);

		entity->GetPropertyValue("LsEmissiveMultiplier", lsEmissiveMultiplier);
		RR_CLAMP(lsEmissiveMultiplier,0,1e6f);
	}
#endif
};


////////////////////////////////////////////////////////////////////////////
//
// PerEntitySettings

//! In Toolbench, we read settings from LightsprintMesh entity or keep defaults.
//! In .gsa, we keep defaults.
struct PerEntitySettings
{
	PropertyEnum lsBakeTarget;
	PropertyEnum lsBakeDirectionality;
	PropertyEnum lsResolutionMode;
	float lsResolutionMultiplier;
	unsigned lsResolutionFixedWidth;
	unsigned lsResolutionFixedHeight;
	float lsEmissiveMultiplier;

	PerEntitySettings()
	{
		lsBakeTarget = PE_INHERIT_FROM_LIGHTSPRINT_SCENE;
		lsBakeDirectionality = PE_INHERIT_FROM_LIGHTSPRINT_SCENE;
		lsResolutionMode = PE_RESOLUTION_CALCULATED;
		lsResolutionMultiplier = 1;
		lsResolutionFixedWidth = 128;
		lsResolutionFixedHeight = 128;
		lsEmissiveMultiplier = 1;
	}
#if GAMEBRYO_MAJOR_VERSION==3
	void readFrom(egf::Entity* entity)
	{
		efd::utf8string str;

		entity->GetPropertyValue("LsBakeTarget", str);
		getPropertyEnum(str,lsBakeTarget);

		entity->GetPropertyValue("LsBakeDirectionality", str);
		getPropertyEnum(str,lsBakeDirectionality);

		entity->GetPropertyValue("LsResolutionMode", str);
		getPropertyEnum(str,lsResolutionMode);

		entity->GetPropertyValue("LsResolutionMultiplier", lsResolutionMultiplier);
		RR_CLAMP(lsResolutionMultiplier,1e-6f,1e6f);

		entity->GetPropertyValue("LsResolutionFixedWidth", lsResolutionFixedWidth);
		RR_CLAMP(lsResolutionFixedWidth,1,4096);

		entity->GetPropertyValue("LsResolutionFixedHeight", lsResolutionFixedHeight);
		RR_CLAMP(lsResolutionFixedWidth,1,4096);

		entity->GetPropertyValue("LsEmissiveMultiplier", lsEmissiveMultiplier);
		RR_CLAMP(lsEmissiveMultiplier,0,1e6f);
	}
	void inheritFrom(const PerSceneSettings& perSceneSettings)
	{
		if (lsBakeTarget==PE_INHERIT_FROM_LIGHTSPRINT_SCENE)
			lsBakeTarget = perSceneSettings.lsDefaultBakeTarget;
		if (lsBakeDirectionality==PE_INHERIT_FROM_LIGHTSPRINT_SCENE)
			lsBakeDirectionality = perSceneSettings.lsDefaultBakeDirectionality;
		lsEmissiveMultiplier *= perSceneSettings.lsEmissiveMultiplier;
	}
#endif
};


////////////////////////////////////////////////////////////////////////////
//
// RRObjectGamebryo

class RRObjectGamebryo : public RRObjectGamebryoBase
{
public:
	// creates new RRObject from NiMesh
	// return NULL for meshes of unsupported formats
	static RRObjectGamebryo* create(NiScene* _pkEntityScene, NiMesh* _mesh, const PerEntitySettings& _perEntitySettings, RRObject::LodInfo _lodInfo, MaterialCacheGamebryo& _materialCache, bool& _aborting)
	{
		if (!_mesh)
		{
			return NULL;
		}
		if (_aborting)
		{
			return NULL;
		}
#if GAMEBRYO_MAJOR_VERSION==2
		if (!NiLightMapUtility::IsLightMapMesh(_mesh))
		{
			return NULL;
		}
#endif
		switch (_mesh->GetPrimitiveType())
		{
			case NiPrimitiveType::PRIMITIVE_TRIANGLES:
			case NiPrimitiveType::PRIMITIVE_TRISTRIPS:
				// this is supported
				break;
			case NiPrimitiveType::PRIMITIVE_QUADS:
				// this would need little bit of additional work
				RR_LIMITED_TIMES(1,RRReporter::report(WARN,"QUADS primitives are ignored, let us know if you need them.\n"));
				return NULL;
			default:
				// lines, points etc
				return NULL;
		}
		// do not call UpdateEffects, calling it and baking lightmaps switches several objects in 'Undead' scene to error shader
		//_mesh->UpdateEffects();

		// this is temporary local copy of settings
		// it is initialized to user requested settings, but we may modify it to match mesh capabilities
		PerEntitySettings perEntitySettings = _perEntitySettings;

#if GAMEBRYO_MAJOR_VERSION==2
		unsigned lightmapTexcoord = (unsigned)NiLightMapUtility::GetLightMapUVSetIndex(_mesh);
#else
		// querying lightmapTexcoord is more complicated in Gamebryo 3
		//  it can't be done later because RRMeshGamebryo() already needs it to lock uv buffer

		// as a subquest, we must decide between nondirectional and directional buffers (it affects lightmapTexcoord)
		bool buildPerVertex = _perEntitySettings.lsBakeTarget==PE_VERTICES;
		bool buildNonDirectional = _perEntitySettings.lsBakeDirectionality==PE_NON_DIRECTIONAL;

		// finally set lightmapTexcoord
		unsigned lightmapTexcoord = UINT_MAX; // any uv that does not exist = unwrap would be autogenerated
		NiGIDescriptorTable nigidt;
		const NiGIDescriptor* nigid = nigidt.GetGIDescriptor(_mesh);
		if (nigid)
		{
			if (buildNonDirectional)
			{
				lightmapTexcoord = NiGIDescriptor::GetUVSetIndex(_mesh, nigid->m_LightMapShaderSlot);
				if (buildPerVertex && !nigid->m_SupportsVertexLightMaps)
				{
					RRReporter::report(WARN,"Mesh %s doesn't support non-directional vertex bake.\n",(const efd::Char*)_mesh->GetName());
					perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
				}
				if (!buildPerVertex && !nigid->m_SupportsTextureLightMaps)
				{
					RRReporter::report(WARN,"Mesh %s doesn't support non-directional texture bake.\n",(const efd::Char*)_mesh->GetName());
					perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
				}
			}
			else
			{
				unsigned directionalTexcoord0 = NiGIDescriptor::GetUVSetIndex(_mesh, nigid->m_RNMShaderSlots[0]);
				unsigned directionalTexcoord1 = NiGIDescriptor::GetUVSetIndex(_mesh, nigid->m_RNMShaderSlots[1]);
				unsigned directionalTexcoord2 = NiGIDescriptor::GetUVSetIndex(_mesh, nigid->m_RNMShaderSlots[2]);
				lightmapTexcoord = directionalTexcoord0;
				if (directionalTexcoord0!=directionalTexcoord1 || directionalTexcoord0!=directionalTexcoord2 || directionalTexcoord1!=directionalTexcoord2)
				{
					RRReporter::report(WARN,"All three textures making single directional lightmap must use the same uv.\n");
				}
				if (buildPerVertex && !nigid->m_SupportsVertexRNMs)
				{
					RRReporter::report(WARN,"Mesh %s doesn't support directional vertex bake.\n",(const efd::Char*)_mesh->GetName());
					perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
				}
				if (!buildPerVertex && !nigid->m_SupportsTextureRNMs)
				{
					RRReporter::report(WARN,"Mesh %s doesn't support directional texture bake.\n",(const efd::Char*)_mesh->GetName());
					perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
				}
			}
		}
		else
		{
			// no GIDescriptor
			// a) keep it cast shadows
			//RRReporter::report(WARN,"Mesh %s doesn't have GIDescriptor.\n",(const efd::Char*)_mesh->GetName());
			//perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
			// b) remove it from scene
			return NULL;
		}

#endif

		RRMesh* mesh = new RRMeshGamebryo(_mesh, lightmapTexcoord);
		if (mesh->getNumTriangles()==0)
		{
			delete mesh;
			return NULL;
		}
		const RRCollider* collider = RRCollider::create(mesh, RRCollider::IT_LINEAR, _aborting);
		if (!collider)
		{
			delete mesh;
			return NULL;
		}
		RRObjectGamebryo* object = new RRObjectGamebryo(MaterialInputs(_mesh,perEntitySettings.lsEmissiveMultiplier), collider, _lodInfo, _materialCache);
		object->perEntitySettings = perEntitySettings;
		object->pkEntityScene = _pkEntityScene;
		return object;
	}
	virtual ~RRObjectGamebryo()
	{
		delete getCollider()->getMesh();
		delete getCollider();
	}

	virtual RRMaterial* getTriangleMaterial(unsigned t, const RRLight* light, const RRObject* receiver) const
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
#if GAMEBRYO_MAJOR_VERSION==2
			const GamebryoLightCache* gamebryoLightCache = (GamebryoLightCache*)light->customData;
			if (!gamebryoLightCache)
			{
				// This light was probably added manually, not adapted from Gamebryo.
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
#else // GAMEBRYO_MAJOR_VERSION!=2
			// Gamebryo 3.x entity has all shadows enabled/disabled at once,
			// so it's simpler, it illuminates all objects / casts all shadows.
			return material;
#endif // GAMEBRYO_MAJOR_VERSION!=2
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

	virtual void recommendLayerParameters(RRObject::LayerParameters& layerParameters) const
	{
#if GAMEBRYO_MAJOR_VERSION==2
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
			const RRObjectGamebryo* object;
		};

		// fill filename
		RR_SAFE_FREE(layerParameters.actualFilename);
		LightmapFunctor lightmapFunctor;
		lightmapFunctor.layerParameters = &layerParameters;
		lightmapFunctor.object = this;
		NiLightMapUtility::VisitLightMapMeshes(pkEntityScene,lightmapFunctor);

		// fill size, type, format
		int w,h;
		NiLightMapUtility::GetLightMapResolution(w,h,lightmapFunctor.object->mesh,layerParameters.suggestedPixelsPerWorldUnit,layerParameters.suggestedMinMapSize,layerParameters.suggestedMaxMapSize);

		layerParameters.actualWidth = w;
		layerParameters.actualHeight = h;
		layerParameters.actualType = BT_2D_TEXTURE;
		layerParameters.actualFormat = BF_RGB;
		layerParameters.actualScaled = true;
		layerParameters.actualBuildNonDirectional = true;
		layerParameters.actualBuildDirectional = false;
		layerParameters.actualBuildBentNormals = false;
#else
		if (perEntitySettings.lsBakeTarget==PE_TARGET_NONE)
		{
			layerParameters.actualBuildNonDirectional = false;
			layerParameters.actualBuildDirectional = false;
			layerParameters.actualBuildBentNormals = false;
		}
		else
		if (perEntitySettings.lsBakeTarget==PE_VERTICES)
		{
			layerParameters.actualType = BT_VERTEX_BUFFER;
			layerParameters.actualWidth = getCollider()->getMesh()->getNumVertices();
			layerParameters.actualHeight = 1;
			layerParameters.actualFormat = BF_RGBA;
			layerParameters.actualScaled = true;
			RR_SAFE_FREE(layerParameters.actualFilename);
			layerParameters.actualBuildNonDirectional = perEntitySettings.lsBakeDirectionality==PE_NON_DIRECTIONAL;
			layerParameters.actualBuildDirectional = !layerParameters.actualBuildNonDirectional;
			layerParameters.actualBuildBentNormals = false;
		}
		else
		if (perEntitySettings.lsResolutionMode==PE_RESOLUTION_CALCULATED)
		{
			// create matrix that scales from lightsprint local space to gamebryo world space
			RRMatrix3x4 worldMatrix = getWorldMatrixRef();
			for (unsigned i=0;i<3;i++)
				for (unsigned j=0;j<4;j++)
					worldMatrix.m[i][j] /= SCALE_GEOMETRY;

			// calculate density in gamebryo world space
			RRMesh* worldSpaceMesh = getCollider()->getMesh()->createTransformed(&worldMatrix);
			float density = worldSpaceMesh->getMappingDensity(CH_LIGHTMAP);
			delete worldSpaceMesh;

			// density -> resolution
			unsigned resolution = (unsigned)RR_MAX(1,density*perSceneSettings.lsPixelsPerWorldUnit*perEntitySettings.lsResolutionMultiplier+0.5f);
			//resolution = RR_CLAMPED(resolution,layerParameters.suggestedMinMapSize,layerParameters.suggestedMaxMapSize);
			unsigned resolutionPOT = RR_MAX(1,layerParameters.suggestedMinMapSize);
			while (resolutionPOT<resolution && resolutionPOT*2<=layerParameters.suggestedMaxMapSize) resolutionPOT *= 2;

			layerParameters.actualType = BT_2D_TEXTURE;
			layerParameters.actualWidth = resolutionPOT;
			layerParameters.actualHeight = resolutionPOT;
			layerParameters.actualFormat = (perEntitySettings.lsBakeTarget==PE_COMPRESSED_TEXTURE)?BF_DXT1:BF_RGB;
			layerParameters.actualScaled = true;
			RR_SAFE_FREE(layerParameters.actualFilename);
			layerParameters.actualBuildNonDirectional = perEntitySettings.lsBakeDirectionality==PE_NON_DIRECTIONAL;
			layerParameters.actualBuildDirectional = !layerParameters.actualBuildNonDirectional;
			layerParameters.actualBuildBentNormals = false;
		}
		else
		{
			layerParameters.actualType = BT_2D_TEXTURE;
			layerParameters.actualWidth = RR_CLAMPED(perEntitySettings.lsResolutionFixedWidth,layerParameters.suggestedMinMapSize,layerParameters.suggestedMaxMapSize);
			layerParameters.actualHeight = RR_CLAMPED(perEntitySettings.lsResolutionFixedHeight,layerParameters.suggestedMinMapSize,layerParameters.suggestedMaxMapSize);
			layerParameters.actualFormat = (perEntitySettings.lsBakeTarget==PE_COMPRESSED_TEXTURE)?BF_DXT1:BF_RGB;
			layerParameters.actualScaled = true;
			RR_SAFE_FREE(layerParameters.actualFilename);
			layerParameters.actualBuildNonDirectional = perEntitySettings.lsBakeDirectionality==PE_NON_DIRECTIONAL;
			layerParameters.actualBuildDirectional = !layerParameters.actualBuildNonDirectional;
			layerParameters.actualBuildBentNormals = false;
		}
#endif
	}

#if GAMEBRYO_MAJOR_VERSION==3
	void* getCustomData(const char* name) const
	{
		if (!strcmp(name,"egmGI::MeshProperties*"))
			return (void*)&meshProperties;
		return RRObject::getCustomData(name);
	}

	// public, so we can easily set it from MeshVisitor. we can't set in in constructor, because we don't have access to properties yet
	egmGI::MeshProperties meshProperties;
#endif

	// we store this only for later use by recommendLayerParameters()
	PerEntitySettings perEntitySettings;

private:
	RRObjectGamebryo(MaterialInputs _materialInputs, const RRCollider* _collider, RRObject::LodInfo _lodInfo, MaterialCacheGamebryo& _materialCache)
	{
		NIASSERT(_materialInputs.mesh);
		NIASSERT(_collider);
		mesh = _materialInputs.mesh;
		meshIndex = 0;
		setCollider(_collider);
		lodInfo = _lodInfo;
		setWorldMatrix(&convertMatrix(mesh->GetWorldTransform()));
		material = _materialCache.getMaterial(_materialInputs);
		faceGroups.push_back(FaceGroup(material,mesh->GetTotalPrimitiveCount()));
		name = mesh->GetName();
	}

	NiScene* pkEntityScene; // used only to query lightmap names from .gsa
	RRMaterial* material;
	LodInfo lodInfo;
};


////////////////////////////////////////////////////////////////////////////
//
// RRObjectsGamebryo

class RRObjectsGamebryo : public RRObjects
{
public:
	// path used by .gsa
	RRObjectsGamebryo(NiScene* _pkEntityScene, bool& _aborting, float _emissiveMultiplier)
	{
		// .gsa doesn't have entity settings, set defaults for .gsa
		PerEntitySettings perEntitySettings;
		perEntitySettings.lsBakeTarget = PE_TEXTURE;
		perEntitySettings.lsBakeDirectionality = PE_NON_DIRECTIONAL;
		perEntitySettings.lsEmissiveMultiplier = _emissiveMultiplier;

		RRObject::LodInfo lodInfo;
		lodInfo.base = 0; // start hierarchy traversal with base 0 marking we are not in LOD
		lodInfo.level = 0;
		unsigned uiCount = _pkEntityScene->GetEntityCount();
		for (unsigned int uiEntity=0; uiEntity < uiCount; uiEntity++)
		{
			NiEntityInterface* pkEntity = _pkEntityScene->GetEntityAt(uiEntity);
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
							addNode(_pkEntityScene,pkRoot,lodInfo,_aborting,perEntitySettings);
						}
					}
				}
			}
		}
	}
#if GAMEBRYO_MAJOR_VERSION==3
	// path used by Gamebryo 3.x Toolbench plugin
	RRObjectsGamebryo(efd::ServiceManager* serviceManager, bool& _aborting)
	{
		RRObject::LodInfo lodInfo;
		lodInfo.base = 0; // start hierarchy traversal with base 0 marking we are not in LOD
		lodInfo.level = 0;
		if (serviceManager)
		{
			egf::EntityManager* entityManager = serviceManager->GetSystemServiceAs<egf::EntityManager>();
			ecr::SceneGraphService* sceneGraphService = serviceManager->GetSystemServiceAs<ecr::SceneGraphService>();
			if (entityManager && sceneGraphService)
			{
				// read scene settings
				unsigned numScenes = 0;
				unsigned numLightsprintScenes = 0;
				unsigned numMeshes = 0;
				unsigned numLightsprintMeshes = 0;
				egf::Entity *entity = NULL;
				for (egf::EntityManager::EntityMap::const_iterator iterator = entityManager->GetFirstEntityPos(); entityManager->GetNextEntity(iterator, entity); )
				{
					if (entity)
					{
						if (entity->GetModel()->ContainsModel("Scene"))
						{
							numScenes++;
						}
						if (entity->GetModel()->ContainsModel("LightsprintScene"))
						{
							perSceneSettings.readFrom(entity);
							numLightsprintScenes++;
						}
						if (entity && entity->GetModel()->ContainsModel("Mesh"))
						{
							numMeshes++;
						}
						if (entity && entity->GetModel()->ContainsModel("LightsprintMesh"))
						{
							numLightsprintMeshes++;
						}
					}
				}
				if (!numLightsprintScenes)
					RRReporter::report(WARN,"No entity contains LightsprintScene model, baking will default to low quality per-vertex. Add LightsprintScene to change it.\n");
				if (numScenes>1)
					RRReporter::report(WARN,"%d entities contain Scene model, isn't one enough?\n",numScenes);
				if (!numLightsprintMeshes && numMeshes)
					RRReporter::report(WARN,"No entity contains LightsprintMesh model, consider using it. It's like Mesh, but with additional bake settings.\n");

				// read meshes
				for (egf::EntityManager::EntityMap::const_iterator iterator = entityManager->GetFirstEntityPos(); entityManager->GetNextEntity(iterator, entity); )
				{
					if (entity && entity->GetModel()->ContainsModel("Mesh"))
					{
						bool isStatic = true;
						// whole cathedral has IsStatic=false, it does not mark static meshes, let's ignore it.
						//entity->GetPropertyValue("IsStatic", isStatic);
						bool useForPrecomputedLighting = true;
						entity->GetPropertyValue("UseForPrecomputedLighting", useForPrecomputedLighting);
						if (isStatic && useForPrecomputedLighting)
						{
							PerEntitySettings perEntitySettings;
							if (entity->GetModel()->ContainsModel("LightsprintMesh"))
								perEntitySettings.readFrom(entity); // call only if LightsprintMesh exists, otherwise it emits warnings
							perEntitySettings.inheritFrom(perSceneSettings); // call always, converts default PE_INHERIT_... to nice values
							if (!entity->GetModel()->ContainsModel("PrecomputedLightReceiver"))
							{
								// this is not PCLMesh, use it as occluder but don't build lightmap for it
								perEntitySettings.lsBakeTarget = PE_TARGET_NONE;
							}

							// manually traverse subtree (necessary for LOD support) and create adapters
							NiAVObject* obj = sceneGraphService->GetSceneGraphFromEntity(entity->GetEntityID());
							addNode(NULL,obj,lodInfo,_aborting,perEntitySettings);

							// traverse again using Gamebryo's visitor (necessary for getting egmGI::MeshProperties)
							class Visitor : public egmGI::MeshVisitor
							{
							public:
								virtual bool VisitMesh(NiMesh* pkMesh,egmGI::MeshProperties kProps = egmGI::MeshProperties())
								{
									for (unsigned i=0;i<objects->size();i++)
									{
										if (((RRObjectGamebryo*)(*objects)[i])->mesh==pkMesh)
											((RRObjectGamebryo*)(*objects)[i])->meshProperties = kProps;
									}
									return true;
								}
								RRObjectsGamebryo* objects;
							};
							Visitor visitor;
							visitor.objects = this;
							if (NiIsKindOf(NiNode,obj))
								egmGI::MeshUtility::VisitMeshes((NiNode*)obj,&visitor,entity);
						}
					}
				}
			}
		}
		// Gamebryo 3.x gives us meshes in random order. We sort them to make scene identical between two runs,
		// so that cached colliders can be reused.
		sortObjects();
	}
	// Sorts elements (RRObject) in this vector (RRObjects).
	// Sorts by mesh geometry (positions), it is sufficiently unique, no need to mix in normals, uvs...
	void sortObjects()
	{
		struct SortElement
		{
			RRObject* object;
			RRHash hash;

			SortElement()
			{
				object=NULL;
			}
			static int sortByHashes(const void* ptr1, const void* ptr2)
			{
				const SortElement* elem1 = (const SortElement*)ptr1;
				const SortElement* elem2 = (const SortElement*)ptr2;
				return memcmp(elem1->hash.value,elem2->hash.value,sizeof(elem1->hash.value));
			}
		};
		unsigned numElements = size();
		SortElement* sortElement = new SortElement[numElements];
		for (unsigned i=0;i<numElements;i++)
		{
			sortElement[i].object = (*this)[i];
			// calculates hash from mesh transformed to world space
			// hashing in local space would produce identical hashes for mesh instances
			RRMesh* worldSpaceMesh = sortElement[i].object->getCollider()->getMesh()->createTransformed(sortElement[i].object->getWorldMatrix());
			sortElement[i].hash = worldSpaceMesh->getHash();
			delete worldSpaceMesh;
		}
		qsort(sortElement,numElements,sizeof(sortElement[0]),SortElement::sortByHashes);
		clear();
		for (unsigned i=0;i<numElements;i++)
			push_back(sortElement[i].object);
		delete[] sortElement;
	}
#endif
	virtual ~RRObjectsGamebryo()
	{
		for (unsigned int i=0; i<size(); i++)
		{
			delete (*this)[i];
		}
	}

private:
	// Adds all instances from node and his subnodes to 'objects'.
	// lodInfo.base==0 marks we are not in LOD
	void addNode(NiScene* pkEntityScene, const NiAVObject* object, RRObject::LodInfo lodInfo, bool& aborting, const PerEntitySettings& perEntitySettings)
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
				addNode(pkEntityScene,node->GetAt(i),lodInfo,aborting,perEntitySettings);
			}
		}
		// adapt single node
		if (NiIsExactKindOf(NiMesh,object) && !object->GetAppCulled())
		{
			NiMesh* niMesh = (NiMesh*)object;

			if (!lodInfo.base)
			{
				// this is not LOD, give it unique base and level 0
				// (only for this mesh, must not be propagated into children)
				lodInfo.base = object;
				lodInfo.level = 0;
			}
			RRObjectGamebryo* rrObject = RRObjectGamebryo::create(pkEntityScene,niMesh,perEntitySettings,lodInfo,materialCache,aborting);
			if (rrObject)
			{
				push_back(rrObject);
			}
		}
	}

	MaterialCacheGamebryo materialCache;
#if GAMEBRYO_MAJOR_VERSION==3
	PerSceneSettings perSceneSettings;
#endif
};


////////////////////////////////////////////////////////////////////////////
//
// RRLightsGamebryo

class RRLightsGamebryo : public RRLights
{
public:
#if GAMEBRYO_MAJOR_VERSION==2
	// path used by Gamebryo 2.6 .gsa
	RRLightsGamebryo(NiScene* scene)
	{
		class AddLightFunctor : public NiVisitLightMapLightFunctor
		{
		public:
			virtual bool operator() (NiLight* pkLight, NiLightMapLightProperties kProps = NiLightMapLightProperties())
			{
				lights->addLight(pkLight);
				return true;
			}
			RRLightsGamebryo* lights;
		};

		AddLightFunctor addLightFunctor;
		addLightFunctor.lights = this;
		NiLightMapUtility::VisitLightMapLights(scene,addLightFunctor);
	}
#else

	// path used by Gamebryo 3.x .gsa
	// note: We still support Gamebryo 3.x .gsa files, from loading .gsa to saving individual lightmaps to disk,
	//       if you need it. However, Emergent replaced .gsa by new format, therefore it is
	//       recommended to leave .gsa and start using Toolbench.
	RRLightsGamebryo(NiScene* pkEntityScene)
	{
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
						addNode(pkRoot);
					}
				}
			}
		}
	}

	// path used by Gamebryo 3.x .gsa
	void addNode(const NiAVObject* object)
	{
		if (!object)
		{
			return;
		}
		// recurse into children
		if (NiIsKindOf(NiNode,object))
		{
			NiNode* node = (NiNode*)object;
			for (unsigned int i=0; i<node->GetArrayCount(); i++)
			{
				addNode(node->GetAt(i));
			}
		}
		// adapt single node
		if (NiIsKindOf(NiLight,object))
		{
			addLight((NiLight*)object);
		}
	}

	// path used by Gamebryo 3.x Toolbench plugin
	RRLightsGamebryo(efd::ServiceManager* serviceManager)
	{
		if (serviceManager)
		{
			egf::EntityManager* entityManager = serviceManager->GetSystemServiceAs<egf::EntityManager>();
			if (entityManager)
			{
				egf::Entity *entity = NULL;
				for (egf::EntityManager::EntityMap::const_iterator iterator = entityManager->GetFirstEntityPos(); entityManager->GetNextEntity(iterator, entity); )
				{
					if (entity && entity->GetModel()->ContainsModel("Light"))
					{
						addLight(entity);
					}
				}
			}
		}
	}

	// path used by Gamebryo 3.x Toolbench plugin
	void addLight(egf::Entity* entity)
	{
		RR_ASSERT(entity);
		bool isStatic = true;
		//entity->GetPropertyValue("IsStatic", isStatic); it is not set in meshes, better ignore it in lights too
		bool useForPrecomputedLighting = true;
		entity->GetPropertyValue("UseForPrecomputedLighting", useForPrecomputedLighting);
		bool isVisible = true;
		entity->GetPropertyValue("IsVisible", isVisible);
		if (isStatic && useForPrecomputedLighting && isVisible)
		{
			efd::Color diffuseColor(1,1,1);
			entity->GetPropertyValue("DiffuseColor", diffuseColor);
			float dimmer = 1;
			entity->GetPropertyValue("Dimmer", dimmer);
			efd::Point3 position(0,0,0);
			entity->GetPropertyValue("Position", position);
			efd::Point3 rotation(0,0,0);
			entity->GetPropertyValue("Rotation", rotation);
			float constantAttenuation = 1;
			entity->GetPropertyValue("ConstantAttenuation", constantAttenuation);
			float linearAttenuation = 0;
			entity->GetPropertyValue("LinearAttenuation", linearAttenuation);
			float quadraticAttenuation = 0;
			entity->GetPropertyValue("QuadraticAttenuation", quadraticAttenuation);

			RRVec3 pos = convertWorldPos(position) * SCALE_GEOMETRY;
			RRVec3 dir = convertWorldRotToDir(rotation);
			RRVec3 color = convertColor(diffuseColor) * dimmer;
			RRVec4 poly(constantAttenuation,linearAttenuation/SCALE_GEOMETRY,quadraticAttenuation/SCALE_GEOMETRY/SCALE_GEOMETRY,1);

			if (color!=RRVec3(0)) // don't adapt lights that have no effect (makes calculation bit faster)
			{
				RRLight* rrLight = NULL;
				
				// spot light
				if (entity->GetModel()->ContainsModel("SpotLight"))
				{
					float innerSpotAngle = 0.5f;
					entity->GetPropertyValue("InnerSpotAngle", innerSpotAngle);
					float outerSpotAngle = 1;
					entity->GetPropertyValue("OuterSpotAngle", outerSpotAngle);
					float spotExponent = 1;
					entity->GetPropertyValue("SpotExponent", spotExponent);

					rrLight = RRLight::createSpotLightPoly(pos, color, poly, dir, RR_DEG2RAD(outerSpotAngle), RR_DEG2RAD(outerSpotAngle-innerSpotAngle), spotExponent);
				}
			
				// point light
				// (note that PointLight is contained also in spotlight, so we must catch spotlights in previous if and not let them here)
				else if (entity->GetModel()->ContainsModel("PointLight"))
				{
					rrLight = RRLight::createPointLightPoly(pos, color, poly);
				}

				// directional light
				else if (entity->GetModel()->ContainsModel("DirectionalLight"))
				{
					rrLight = RRLight::createDirectionalLight(dir, color, false);
				}

				// common light properties
				if (rrLight)
				{
					efd::utf8string name;
					entity->GetPropertyValue("Name",name); //!!! returns ""
					if (name.empty())
						name = entity->GetModelName();
					rrLight->name = name.c_str();
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
					rrLight->customData = entity;
					rrLight->castShadows = true;
					entity->GetPropertyValue("CastShadows", rrLight->castShadows);
#else
					rrLight->castShadows = true;
#endif
					push_back(rrLight);
				}
			}
		}
	}
#endif // GAMEBRYO_MAJOR_VERSION==3

	// path used by Gamebryo 2/3 .gsa
	void addLight(NiLight* light)
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
				RRVec4 poly(pointLight->GetConstantAttenuation(),pointLight->GetLinearAttenuation()/SCALE_GEOMETRY,pointLight->GetQuadraticAttenuation()/SCALE_GEOMETRY/SCALE_GEOMETRY,1);

				rrLight = RRLight::createPointLightPoly(pos, color, poly);
			}

			// spot light
			else if (NiIsExactKindOf(NiSpotLight, light))
			{
				NiSpotLight* spotLight = (NiSpotLight*) light;

				RRVec3 pos = convertWorldPos(spotLight->GetWorldLocation()) * SCALE_GEOMETRY;
				RRVec3 dir = convertWorldDir(spotLight->GetWorldDirection());
				RRVec3 color = convertColor(spotLight->GetDiffuseColor()) * spotLight->GetDimmer();
				RRVec4 poly(spotLight->GetConstantAttenuation(),spotLight->GetLinearAttenuation()/SCALE_GEOMETRY,spotLight->GetQuadraticAttenuation()/SCALE_GEOMETRY/SCALE_GEOMETRY,1);
				float innerAngle = 0.0174533f * spotLight->GetInnerSpotAngle();
				float outerAngle = 0.0174533f * spotLight->GetSpotAngle();
				float spotExponent = spotLight->GetSpotExponent();

				rrLight = RRLight::createSpotLightPoly(pos, color, poly, dir, outerAngle, outerAngle-innerAngle, spotExponent);
			}

			// common light properties
			if (rrLight)
			{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
#if GAMEBRYO_MAJOR_VERSION==2
				rrLight->customData = new GamebryoLightCache(light);
#else
				rrLight->customData = NULL; // customData in Gamebryo 3.x hold Entity*, it's not available in .gsa
#endif
				NiShadowGenerator* shadowGenerator = light->GetShadowGenerator();
				rrLight->castShadows = shadowGenerator && shadowGenerator->GetActive();
#else
				rrLight->castShadows = true;
#endif
				rrLight->name = light->GetName(); //!!! returns ""
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
	//!  True to treat Gamebryo as completely uninitialized (and call NiInit/NiShutdown).
	//!  Set true when importing .gsa into generic Lightsprint samples, false in Toolbench plugin.
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	RRSceneGamebryo(const char* filename, bool initGamebryo, bool& aborting, float emissiveMultiplier = 1);
#if GAMEBRYO_MAJOR_VERSION==3
	//! Imports scene from toolbench.
	RRSceneGamebryo(efd::ServiceManager* serviceManager, bool& aborting);
#endif
	virtual ~RRSceneGamebryo();

	//! Loader suitable for RRScene::registerLoader().
	static RRScene* load(const char* filename, float scale, bool* aborting, float emissiveMultiplier)
	{
		bool not_aborting = false;
		return new RRSceneGamebryo(filename,true,aborting ? *aborting : not_aborting,emissiveMultiplier);
	}

protected:
	void gamebryoInit();
	void gamebryoShutdown();
	void updateCastersReceiversCache();

	bool                      initGamebryo;
	NiScene*                  pkEntityScene; // used only in .gsa
	HWND                      localHWND; // locally created window
	NiDX9Renderer*            localRenderer; // locally created renderer
};

RRSceneGamebryo::RRSceneGamebryo(const char* _filename, bool _initGamebryo, bool& _aborting, float _emissiveMultiplier)
{
	//RRReportInterval report(INF1,"Loading scene %s...\n",_filename); already reported one level up
	initGamebryo = _initGamebryo;
	pkEntityScene = NULL;

	gamebryoInit();

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
	protectedLights = adaptLightsFromGamebryo(pkEntityScene);

	// adapt meshes
	protectedObjects = adaptObjectsFromGamebryo(pkEntityScene,_aborting,_emissiveMultiplier);

	updateCastersReceiversCache();

	if ((!protectedObjects || !protectedObjects->size()) && !protectedLights)
	{
		RRReporter::report(WARN,"Scene %s empty.\n",_filename);
	}
}

#if GAMEBRYO_MAJOR_VERSION==3
RRSceneGamebryo::RRSceneGamebryo(efd::ServiceManager* serviceManager, bool& _aborting)
{
	initGamebryo = false;
	pkEntityScene = NULL;

	gamebryoInit();

	// adapt lights
	protectedLights = adaptLightsFromGamebryo(serviceManager);

	// adapt meshes
	protectedObjects = adaptObjectsFromGamebryo(serviceManager,_aborting);

	// adapt environment
	environment = adaptEnvironmentFromGamebryo(serviceManager);

	updateCastersReceiversCache();
}
#endif

void RRSceneGamebryo::gamebryoInit()
{
	// Init Gamebryo if necessary
	if (initGamebryo)
	{
		RRReporter::report(INF2,"Gamebryo init\n");
		NiInit();
		NiTexture::SetMipmapByDefault(true);
		NiSourceTexture::SetUseMipmapping(true);
	}
	
	// Create renderer if necessary, NiMaterialDetector will need it
	if (NiDX9Renderer::GetRenderer())
	{
		localHWND = 0;
		localRenderer = NULL;
	}
	else
	{
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
		localHWND = CreateWindow("a", "a", dwStyle, 0, 0, rect.right-rect.left, rect.bottom-rect.top, NULL, NULL, NULL, NULL);

		localRenderer = NiDX9Renderer::Create(0, 0, 0, localHWND, NULL);
		NiShadowManager::Initialize();
	}

	// Init the rest
	NiMaterialDetector::Init();
	NIASSERT(NiDataStream::GetFactory());
	NiDataStream::GetFactory()->SetCallback(NiDataStreamFactory::ForceCPUReadAccessCallback);
	NiImageConverter::SetImageConverter(NiNew NiDevImageConverter);
}

void RRSceneGamebryo::gamebryoShutdown()
{
	NiMaterialDetector::Shutdown();
	if (localRenderer)
	{
		NiShadowManager::Shutdown();
		delete localRenderer;
		DestroyWindow(localHWND);
	}
	if (initGamebryo)
	{
		RRReporter::report(INF2,"Gamebryo shutdown\n");
		NiShutdown();
	}
}

RRSceneGamebryo::~RRSceneGamebryo()
{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
#if GAMEBRYO_MAJOR_VERSION==2
	for (unsigned i=0;protectedLights && i<protectedLights->size();i++)
	{
		delete (GamebryoLightCache*)((*protectedLights)[i]->customData);
	}
#endif
#endif
	if (pkEntityScene) pkEntityScene->DecRefCount();

	gamebryoShutdown();
}

void RRSceneGamebryo::updateCastersReceiversCache()
{
#ifdef SUPPORT_DISABLED_LIGHTING_SHADOWING
#if GAMEBRYO_MAJOR_VERSION==2
	if (protectedObjects && protectedLights)
	{
		// Reindex meshes.
		for (unsigned i=0;i<protectedObjects->size();i++)
		{
			((RRObjectGamebryo*)((*protectedObjects)[i]))->meshIndex = i;
		}
		// Update cache in lights.
		for (unsigned i=0;i<protectedLights->size();i++)
		{
			GamebryoLightCache* cache = (GamebryoLightCache*)((*protectedLights)[i]->customData);
			if (cache)
				cache->update(*protectedObjects);
		}
	}
#endif
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

#if GAMEBRYO_MAJOR_VERSION==3
RRObjects* adaptObjectsFromGamebryo(efd::ServiceManager* serviceManager, bool& aborting)
{
	return new RRObjectsGamebryo(serviceManager,aborting);
}

RRLights* adaptLightsFromGamebryo(efd::ServiceManager* serviceManager)
{
	return new RRLightsGamebryo(serviceManager);
}

RRBuffer* adaptEnvironmentFromGamebryo(class efd::ServiceManager* serviceManager)
{
	RRBuffer* environment = NULL;
	if (serviceManager)
	{
		egf::EntityManager* entityManager = serviceManager->GetSystemServiceAs<egf::EntityManager>();
		if (entityManager)
		{
			egf::Entity *entity = NULL;
			for (egf::EntityManager::EntityMap::const_iterator iterator = entityManager->GetFirstEntityPos(); entityManager->GetNextEntity(iterator, entity); )
			{
				if (entity && entity->GetModel()->ContainsModel("LightsprintScene"))
				{
					efd::utf8string lsEnvironmentTexture;
					entity->GetPropertyValue("LsEnvironmentTexture", lsEnvironmentTexture);

					efd::Color lsEnvironmentColorTmp(0,0,0);
					entity->GetPropertyValue("LsEnvironmentColor", lsEnvironmentColorTmp);
					RRVec3 lsEnvironmentColor = convertColor(lsEnvironmentColorTmp);

					float lsEnvironmentMultiplier = 1;
					entity->GetPropertyValue("LsEnvironmentMultiplier", lsEnvironmentMultiplier);

					if (lsEnvironmentMultiplier==0)
					{
						// multiplier 0 -> keep environment NULL, black
					}
					else
					if (lsEnvironmentColor==RRVec3(0))
					{
						// color 0 -> ignore color, return texture * multiplier
						if (lsEnvironmentTexture!="")
							environment = RRBuffer::loadCube(lsEnvironmentTexture.c_str());
						if (environment && lsEnvironmentMultiplier!=1)
						{
							environment->setFormatFloats();
							environment->multiplyAdd(RRVec4(RRVec3(lsEnvironmentMultiplier),0),RRVec4(0));
						}
					}
					else
					{
						// texture * color * multiplier
						if (lsEnvironmentTexture!="")
							environment = RRBuffer::loadCube(lsEnvironmentTexture.c_str());
						if (!environment)
							environment = RRBuffer::createSky();
						if (lsEnvironmentColor*lsEnvironmentMultiplier!=RRVec3(1))
							environment->setFormatFloats();
						environment->multiplyAdd(RRVec4(lsEnvironmentColor*lsEnvironmentMultiplier,0),RRVec4(0));
					}
				}
			}
		}
	}
	return environment;
}

RRScene* adaptSceneFromGamebryo(efd::ServiceManager* serviceManager, bool& aborting)
{
	return new RRSceneGamebryo(serviceManager,aborting);
}
#endif

void registerLoaderGamebryo()
{
	RRScene::registerLoader("*.gsa",RRSceneGamebryo::load);
}

#endif // SUPPORT_GAMEBRYO
