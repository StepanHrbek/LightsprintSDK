// --------------------------------------------------------------------------
// Lightsprint adapters for Assimp formats.
// Copyright (C) 2009-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_ASSIMP

#include "RRObjectAssimp.h"
#include "Lightsprint/RRScene.h"
#include "include/assimp/cimport.h"
#include "include/assimp/config.h"
#include "include/assimp/postprocess.h"
#include "include/assimp/scene.h"
#include "include/assimp/material.inl"

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

inline RRVec2 convertUv(const aiVector3D& v)
{
	return RRVec2(v.x, v.y);
}

inline RRVec3 convertPos(const aiVector3D& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertDir(const aiVector3D& v)
{
	return RRVec3(v.x, v.y, v.z);
}

inline RRVec3 convertColor(const aiColor3D& c)
{
	return RRVec3(c.r, c.g, c.b);
}

inline const char* convertStr(const aiString& s)
{
	return s.data;
}

RRMatrix3x4 convertMatrix(const aiMatrix4x4& transform)
{
	RRMatrix3x4 wm;
	for (unsigned i=0;i<3;i++)
		for (unsigned j=0;j<4;j++)
			wm.m[i][j] = transform[i][j];
	return wm;
}

RRCamera convertCamera(const aiCamera& c)
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
		materials = NULL;
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
				? ((material.specularShininess<=1)?rr::RRMaterial::BLINN_TORRANCE_SPARROW:rr::RRMaterial::BLINN_PHONG)
				: rr::RRMaterial::PHONG;

			// diffuseReflectance
			if (model==aiShadingMode_NoShading)
				material.diffuseReflectance.color = RRVec3(0);
			else
				convertMaterialProperty(aimaterial,aiTextureType_DIFFUSE,AI_MATKEY_COLOR_DIFFUSE,material.diffuseReflectance);

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
			convertMaterialProperty(aimaterial,aiTextureType_EMISSIVE,AI_MATKEY_COLOR_EMISSIVE,material.diffuseEmittance);

			// specularTransmittance
			{
				convertMaterialProperty(aimaterial,aiTextureType_OPACITY,AI_MATKEY_COLOR_TRANSPARENT,material.specularTransmittance);
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
			convertMaterialProperty(aimaterial,aiTextureType_NORMALS,NULL,0,0,material.bumpMap);
			if (!material.bumpMap.texture)
				convertMaterialProperty(aimaterial,aiTextureType_HEIGHT,NULL,0,0,material.bumpMap);

			// lightmapTexcoord
			if (aimaterial->Get(_AI_MATKEY_UVWSRC_BASE,aiTextureType_LIGHTMAP,0,(int&)material.lightmapTexcoord)!=AI_SUCCESS)
				aimaterial->Get(_AI_MATKEY_UVWSRC_BASE,aiTextureType_AMBIENT,0,(int&)material.lightmapTexcoord);

			// get average colors from textures
			{
				RRScaler* scaler = RRScaler::createRgbScaler();
				material.updateColorsFromTextures(scaler,RRMaterial::UTA_NULL,true);
				delete scaler;
			}

			// final decision on number of sides
			if (unknownNumberOfSides && material.specularTransmittance.color!=rr::RRVec3(0))
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
					memcpy(mesh->position,aimesh->mVertices,numVertices*sizeof(RRVec3));
					memcpy(mesh->normal,aimesh->mNormals,numVertices*sizeof(RRVec3));
					if (tangents)
					{
						memcpy(mesh->tangent,aimesh->mTangents,numVertices*sizeof(RRVec3));
						memcpy(mesh->bitangent,aimesh->mBitangents,numVertices*sizeof(RRVec3));
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
			colliders[c] = RRCollider::create(meshes+c,NULL,RRCollider::IT_LINEAR,aborting);
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

			RRObjects objectsInThisNode;
			for (unsigned i=0;i<_node->mNumMeshes;i++)
			{
				unsigned meshIndex = _node->mMeshes[i];
				RRCollider* collider = colliders[meshIndex];
				if (collider)
				{
					RRObject* object = new RRObject;
					object->setCollider(collider);
					RRMatrix3x4 world = convertMatrix(_transformation);
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
				RRObject* object = RRObject::createMultiObject(&objectsInThisNode,RRCollider::IT_LINEAR,aborting,-1,-1,true,0,NULL);
				object->name = convertStr(_node->mName);
				push_back(object);
			}

			for (unsigned i=0;i<_node->mNumChildren;i++)
			{
				addNode(_scene,_node->mChildren[i],_transformation);
			}
		}
	}

	void convertMaterialProperty(aiMaterial* aimaterial, aiTextureType aitype, const char* aimatkey, unsigned zero1, unsigned zero2, RRMaterial::Property& property)
	{
		aiString str;
		aimaterial->Get(_AI_MATKEY_TEXTURE_BASE,aitype,0,str);
		if (str.length)
		{
			//property.texture = RRBuffer::load(convertStr(str));
			property.texture = RRBuffer::load(convertStr(str),NULL,textureLocator);
			aimaterial->Get(_AI_MATKEY_UVWSRC_BASE,aitype,0,(int&)property.texcoord);
			if (property.texture)
			{
				int texFlags;
				if (aimaterial->Get(_AI_MATKEY_TEXFLAGS_BASE,aitype,0,texFlags)==AI_SUCCESS)
				{
					if (texFlags&aiTextureFlags_Invert)
					{
						RRBuffer* textureCopy = property.texture->createCopy();
						textureCopy->invert();
						delete property.texture;
						property.texture = textureCopy;
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
				RRLight* light = NULL;
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
// RRSceneAssimp

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
			,NULL,propertyStore);
		aiReleasePropertyStore(propertyStore);
		if (!aiscene)
		{
			RRReporter::report(ERRO,aiGetErrorString());
			return NULL;
		}
		RRSceneAssimp* scene = new RRSceneAssimp;
		RRReportInterval report(INF3,"Adapting scene...\n");
		scene->protectedObjects = new RRObjectsAssimp(aiscene,textureLocator);
		scene->protectedLights = new RRLightsAssimp(aiscene);
		for (unsigned i=0;i<aiscene->mNumCameras;i++)
			if (aiscene->mCameras[i])
				scene->cameras.push_back(convertCamera(*aiscene->mCameras[i]));
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
	std::string str(convertStr(extensions));
#if defined(SUPPORT_OPENCOLLADA) || defined(SUPPORT_FCOLLADA)
	// hide assimp collada loader if better one exists
	str.erase(str.find("*.dae;"),6);
#endif
#ifdef SUPPORT_3DS
	// hide assimp 3ds loader if better? one exists
	str.erase(str.find("*.3ds;"),6);
#endif
	RRScene::registerLoader(str.c_str(),RRSceneAssimp::load);
}

#endif // SUPPORT_ASSIMP
