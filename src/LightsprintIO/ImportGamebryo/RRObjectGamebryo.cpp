// --------------------------------------------------------------------------
// Lightsprint adapters for accesing Gamebryo scene.
// Copyright (C) Stepan Hrbek, Lightsprint, 2008, All rights reserved
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_GSA

// TODO
// - test LODs (implemented but not tested)
// - support FLOAT16_n

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

// commandline
#include <windows.h>
#include <NiCommand.h>
#pragma comment(lib,"NiApplication")
#pragma comment(lib,"NiInput")

// cache
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
			add->setElement(i,RRVec4(
				RRVec3(a-s),
				a[3]));
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
// RRMesh

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
					diffuseTexcoord = pkGlowMap->GetTextureIndex();
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

	//! uv is in/out, function converts it from coordinate in triangle to coordinate in material.
	void convertTriangleUvToMaterialUv(unsigned t, RRVec2& uv) const
	{
		Channel channel = hasTexcoord[CH_EMISSIVE] ? CH_EMISSIVE : CH_DIFFUSE;
		TriangleMapping triangleMapping;
		if (getTriangleMapping(t,triangleMapping,channel))
		{
			uv = triangleMapping.uv[0]*(1-uv[0]-uv[1]) + triangleMapping.uv[1]*uv[0] + triangleMapping.uv[2]*uv[1];
		}
	}


	NiMesh* mesh;
	//! Unique mesh index, used by GamebryoLightCache
private:
	unsigned numVertices;
	unsigned numTriangles;

	// locks and iterators created in constructor and used in getXxx() functions
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
// RRObject

class RRObjectGamebryo : public RRObjectGamebryoBase
{
public:
	// creates new RRObject from NiMesh
	// return NULL for meshes of unsupported formats
	static RRObjectGamebryo* create(NiMesh* _mesh, RRObject::LodInfo _lodInfo, bool& _aborting)
	{
		if (!_mesh)
		{
			return NULL;
		}
		if (_aborting)
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
		return new RRObjectGamebryo(_mesh, collider, _lodInfo);
	}
	virtual ~RRObjectGamebryo()
	{
		delete material.diffuseReflectance.texture;
		delete material.specularReflectance.texture;
		delete material.specularTransmittance.texture;
		delete material.diffuseEmittance.texture;
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
				return &material;
			}
			if (receiver)
			{
				// Does this mesh cast shadows from light to receiver?
				if (gamebryoLightCache->areCasterAndReceiver(this,(RRObjectGamebryo*)receiver))
				{
					// Yes, this mesh casts shadow from light to receiver.
					return &material;
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
						return &material;
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
			return &material;
		}
	}

	virtual void getTriangleLod(unsigned t, RRObject::LodInfo& out) const
	{
		out = lodInfo;
	}

private:
	RRObjectGamebryo(NiMesh* _mesh, const RRCollider* _collider, RRObject::LodInfo _lodInfo)
	{
		NIASSERT(_mesh);
		NIASSERT(_collider);
		mesh = _mesh;
		meshIndex = 0;
		collider = _collider;
		lodInfo = _lodInfo;
		worldMatrix = convertMatrix(mesh->GetWorldTransform());
		illumination = new RRObjectIllumination(collider->getMesh()->getNumVertices());

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
			RRScaler* scaler = RRScaler::createFastRgbScaler();
			material.updateColorsFromTextures(scaler,RRMaterial::UTA_DELETE);
			delete scaler;
			// optional - corrects invalid properties
			//material.validate();
		}
		material.updateSideBitsFromColors();
	}

	const RRCollider* collider;
	RRObjectIllumination* illumination;
	RRMatrix3x4 worldMatrix;
	RRMaterial material;
	LodInfo lodInfo;
};


////////////////////////////////////////////////////////////////////////////
//
// RRObjects

class RRObjectsGamebryo : public RRObjects
{
public:
	RRObjectsGamebryo(NiScene* _pkEntityScene, bool& _aborting)
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

