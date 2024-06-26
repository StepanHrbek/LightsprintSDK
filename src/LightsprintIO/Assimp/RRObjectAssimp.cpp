// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for Assimp formats.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_ASSIMP

#include "RRObjectAssimp.h"
#include "Lightsprint/RRScene.h"
#include "assimp/cimport.h"
#include "assimp/cexport.h"
#include "assimp/config.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/material.inl"
#include "assimp/revision.h"
#include <filesystem>

//#pragma comment(lib,VER_ORIGINAL_FILENAME_STR)           // this macro has dll instead of lib
//#pragma comment(lib,"assimp-" RR_LIB_COMPILER "-mt.lib") // this macro has v143 instead of vc143
#pragma comment(lib,"assimp-vc143-mtd.lib")                 // too specific

namespace bf = std::filesystem;

using namespace rr;

// Assimp limitations
// - only one material per mesh, explodes multimaterial meshes
// - doesn't specify how to interpret opacity, may be inverted for some formats
// - leaks memory
// - increased memory footprint (two instances of the same mesh are usually loaded as two meshes)

// Lightsprint limitations
// - doesn't read all material properties


////////////////////////////////////////////////////////////////////////////
//
// utility functions

static RRVec2 convertUv(const aiVector3D& v)
{
	return RRVec2(v.x, v.y);
}

static RRVec3 convertPos(const aiVector3D& v)
{
	return RRVec3(v.x, v.y, v.z);
}

static RRVec3 convertDir(const aiVector3D& v)
{
	return RRVec3(v.x, v.y, v.z);
}

static RRVec3 convertColor(const aiColor3D& c)
{
	return RRVec3(c.r, c.g, c.b);
}

static RRString convertStr(const aiString& s)
{
	return s.data;
}

static RRMatrix3x4 convertMatrix(const aiMatrix4x4& m)
{
	return RRMatrix3x4(m[0][0],m[0][1],m[0][2],m[0][3], m[1][0],m[1][1],m[1][2],m[1][3], m[2][0],m[2][1],m[2][2],m[2][3]);
}