	unsigned createLayer(int layerNumber, const RRIlluminatedObject::LayerParameters& params) const
	{
		unsigned created = 0;
		if (layerNumber>=0)
		{
			for (unsigned i=0;i<size();i++)
			{
				unsigned numVertices = (*this)[i].object->getCollider()->getMesh()->getNumVertices();
				NiMesh* niMesh = ((RRObjectGamebryo*)(*this)[i].object)->mesh;
				if (numVertices && !(*this)[i].illumination->getLayer(layerNumber) && NiLightMapUtility::IsLightMapMesh(niMesh))
				{
					int w,h;
					NiLightMapUtility::GetLightMapResolution(w,h,niMesh,params.pixelsPerWorldUnit,params.mapSizeMin,params.mapSizeMax);
					(*this)[i].illumination->getLayer(layerNumber) =
						w*h
						?
						// allocate lightmap for selected object
						RRBuffer::create(BT_2D_TEXTURE,w,h,1,BF_RGBA,true,NULL)
						:
						// allocate vertex buffers for other objects
						rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,params.format,params.scaled,NULL);
					created++;
				}
			}
		}
		return created;
	}

	virtual unsigned loadLayer(int layerNumber, const char* path, const char* ext) const
	{
		rr::RRReporter::report(rr::WARN,"Gamebryo lightmap load not yet implemented.\n");
		return 0;
	}

	virtual unsigned saveLayer(int layerNumber, const char* path, const char* ext) const
	{
		class SaveLightmapFunctor : public NiVisitLightMapMeshFunctor
		{
		public:
			virtual bool operator() (NiMesh* pkMesh, NiLightMapMeshProperties kProps)
			{
				for (unsigned i=0;objects && i<objects->size();i++)
				{
					RRObjectGamebryo* object = (RRObjectGamebryo*)(*objects)[i].object;
					if (pkMesh==object->mesh)
					{
						NiString kDirectoryName = NiString(path) + "/" + kProps.m_pcEntityDirectory;
						if (!NiFile::DirectoryExists(kDirectoryName))
						{
							NiFile::CreateDirectoryRecursive(kDirectoryName);
							if (!NiFile::DirectoryExists(kDirectoryName))
							{
								rr::RRReporter::report(rr::WARN,"Light map directory \"%s\" cannot be created for file \"%s\".", kDirectoryName, kProps.m_pcLightMapFilename);
							}
						}
						if ((*objects)[i].illumination->getLayer(layerNumber)->save(kDirectoryName + "/" + kProps.m_pcLightMapFilename + "." + ext))
						{
							saved++;
						}
					}
				}
				return true;
			}
			const RRObjects* objects;
			int layerNumber;
			unsigned saved;
			const char* path;
			const char* ext;
		};

		SaveLightmapFunctor saveLightmap;
		saveLightmap.saved = 0;
		if (layerNumber>=0)
		{
			if (pkEntityScene)
			{
				saveLightmap.objects = this;
				saveLightmap.path = path;
				saveLightmap.ext = ext;
				saveLightmap.layerNumber = layerNumber;
				NiLightMapUtility::VisitLightMapMeshes(pkEntityScene,saveLightmap);
			}
			rr::RRReporter::report(rr::INF1,"Saved %d lightmaps.\n",saveLightmap.saved);
		}
		return saveLightmap.saved;
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
			RRObjectGamebryo* rrObject = RRObjectGamebryo::create((NiMesh*)object,lodInfo,aborting);
			if (rrObject)
			{
				push_back(RRIlluminatedObject(rrObject, rrObject->getIllumination()));
			}
		}
	}

	NiScene* pkEntityScene; // used only to query lightmap names
};


////////////////////////////////////////////////////////////////////////////
//
// RRLights

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
// main low level interface

RRObjects* adaptObjectsFromGamebryo(NiScene* scene, bool& aborting)
{
	return new RRObjectsGamebryo(scene,aborting);
}

RRLights* adaptLightsFromGamebryo(NiScene* scene)
{
	return new RRLightsGamebryo(scene);
}


////////////////////////////////////////////////////////////////////////////
//
// import .gsa from disk - main high level interface

ImportSceneGamebryo::ImportSceneGamebryo(const char* _filename, bool _initGamebryo, bool& _aborting)
{
	RRReportInterval report(INF1,"Loading scene %s...\n",_filename);
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
	objects = adaptObjectsFromGamebryo(pkEntityScene,_aborting);

	updateCastersReceiversCache();

	if ((!objects || !objects->size()) && !lights)
	{
		RRReporter::report(WARN,"Scene %s empty.\n",_filename);
	}
}

ImportSceneGamebryo::~ImportSceneGamebryo()
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

void ImportSceneGamebryo::updateCastersReceiversCache()
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

#endif // SUPPORT_GSA