static RRCamera convertCamera(const aiCamera& c)
{
	RRCamera camera(convertPos(c.mPosition),RRVec3(0),c.mAspect?c.mAspect:1,90,c.mClipPlaneNear,c.mClipPlaneFar);
	camera.setDirection(convertDir(c.mLookAt));
	return camera;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRObjectsAssimp

class RRObjectsAssimp : public RRObjects
{
public:
	RRObjectsAssimp(const aiScene* _scene, const RRFileLocator* _textureLocator)
	{
		textureLocator = _textureLocator;
		materials = nullptr;
		numMeshes = 0;

		if (!_scene)
			return;

		// adapt materials
		materials = new RRMaterial[_scene->mNumMaterials];
		for (unsigned m=0;m<_scene->mNumMaterials;m++)
		{
			aiMaterial* aimaterial = _scene->mMaterials[m];
			RRMaterial& material = materials[m];

			// sideBits
			bool unknownNumberOfSides;
			{
				int twoSided = 0;
				unknownNumberOfSides = aimaterial->Get(AI_MATKEY_TWOSIDED,twoSided)!=aiReturn_SUCCESS;
				material.reset(twoSided!=0);
			}

			// name
			{
				aiString str;
				aimaterial->Get(AI_MATKEY_NAME,str);
				material.name = convertStr(str);
			}

			// shininess
			aimaterial->Get(AI_MATKEY_SHININESS_STRENGTH,material.specularShininess);

			// model (needs shininess)
			aiShadingMode model = aiShadingMode_Phong;
			aimaterial->Get(AI_MATKEY_SHADING_MODEL,model);
			material.specularModel = (model==aiShadingMode_Blinn)
				? ((material.specularShininess<=1)?RRMaterial::BLINN_TORRANCE_SPARROW:RRMaterial::BLINN_PHONG)
				: RRMaterial::PHONG;

			// diffuseReflectance
			if (model==aiShadingMode_NoShading)
				material.diffuseReflectance.color = RRVec3(0);
			else
				convertMaterialProperty(aimaterial,aiTextureType_DIFFUSE,AI_MATKEY_COLOR_DIFFUSE,material,material.diffuseReflectance);

			// specularReflectance
			if (model!=aiShadingMode_NoShading)
			{
				RRVec3 specular(0);
				{
					aiColor3D specularColor;
					bool specularColorSet = aimaterial->Get(AI_MATKEY_COLOR_SPECULAR,specularColor)==AI_SUCCESS;
					float shininessStrength;
					bool shininessStrengthSet = aimaterial->Get(AI_MATKEY_SHININESS_STRENGTH,shininessStrength)==AI_SUCCESS;
					if (specularColorSet && shininessStrengthSet) specular = convertColor(specularColor)*shininessStrength; else
					if (specularColorSet && !shininessStrengthSet) specular = convertColor(specularColor); else
					if (!specularColorSet && shininessStrengthSet) specular = RRVec3(shininessStrength);
				}
				RRVec3 reflective(0);
				{
					aiColor3D reflectiveColor;
					bool reflectiveColorSet = aimaterial->Get(AI_MATKEY_COLOR_REFLECTIVE,reflectiveColor)==AI_SUCCESS;
					float reflectivity;
					bool reflectivitySet = aimaterial->Get(AI_MATKEY_REFLECTIVITY,reflectivity)==AI_SUCCESS;
					if (reflectiveColorSet && reflectivitySet) reflective = convertColor(reflectiveColor)*reflectivity; else
					if (reflectiveColorSet && !reflectivitySet) reflective = convertColor(reflectiveColor); else
					if (!reflectiveColorSet && reflectivitySet) reflective = RRVec3(reflectivity);
				}
				material.specularReflectance.color = specular+reflective;
				if (reflective.sum()>specular.sum())
				{
					// mirroring
					material.specularModel = RRMaterial::PHONG;
					material.specularShininess = 100000;
				}
			}

			// diffuseEmittance
			convertMaterialProperty(aimaterial,aiTextureType_EMISSIVE,AI_MATKEY_COLOR_EMISSIVE,material,material.diffuseEmittance);

			// specularTransmittance
			{
				convertMaterialProperty(aimaterial,aiTextureType_OPACITY,AI_MATKEY_COLOR_TRANSPARENT,material,material.specularTransmittance);
				aiColor3D transparentColor;
				bool transparentColorSet = aimaterial->Get(AI_MATKEY_COLOR_TRANSPARENT,transparentColor)==AI_SUCCESS;
				float opacity;
				bool opacitySet = aimaterial->Get(AI_MATKEY_OPACITY,opacity)==AI_SUCCESS;
				if (transparentColorSet && opacitySet) material.specularTransmittance.color = convertColor(transparentColor)*(1-opacity); else
				if (transparentColorSet && !opacitySet) material.specularTransmittance.color = convertColor(transparentColor); else
				if (!transparentColorSet && opacitySet) material.specularTransmittance.color = RRVec3(1-opacity); else
				if (!transparentColorSet && !opacitySet) material.specularTransmittance.color = RRVec3(0);
				if (material.diffuseReflectance.texture && !material.specularTransmittance.texture && material.diffuseReflectance.texture->getFormat()==BF_RGBA)
				{
					RRReporter::report(INF2,"Using transparency from diffuse map.\n");
					material.specularTransmittance.texture = material.diffuseReflectance.texture->createReference();
					material.specularTransmittance.texcoord = material.diffuseReflectance.texcoord;
				}
				material.specularTransmittanceInAlpha = material.specularTransmittance.texture && (material.specularTransmittance.texture->getFormat()==BF_RGBA || material.specularTransmittance.texture->getFormat()==BF_RGBAF);
			}

			// refractionIndex
			aimaterial->Get(AI_MATKEY_REFRACTI,material.refractionIndex);

			// bumpMap
			convertMaterialProperty(aimaterial,aiTextureType_NORMALS,nullptr,0,0,material,material.bumpMap);
			if (!material.bumpMap.texture)
				convertMaterialProperty(aimaterial,aiTextureType_HEIGHT,nullptr,0,0,material,material.bumpMap);

			// lightmap
			convertMaterialProperty(aimaterial,aiTextureType_LIGHTMAP,nullptr,0,0,material,material.lightmap);
			if (!material.lightmap.texture)
				convertMaterialProperty(aimaterial,aiTextureType_AMBIENT,nullptr,0,0,material,material.lightmap);

			// get average colors from textures
			{
				RRColorSpace* colorSpace = RRColorSpace::create_sRGB();
				material.updateColorsFromTextures(colorSpace,RRMaterial::UTA_NULL,true);
				delete colorSpace;
			}

			// final decision on number of sides
			if (unknownNumberOfSides && material.specularTransmittance.color!=RRVec3(0))
			{
				// turn transparent 1-sided into 2-sided, it could be closer to what user expects (e.g. sanmiguel.obj leaves clearly need 2-sided, while walls are fine with any setting, but 1-sided walls need less fillrate)
				material.sideBits[1].renderFrom = 1;
				material.sideBits[1].receiveFrom = 1;
				material.sideBits[1].reflect = 1;
			}

			// autodetect keying
			material.updateKeyingFromTransmittance();

			// optimize material flags
			material.updateSideBitsFromColors();

			// autodetect bump map type
			material.updateBumpMapType();
		}

		// adapt meshes
		numMeshes = _scene->mNumMeshes;
		meshes = new RRMeshArrays[numMeshes];
		RRVector<unsigned> texcoords;
		for (unsigned m=0;m<numMeshes;m++)
		{
			aiMesh* aimesh = _scene->mMeshes[m];
			if (aimesh)
			{
				RRMeshArrays* mesh = meshes+m;
				texcoords.clear();
				unsigned numTriangles = aimesh->mNumFaces;
				unsigned numVertices = aimesh->mNumVertices;
				unsigned numTexcoords = aimesh->GetNumUVChannels();
				bool tangents = aimesh->mTangents && aimesh->mBitangents;
				for (unsigned i=0;i<numTexcoords;i++)
					texcoords.push_back(i);
				if (mesh->resizeMesh(numTriangles,numVertices,&texcoords,tangents,false))
				{	
					for (unsigned t=0;t<numTriangles;t++)
						memcpy(&mesh->triangle[t][0],&aimesh->mFaces[t].mIndices[0],sizeof(RRMesh::Triangle));
					if (sizeof(ai_real)==sizeof(RRReal))
					{
						// memcpy if both assimp and lightsprint use the same float type
						memcpy(mesh->position,aimesh->mVertices,numVertices*sizeof(RRVec3));
						memcpy(mesh->normal,aimesh->mNormals,numVertices*sizeof(RRVec3));
						if (tangents)
						{
							memcpy(mesh->tangent,aimesh->mTangents,numVertices*sizeof(RRVec3));
							memcpy(mesh->bitangent,aimesh->mBitangents,numVertices*sizeof(RRVec3));
						}
					}
					else
					{
						// convert between doubles and floats
						for (unsigned v=0;v<numVertices;v++)
						{
							mesh->position[v] = convertPos(aimesh->mVertices[v]);
							mesh->normal[v] = convertDir(aimesh->mNormals[v]);
							if (tangents) mesh->tangent[v] = convertDir(aimesh->mTangents[v]);
							if (tangents) mesh->bitangent[v] = convertDir(aimesh->mBitangents[v]);
						}
					}
					for (unsigned uv=0;uv<numTexcoords;uv++)
						for (unsigned v=0;v<numVertices;v++)
							mesh->texcoord[uv][v] = convertUv(aimesh->mTextureCoords[uv][v]);
				}
			}
		}

		// create colliders
		colliders = new RRCollider*[numMeshes];
		for (unsigned c=0;c<numMeshes;c++)
		{
			bool aborting = false;
			colliders[c] = RRCollider::create(meshes+c,nullptr,RRCollider::IT_LINEAR,aborting);
		}

		// adapt objects
		aiMatrix4x4 identity;
		addNode(_scene,_scene->mRootNode,identity);
	}

	void addNode(const aiScene* _scene, aiNode* _node, aiMatrix4x4 _transformation)
	{
		bool collapse = false; // import all meshes referenced by aiNode as one object?

		if (_node)
		{
			_transformation *= _node->mTransformation;
			RRMatrix3x4 world = convertMatrix(_transformation);

			for (unsigned i=0;i<_scene->mNumLights;i++)
			{
				if (_scene->mLights[i]->mName==_node->mName)
				{
					// each light is transformed by nodes with the same name
					// i hope assimp ensures that there is only one such node
					// transform lights in this node
					_scene->mLights[i]->mDirection = _transformation * _scene->mLights[i]->mDirection;
					_scene->mLights[i]->mPosition = _transformation * _scene->mLights[i]->mPosition + aiVector3D(_transformation.d1, _transformation.d2, _transformation.d3);
				}
			}

			RRObjects objectsInThisNode;
			for (unsigned i=0;i<_node->mNumMeshes;i++)
			{
				unsigned meshIndex = _node->mMeshes[i];
				RRCollider* collider = colliders[meshIndex];
				if (collider)
				{
					RRObject* object = new RRObject;
					object->setCollider(collider);
					object->setWorldMatrix(&world);
					if (_node->mParent && _node->mParent->mParent && !_node->mParent->mNumMeshes && _node->mParent->mName.length)
						object->name.format(L"%hs / %hs",_node->mParent->mName.data,_node->mName.data); // prepend name of empty non-root parent, helps in .ifc
					else
						object->name = convertStr(_node->mName);
					object->faceGroups.push_back(RRObject::FaceGroup(&materials[_scene->mMeshes[meshIndex]->mMaterialIndex],meshes[meshIndex].numTriangles));
					if (!collapse)
						push_back(object);
					else
						objectsInThisNode.push_back(object);
				}
			}
			if (collapse && objectsInThisNode.size())
			{
				bool aborting = false;
				RRObject* object = objectsInThisNode.createMultiObject(RRCollider::IT_LINEAR,aborting,-1,-1,true,0,nullptr);
				object->name = convertStr(_node->mName);
				push_back(object);
			}

			for (unsigned i=0;i<_node->mNumChildren;i++)
			{
				addNode(_scene,_node->mChildren[i],_transformation);
			}
		}
	}

	void convertMaterialProperty(aiMaterial* aimaterial, aiTextureType aitype, const char* aimatkey, unsigned zero1, unsigned zero2, RRMaterial& material, RRMaterial::Property& property)
	{
		aiString str;
		aimaterial->Get(_AI_MATKEY_TEXTURE_BASE,aitype,0,str);
		if (str.length)
		{
			aimaterial->Get(_AI_MATKEY_UVWSRC_BASE, aitype, 0, (int&)property.texcoord);
			if (property == material.lightmap && str == aiString("unwrap.jpg"))
			{
				// I can't find way to specify ambient uv channel without texture in .dae file.
				// So in koupelna4.dae, I use fake "unwrap" name. Assimp somehow appends .jpg.
				// Skip loading the fake "unwrap.jpg", we don't want to flood log with warnings.
				property.texture = nullptr;
				return;
			}
			//property.texture = RRBuffer::load(convertStr(str));
			property.texture = RRBuffer::load(convertStr(str),nullptr,textureLocator);
			if (property.texture)
			{
				int texFlags;
				if (aimaterial->Get(_AI_MATKEY_TEXFLAGS_BASE,aitype,0,texFlags)==AI_SUCCESS)
				{
					if (texFlags&aiTextureFlags_Invert)
					{
						if (&property==&material.specularTransmittance)
						{
							material.specularTransmittanceMapInverted = true;
						}
						else
						{
							RRBuffer* textureCopy = property.texture->createCopy();
							textureCopy->invert();
							delete property.texture;
							property.texture = textureCopy;
						}
					}
				}
			}
		}
		else
		if (aimatkey)
		{
			aiColor3D color;
			if (aimaterial->Get(aimatkey,zero1,zero2,color)==AI_SUCCESS)
				property.color = convertColor(color);
		}
	}

	virtual ~RRObjectsAssimp()
	{
		// delete objects
		for (unsigned i=0;i<size();i++)
		{
			delete (*this)[i];
		}

		// delete colliders
		for (unsigned i=0;i<numMeshes;i++)
		{
			delete colliders[i];
		}
		delete[] colliders;

		// delete meshes
		delete[] meshes;

		// delete materials
		delete[] materials;
	}

private:
	const RRFileLocator* textureLocator;
	RRMaterial* materials;
	unsigned numMeshes;
	RRMeshArrays* meshes;
	RRCollider** colliders;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLightsAssimp

class RRLightsAssimp : public RRLights
{
public:
	RRLightsAssimp(const aiScene* scene)
	{
		if (!scene)
			return;
		for (unsigned i=0;i<scene->mNumLights;i++)
		{
			aiLight* ailight = scene->mLights[i];
			if (ailight
				// ignore specular and ambient lights, what a hack, we are adding realism, not declaring war on it
				&& ailight->mColorDiffuse!=aiColor3D(0,0,0))
			{
				RRLight* light = nullptr;
				switch (ailight->mType)
				{
					case aiLightSource_DIRECTIONAL:
						light = RRLight::createDirectionalLight(
							convertDir(ailight->mDirection),
							convertColor(ailight->mColorDiffuse),
							false);
						break;
					case aiLightSource_POINT:
						light = RRLight::createPointLightPoly(
							convertPos(ailight->mPosition),
							convertColor(ailight->mColorDiffuse),
							RRVec4(ailight->mAttenuationConstant,ailight->mAttenuationLinear,ailight->mAttenuationQuadratic,0));
						break;
					case aiLightSource_SPOT:
						light = RRLight::createSpotLightPoly(
							convertPos(ailight->mPosition),
							convertColor(ailight->mColorDiffuse),
							RRVec4(ailight->mAttenuationConstant,ailight->mAttenuationLinear,ailight->mAttenuationQuadratic,0),
							convertDir(ailight->mDirection),
							ailight->mAngleOuterCone,
							ailight->mAngleOuterCone-ailight->mAngleInnerCone,
							1);
						break;
					default: RR_ASSERT(0);
				}
				if (light)
				{
					light->name = convertStr(ailight->mName);
					push_back(light);
				}
			}
		}
	}
	virtual ~RRLightsAssimp()
	{
		for (unsigned i=0;i<size();i++)
			delete (*this)[i];
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRSceneAssimp - importer

class RRSceneAssimp : public RRScene
{
public:
	static RRScene* load(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
	{
		aiPropertyStore* propertyStore = aiCreatePropertyStore();
		aiSetImportPropertyInteger(propertyStore,AI_CONFIG_PP_SBP_REMOVE,aiPrimitiveType_LINE|aiPrimitiveType_POINT);
		aiSetImportPropertyInteger(propertyStore,AI_CONFIG_PP_RVC_FLAGS,aiComponent_COLORS|aiComponent_BONEWEIGHTS|aiComponent_ANIMATIONS);
		aiSetImportPropertyFloat(propertyStore,AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE,30.f);

		const aiScene* aiscene = aiImportFileExWithProperties(RR_RR2CHAR(filename),0 // assimp doesn't support unicode filename
			//|aiProcess_CalcTangentSpace
			//|aiProcess_JoinIdenticalVertices
			//|aiProcess_MakeLeftHanded
			|aiProcess_Triangulate
			|aiProcess_RemoveComponent
			//|aiProcess_GenNormals
			|aiProcess_GenSmoothNormals
			//|aiProcess_SplitLargeMeshes
			//|aiProcess_PreTransformVertices
			//|aiProcess_LimitBoneWeights
			//|aiProcess_ValidateDataStructure
			//|aiProcess_ImproveCacheLocality
			|aiProcess_RemoveRedundantMaterials // .ifc contains many identical materials
			//|aiProcess_FixInfacingNormals
			|aiProcess_SortByPType
			//|aiProcess_FindDegenerates
			//|aiProcess_FindInvalidData
			|aiProcess_GenUVCoords
			|aiProcess_TransformUVCoords
			//|aiProcess_FindInstances
			//|aiProcess_OptimizeMeshes
			//|aiProcess_OptimizeGraph
			//|aiProcess_FlipUVs
			//|aiProcess_FlipWindingOrder
			,nullptr,propertyStore);
		aiReleasePropertyStore(propertyStore);
		if (!aiscene)
		{
			RRReporter::report(ERRO,"%s\n",aiGetErrorString());
			return nullptr;
		}
		RRSceneAssimp* scene = new RRSceneAssimp;
		RRReportInterval report(INF3,"Adapting scene...\n");
		RRString subdirTex = RR_PATH2RR(bf::path(RR_RR2PATH(filename)).parent_path()/"tex/"); // .c4d is known to have textures in "tex" subdir
		RRString subdirTextures = RR_PATH2RR(bf::path(RR_RR2PATH(filename)).parent_path()/"textures/"); // "textures" is another subdir searched by assimp_view
		if (textureLocator)
		{
			textureLocator->setParent(true,subdirTex);
			textureLocator->setParent(true,subdirTextures);
		}
		// first objects, then lights, because we transform lights while reading objects
		scene->protectedObjects = new RRObjectsAssimp(aiscene,textureLocator);
		scene->protectedLights = new RRLightsAssimp(aiscene);
		for (unsigned i=0;i<aiscene->mNumCameras;i++)
			if (aiscene->mCameras[i])
				scene->cameras.push_back(convertCamera(*aiscene->mCameras[i]));
		if (textureLocator)
		{
			textureLocator->setParent(false,subdirTextures);
			textureLocator->setParent(false,subdirTex);
		}
		aiReleaseImport(aiscene);
		return scene;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderAssimp()
{
	aiString extensions;
	aiGetExtensionList(&extensions);
	std::string str(extensions.C_Str());
#if defined(SUPPORT_OPENCOLLADA) || defined(SUPPORT_FCOLLADA)
	// hide assimp collada loader if better one exists
	if (str.find("*.dae;")!=std::string::npos)
		str.erase(str.find("*.dae;"),6);
#endif
#ifdef SUPPORT_3DS
	// hide assimp 3ds loader if better? one exists
	if (str.find("*.3ds;")!=std::string::npos)
		str.erase(str.find("*.3ds;"),6);
#endif
#if defined(ASSIMP_SUPPORT_ONLY_FORMATS)
	str = ASSIMP_SUPPORT_ONLY_FORMATS;
#endif
	RRScene::registerLoader(str.c_str(),RRSceneAssimp::load);
}

#endif // SUPPORT_ASSIMP
