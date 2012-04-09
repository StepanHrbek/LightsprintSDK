// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#define MIRRORS // enables implementation of mirrors, marks mirror source code

#include <algorithm> // sort
#ifdef MIRRORS
	#include <map>
#endif
#include <cstdio>
#include <boost/unordered_map.hpp>
#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "MultiPass.h"
#include "RendererOfMesh.h"
#include "Workaround.h"
#include "tmpstr.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfMeshCache

class RendererOfMeshCache
{
public:
	RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh)
	{
		Cache::iterator i=cache.find(mesh);
		if (i!=cache.end())
		{
			return i->second;
		}
		else
		{
			return cache[mesh] = new RendererOfMesh();
		}
	}
	~RendererOfMeshCache()
	{
		for (Cache::const_iterator i=cache.begin();i!=cache.end();++i)
			delete i->second;
	}
private:
	typedef boost::unordered_map<const rr::RRMesh*,RendererOfMesh*> Cache;
	Cache cache;
};


//////////////////////////////////////////////////////////////////////////////
//
// PerObjectBuffers

struct PerObjectBuffers
{
	float eyeDistance;
	rr::RRObject* object;
	RendererOfMesh* meshRenderer;
	rr::RRBuffer* reflectionEnvMap;
#ifdef MIRRORS
	rr::RRVec4 mirrorPlane;
	rr::RRBuffer* mirrorMap;
#endif
	rr::RRBuffer* lightIndirectBuffer;
	rr::RRBuffer* lightIndirectDetailMap;
	UberProgramSetup objectUberProgramSetup; // only for blended facegroups. everything is set except for MATERIAL_XXX, use enableUsedMaterials()
};


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfSceneImpl

class RendererOfSceneImpl : public RendererOfScene
{
public:
	RendererOfSceneImpl(const char* pathToShaders);
	virtual ~RendererOfSceneImpl();

	virtual void render(
		rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLayers,
		unsigned _layerLightmap,
		unsigned _layerEnvironment,
		unsigned _layerLDM,
		const ClipPlanes* _clipPlanes,
		bool _srgbCorrect,
		const rr::RRVec4* _brightness,
		float _gamma);

	virtual RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh)
	{
		return rendererOfMeshCache.getRendererOfMesh(mesh);
	}

	virtual TextureRenderer* getTextureRenderer()
	{
		return textureRenderer;
	}

private:
	// PERMANENT CONTENT
	TextureRenderer* textureRenderer;
	UberProgram* uberProgram;
	RendererOfMeshCache rendererOfMeshCache;
#ifdef MIRRORS
	rr::RRBuffer* depthMap;
#endif

	// PERMANENT ALLOCATION, TEMPORARY CONTENT (used only inside render(), placing it here saves alloc/free in every render(), but makes render() nonreentrant)
	rr::RRObjects multiObjects; // used in first half of render()
	//! Gathered per-object information.
	rr::RRVector<PerObjectBuffers> perObjectBuffers[2]; // used in whole render(), indexed by [recursionDepth]
	typedef boost::unordered_map<UberProgramSetup,rr::RRVector<FaceGroupRange>*> ShaderFaceGroups;
	//! Gathered non-blended object information.
	ShaderFaceGroups nonBlendedFaceGroupsMap[2]; // used in whole render(), indexed by [recursionDepth]
	//! Gathered blended object information.
	rr::RRVector<FaceGroupRange> blendedFaceGroups[2]; // used in whole render(), indexed by [recursionDepth]
	//! usually 0, 1 only when render() calls render() because of mirror
	unsigned recursionDepth;
};

static rr::RRVector<PerObjectBuffers>* s_perObjectBuffers = NULL; // used by sort()

bool FaceGroupRange::operator <(const FaceGroupRange& a) const // for sort()
{
	return (*s_perObjectBuffers)[object].eyeDistance>(*s_perObjectBuffers)[a.object].eyeDistance;
}

RendererOfSceneImpl::RendererOfSceneImpl(const char* pathToShaders)
{
	textureRenderer = new TextureRenderer(pathToShaders);
	uberProgram = UberProgram::create(
		tmpstr("%subershader.vs",pathToShaders),
		tmpstr("%subershader.fs",pathToShaders));
#ifdef MIRRORS
	depthMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,512,512,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
#endif
	recursionDepth = 0;
}

RendererOfSceneImpl::~RendererOfSceneImpl()
{
#ifdef MIRRORS
	delete depthMap;
#endif
	delete uberProgram;
	delete textureRenderer;
	for (recursionDepth=0;recursionDepth<2;recursionDepth++)
		for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
			delete i->second;
}

rr::RRBuffer* onlyVbuf(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_VERTEX_BUFFER) ? buffer : NULL;
}
rr::RRBuffer* onlyLmap(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_2D_TEXTURE) ? buffer : NULL;
}
rr::RRBuffer* onlyCube(rr::RRBuffer* buffer)
{
	return (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE) ? buffer : NULL;
}

void RendererOfSceneImpl::render(
		rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const RealtimeLights* _lights,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLayers,
		unsigned _layerLightmap,
		unsigned _layerEnvironment,
		unsigned _layerLDM,
		const ClipPlanes* _clipPlanes,
		bool _srgbCorrect,
		const rr::RRVec4* _brightness,
		float _gamma)
{
	if (!_solver)
	{
		RR_ASSERT(0);
		return;
	}

	// Get current camera for culling and distance-sorting.
	const rr::RRCamera* eye = getRenderCamera();
	if (!eye)
	{
		RR_ASSERT(0); // eye not set
		return;
	}

	// Ensure sRGB correctness.
	if (!Workaround::supportsSRGB())
		_srgbCorrect = false;
	PreserveFlag p0(GL_FRAMEBUFFER_SRGB,_srgbCorrect);
	if (_srgbCorrect)
	{
		_gamma *= 2.2f;
	}

#ifdef MIRRORS
	// get viewport size, so we know how large mirror textures to use
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	// here we map planes to mirrors, so that two objects with the same plane share mirror
	struct PlaneCompare // comparing RRVec4 looks strange, so we do it in private PlaneCompare rather than in public RRVec4 operator<
	{
		bool operator()(const rr::RRVec4& a, const rr::RRVec4& b) const
		{
			return a.x<b.x || (a.x==b.x && (a.y<b.y || (a.y==b.y && (a.z<b.z || (a.z==b.z && a.w<b.w)))));
		}
	};
	typedef std::map<rr::RRVec4,rr::RRBuffer*,PlaneCompare> Mirrors;
	Mirrors mirrors;
#endif

	// Will we render multiobject or individual objects?
	//unsigned lightIndirectVersion = _solver?_solver->getSolutionVersion():0;
	bool needsIndividualStaticObjectsForEverything =
		// optimized render is faster and supports rendering into shadowmaps (this will go away with colored shadows)
		!_renderingFromThisLight

		// disabled, this forced multiobj even if user expected lightmaps from 1obj to be rendered
		// should be no loss, using multiobj in scenes with 1 object was faster only in old renderer, new renderer makes no difference
		//&& _solver->getStaticObjects().size()>1

		&& (
			// optimized render can't render LDM for more than 1 object
			(_uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _layerLDM!=UINT_MAX)
			// if we are to use provided indirect, take it always from 1objects
			// (if we are to update indirect, we update and render it in 1object or multiobject, whatever is faster. so both buffers must be allocated)
			|| ((_uberProgramSetup.LIGHT_INDIRECT_VCOLOR||_uberProgramSetup.LIGHT_INDIRECT_MAP) && !_updateLayers && _layerLightmap!=UINT_MAX)
			// optimized render would look bad with single specular cube per-scene
			|| ((_uberProgramSetup.MATERIAL_SPECULAR && _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) && _layerEnvironment!=UINT_MAX)
#ifdef MIRRORS
			// optimized render would look bad without mirrors in static parts
			|| (_uberProgramSetup.MATERIAL_DIFFUSE && _uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE)
			|| (_uberProgramSetup.MATERIAL_SPECULAR && _uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR)
#endif
		);

	// Rendering multiobj with normal maps enabled, check if normal maps are really present (rare), they would need switch to 1obj rendering.
	if (_uberProgramSetup.MATERIAL_NORMAL_MAP && !needsIndividualStaticObjectsForEverything)
	{
		const rr::RRObjects& objects = _solver->getStaticObjects();
		for (unsigned i=0;i<objects.size();i++)
		{
			const rr::RRMeshArrays* arrays = dynamic_cast<const rr::RRMeshArrays*>(objects[i]->getCollider()->getMesh());
			if (arrays && arrays->tangent && arrays->bitangent)
				needsIndividualStaticObjectsForEverything = true;
		}
	}

	// Will we render opaque parts from multiobject and blended parts from 1objects?
	// It's optimizations, makes render 10x faster in diacor (25k 1objects), compared to rendering everything from 1objects.
	bool needsIndividualStaticObjectsOnlyForBlending =
		!needsIndividualStaticObjectsForEverything
		&& !_renderingFromThisLight
		&& (
			// optimized render can't sort
			_uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND
		);

	rr::RRReportInterval report(rr::INF3,"Rendering %s%s scene...\n",needsIndividualStaticObjectsForEverything?"1obj":"multiobj",needsIndividualStaticObjectsOnlyForBlending?"+1objblend":"");

	// Preprocess scene, gather information about shader classes. Update cubemaps. Gather mirrors that need update.
	perObjectBuffers[recursionDepth].clear();
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
		i->second->clear();
	blendedFaceGroups[recursionDepth].clear();
	multiObjects.clear();
	multiObjects.push_back(_solver->getMultiObjectCustom());
	for (unsigned pass=0;pass<3;pass++)
	{
		const rr::RRObjects* objects;
		switch (pass)
		{
			case 0:
				if (needsIndividualStaticObjectsForEverything) continue;
				objects = &multiObjects;
				break;
			case 1:
				if (!needsIndividualStaticObjectsForEverything && !needsIndividualStaticObjectsOnlyForBlending) continue;
				objects = &_solver->getStaticObjects();
				break;
			case 2:
				if (_uberProgramSetup.FORCE_2D_POSITION) continue;
				objects = &_solver->getDynamicObjects();
				break;
		}
		for (unsigned i=0;i<objects->size();i++)
		{
			rr::RRObject* object = (*objects)[i];
			if (object)// && !eye->frustumCull(object))
			{
				const rr::RRMesh* mesh = object->getCollider()->getMesh();
				rr::RRObjectIllumination& illumination = object->illumination;

				PerObjectBuffers objectBuffers;
				objectBuffers.object = object;
				objectBuffers.meshRenderer = rendererOfMeshCache.getRendererOfMesh(mesh);
				objectBuffers.reflectionEnvMap = (_uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) ? onlyCube(illumination.getLayer(_layerEnvironment)) : NULL;
#ifdef MIRRORS
				objectBuffers.mirrorMap = NULL;
				objectBuffers.mirrorPlane = rr::RRVec4(0);
				if ((_uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE || _uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR) && !onlyCube(illumination.getLayer(_layerEnvironment)))
				{
					rr::RRVec3 mini,maxi;
					mesh->getAABB(&mini,&maxi,NULL);
					rr::RRVec3 size = maxi-mini;

					// is it planar?
					if (!size.mini() && size.sum()>size.maxi())
					{
						rr::RRVec4 plane = size.x ? ( size.y ? rr::RRVec4(0,0,1,-mini.z) : rr::RRVec4(0,1,0,-mini.y) ) : rr::RRVec4(1,0,0,-mini.x);
						rr::RRVec4 mirrorPlane = object->getWorldMatrixRef().getTransformedPlane(plane);
						{
							// is it reflective?
							const rr::RRObject::FaceGroups& faceGroups = object->faceGroups;
							for (unsigned g=0;g<faceGroups.size();g++)
							{
								const rr::RRMaterial* material = faceGroups[g].material;
								if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom) && material->specularReflectance.color.sum()+material->diffuseReflectance.color.sum()>0)
								{
									// allocate mirror
									objectBuffers.mirrorPlane = mirrorPlane;
									Mirrors::const_iterator i = mirrors.find(mirrorPlane);
									if (i!=mirrors.end())
										objectBuffers.mirrorMap = i->second;
									else
									{
										objectBuffers.mirrorMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,(viewport[2]+1)/2,(viewport[3]+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
										mirrors.insert(std::pair<rr::RRVec4,rr::RRBuffer*>(objectBuffers.mirrorPlane,objectBuffers.mirrorMap));
									}
									break;
								}
							}
						}
					}
				}
#endif
				rr::RRBuffer* lightIndirectVcolor = _uberProgramSetup.LIGHT_INDIRECT_VCOLOR ? onlyVbuf(illumination.getLayer(_layerLightmap)) : NULL;
				rr::RRBuffer* lightIndirectMap = _uberProgramSetup.LIGHT_INDIRECT_MAP ? onlyLmap(illumination.getLayer(_layerLightmap)) : NULL;
				objectBuffers.lightIndirectBuffer = lightIndirectVcolor?lightIndirectVcolor:lightIndirectMap;
				objectBuffers.lightIndirectDetailMap = _uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? onlyLmap(illumination.getLayer(_layerLDM)) : NULL;

				objectBuffers.objectUberProgramSetup = _uberProgramSetup;
				if (pass==2 && objectBuffers.objectUberProgramSetup.SHADOW_MAPS>1)
					objectBuffers.objectUberProgramSetup.SHADOW_MAPS = 1; // decrease shadow quality on dynamic objects
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = _uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && objectBuffers.reflectionEnvMap;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = _uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR && objectBuffers.reflectionEnvMap;
#ifdef MIRRORS
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = objectBuffers.mirrorMap && !objectBuffers.lightIndirectBuffer && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = objectBuffers.mirrorMap!=NULL;
#else
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
#endif
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_CONST = _uberProgramSetup.LIGHT_INDIRECT_CONST && !lightIndirectVcolor && !lightIndirectMap && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE; // keep const only if no other indirect diffuse is available
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR = lightIndirectVcolor!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = lightIndirectVcolor!=NULL && !lightIndirectVcolor->getScaled();
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP = lightIndirectMap!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP2 = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = objectBuffers.lightIndirectDetailMap!=NULL;
				objectBuffers.objectUberProgramSetup.OBJECT_SPACE = object->getWorldMatrix()!=NULL;
				if (_srgbCorrect) // we changed gamma value, it has to be enabled in shader to have effect
					objectBuffers.objectUberProgramSetup.POSTPROCESS_GAMMA = true;

				const rr::RRObject::FaceGroups& faceGroups = object->faceGroups;
				bool blendedAlreadyFoundInObject = false;
				unsigned triangleInFgFirst = 0;
				unsigned triangleInFgLastPlus1 = 0;
				bool objectWillBeRendered = false; // solid 1obj won't be rendered if we mix solid from multiobj and blended from 1objs. then it does not need indirect updated
				if (faceGroups.size()>65535) // FaceGroupRange contains 16bit shorts
					RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Object has %d facegroups, that's sick (we render only first 65535).\n",faceGroups.size()));
				for (unsigned g=0;g<faceGroups.size();g++)
				{
					triangleInFgLastPlus1 += faceGroups[g].numTriangles;
					const rr::RRMaterial* material = faceGroups[g].material;
					if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom))
					{
						if (material->needsBlending() && _uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND)
						{
							// here we process everything that needs sorting, i.e. what is blended in final render
							// blended objects rendered into rgb shadowmap are not processed here, because they don't need sort
							if (!needsIndividualStaticObjectsOnlyForBlending || pass!=0)
							{
								objectWillBeRendered = true;
								blendedFaceGroups[recursionDepth].push_back(FaceGroupRange(perObjectBuffers[recursionDepth].size(),g,triangleInFgFirst,triangleInFgLastPlus1));
								if (!blendedAlreadyFoundInObject)
								{
									blendedAlreadyFoundInObject = true;
									rr::RRVec3 center;
									mesh->getAABB(NULL,NULL,&center);
									const rr::RRMatrix3x4* worldMatrix = object->getWorldMatrix();
									if (worldMatrix)
										worldMatrix->transformPosition(center);
									objectBuffers.eyeDistance = (eye->getPosition()-center).length();
								}
							}
						}
						else
						if (!needsIndividualStaticObjectsOnlyForBlending || pass!=1)
						{
							objectWillBeRendered = true;
							UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
							fgUberProgramSetup.enableUsedMaterials(material,dynamic_cast<const rr::RRMeshArrays*>(mesh));
							fgUberProgramSetup.reduceMaterials(_uberProgramSetup);
							fgUberProgramSetup.validate();
							rr::RRVector<FaceGroupRange>*& nonBlended = nonBlendedFaceGroupsMap[recursionDepth][fgUberProgramSetup];
							if (!nonBlended)
								nonBlended = new rr::RRVector<FaceGroupRange>;
							if (nonBlended->size() && (*nonBlended)[nonBlended->size()-1].object==perObjectBuffers[recursionDepth].size() && (*nonBlended)[nonBlended->size()-1].faceGroupLast==g-1)
							{
								(*nonBlended)[nonBlended->size()-1].faceGroupLast++;
								(*nonBlended)[nonBlended->size()-1].triangleLastPlus1 = triangleInFgLastPlus1;
							}
							else
								nonBlended->push_back(FaceGroupRange(perObjectBuffers[recursionDepth].size(),g,triangleInFgFirst,triangleInFgLastPlus1));
						}
					}
					triangleInFgFirst = triangleInFgLastPlus1;
				}
				perObjectBuffers[recursionDepth].push_back(objectBuffers);

				if (_updateLayers && objectWillBeRendered)
				{
					// update vertex buffers
					if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR
						// quit if buffer is already up to date
						&& lightIndirectVcolor->version!=_solver->getSolutionVersion())
					{
						if (pass==1)
						{
							// updates indexed 1object buffer
							_solver->updateLightmap(i,lightIndirectVcolor,NULL,NULL,NULL);
						}
						else
						if (pass==0)
						{
							// -1 = updates indexed multiobject buffer
							_solver->updateLightmap(-1,lightIndirectVcolor,NULL,NULL,NULL);
						}
					}
					// update cube maps
					// built-in version check
					if (objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE||objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
					{
						_solver->updateEnvironmentMap(&illumination,_layerEnvironment);
					}
				}
			}
		}
	}

#ifdef MIRRORS
	// Update mirrors.
	if (mirrors.size() && getRenderCamera())
	{
		recursionDepth = 1;
		UberProgramSetup mirrorUberProgramSetup = _uberProgramSetup;
		mirrorUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false; // Don't use mirror in mirror, to prevent update in update (infinite recursion).
		mirrorUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
		mirrorUberProgramSetup.CLIP_PLANE = true;
		rr::RRCamera mainCamera = *getRenderCamera();
		FBO oldState = FBO::getState();
		for (Mirrors::const_iterator i=mirrors.begin();i!=mirrors.end();++i)
		{
			rr::RRVec4 mirrorPlane = i->first;
			bool cameraInFrontOfMirror = mirrorPlane.planePointDistance(mainCamera.getPosition())>0;
			if (!cameraInFrontOfMirror) mirrorPlane = -mirrorPlane;
			mirrorPlane.w -= mirrorPlane.RRVec3::length()*mainCamera.getFar()*1e-5f; // add bias, clip face in clipping plane, avoid reflecting mirror in itself
			rr::RRBuffer* mirrorMap = i->second;
			rr::RRCamera mirrorCamera = mainCamera;
			mirrorCamera.mirror(mirrorPlane);
			setupForRender(mirrorCamera);
			ClipPlanes clipPlanes = {mirrorPlane,0,0,0,0,0,0};
			depthMap->reset(rr::BT_2D_TEXTURE,mirrorMap->getWidth(),mirrorMap->getHeight(),1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
			FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,getTexture(depthMap,false,false));
			Texture* mirrorTex = new Texture(mirrorMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE); // new Texture instead of getTexture makes our texture deletable at the end of render()
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,mirrorTex);
			glViewport(0,0,mirrorMap->getWidth(),mirrorMap->getHeight());
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			// Q: how to make mirrors srgb correct?
			// A: current mirror is always srgb incorrect, srgb correct render works only into real backbuffer, not into texture.
			//    result is not affected by using GL_RGB vs GL_SRGB
			//    even if we render srgb correctly into texture(=writes encode), reads in final render will decode(=we get what we wrote in, conversion is lost)
			//    for srgb correct mirror, we would have to
			//    a) add LIGHT_INDIRECT_MIRROR_SRGB, and convert manually in shader (would increase already large number of shaders)
			//    b) make ubershader work with linear light (number of differences between correct and incorrect path would grow too much, difficult to maintain)
			//       (srgb incorrect path must remain because of OpenGL ES)
			render(_solver,mirrorUberProgramSetup,_lights,NULL,_updateLayers,_layerLightmap,_layerEnvironment,_layerLDM,&clipPlanes,false,NULL,1);

			// build mirror mipmaps
			mirrorTex->bindTexture();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmapEXT(GL_TEXTURE_2D); // part of EXT_framebuffer_object
		}
		oldState.restore();
		setupForRender(mainCamera);
		glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
		recursionDepth = 0;
	}
#endif

	PreserveCullFace p1;
	PreserveCullMode p2;
	PreserveBlend p3;     // changed by RendererOfMesh (in MultiPass)
	PreserveColorMask p4; // changed by RendererOfMesh
	PreserveDepthMask p5; // changed by RendererOfMesh

	// Render non-sorted facegroups.
	for (unsigned transparencyToRGB=0;transparencyToRGB<2;transparencyToRGB++) // 2 passes, MATERIAL_TRANSPARENCY_TO_RGB(e.g. window) must go _after_ !MATERIAL_TRANSPARENCY_TO_RGB(e.g. wall), otherwise in "light - wall - window" scene, window would paint to rgb shadowmap (wall does not write to rgb) and cast rgb shadow on wall
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
	if (i->first.MATERIAL_TRANSPARENCY_TO_RGB == (transparencyToRGB!=0))
	{
		rr::RRVector<FaceGroupRange>*& nonBlendedFaceGroups = i->second;
		if (nonBlendedFaceGroups && nonBlendedFaceGroups->size())
		{
			const UberProgramSetup& classUberProgramSetup = i->first;
			if (_uberProgramSetup.MATERIAL_CULLING && !classUberProgramSetup.MATERIAL_CULLING)
			{
				// we are rendering with culling, but it was disabled in this class because front=back
				// setup culling at the beginning
				glDisable(GL_CULL_FACE);
			}
			MultiPass multiPass(_lights,_renderingFromThisLight,classUberProgramSetup,uberProgram,_clipPlanes,_srgbCorrect,_brightness,_gamma);
			UberProgramSetup passUberProgramSetup;
			RealtimeLight* light;
			Program* program;
			while (program = multiPass.getNextPass(passUberProgramSetup,light))
			{
				for (unsigned j=0;j<nonBlendedFaceGroups->size();)
				{
					PerObjectBuffers& objectBuffers = perObjectBuffers[recursionDepth][(*nonBlendedFaceGroups)[j].object];
					rr::RRObject* object = objectBuffers.object;

					// set transformation
					passUberProgramSetup.useWorldMatrix(program,object);

					// set envmaps
					passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_layerEnvironment));
#ifdef MIRRORS
					// set mirror
					passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorMap);
#endif
					// how many ranges can we render at once (the same mesh)?
					unsigned numRanges = 1;
					while (j+numRanges<nonBlendedFaceGroups->size() && (*nonBlendedFaceGroups)[j+numRanges].object==(*nonBlendedFaceGroups)[j].object) numRanges++;

					// render
					objectBuffers.meshRenderer->render(
						program,
						object,
						&(*nonBlendedFaceGroups)[j],numRanges,
						passUberProgramSetup,
						_renderingFromThisLight?true:false,
						objectBuffers.lightIndirectBuffer,
						objectBuffers.lightIndirectDetailMap);

					j += numRanges;
#ifdef MIRRORS
					//if (objectBuffers.mirrorMap)
					//	textureRenderer->render2D(getTexture(objectBuffers.mirrorMap,false,false),NULL,1,0,0,0.5f,0.5f);
#endif
				}
			}
		}
	}

	// Render skybox.
 	if (!_renderingFromThisLight && !_uberProgramSetup.FORCE_2D_POSITION)
	{
		const rr::RRBuffer* env0 = _solver->getEnvironment(0);
		if (textureRenderer && env0)
		{
			const rr::RRBuffer* env1 = _solver->getEnvironment(1);
			float blendFactor = _solver->getEnvironmentBlendFactor();
			Texture* texture0 = (env0->getWidth()>2)
				? getTexture(env0,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env0,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				;
			Texture* texture1 = env1 ? ( (env1->getWidth()>2)
				? getTexture(env1,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env1,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				) : NULL;
			textureRenderer->renderEnvironment(texture0,texture1,blendFactor,_brightness,_gamma,true);
		}
	}

	// Render sorted facegroups.
	if (blendedFaceGroups[recursionDepth].size())
	{
		// Sort blended objects.
		s_perObjectBuffers = &perObjectBuffers[recursionDepth];
		std::sort(&blendedFaceGroups[recursionDepth][0],&blendedFaceGroups[recursionDepth][0]+blendedFaceGroups[recursionDepth].size());

		// Render them by facegroup, it's usually small number.
		for (unsigned i=0;i<blendedFaceGroups[recursionDepth].size();i++)
		{
			const PerObjectBuffers& objectBuffers = perObjectBuffers[recursionDepth][blendedFaceGroups[recursionDepth][i].object];
			rr::RRObject* object = objectBuffers.object;
			rr::RRMaterial* material = object->faceGroups[blendedFaceGroups[recursionDepth][i].faceGroupFirst].material;
			UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
			fgUberProgramSetup.enableUsedMaterials(material,dynamic_cast<const rr::RRMeshArrays*>(object->getCollider()->getMesh()));
			fgUberProgramSetup.reduceMaterials(_uberProgramSetup);
			fgUberProgramSetup.validate();
			MultiPass multiPass(_lights,_renderingFromThisLight,fgUberProgramSetup,uberProgram,_clipPlanes,_srgbCorrect,_brightness,_gamma);
			UberProgramSetup passUberProgramSetup;
			RealtimeLight* light;
			Program* program;
			while (program = multiPass.getNextPass(passUberProgramSetup,light))
			{
				// set transformation
				passUberProgramSetup.useWorldMatrix(program,object);

				// set envmaps
				passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_layerEnvironment));
#ifdef MIRRORS
				// set mirror
				passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorMap);
#endif
				// render
				objectBuffers.meshRenderer->render(
					program,
					object,
					&blendedFaceGroups[recursionDepth][i],1,
					passUberProgramSetup,
					_renderingFromThisLight?true:false,
					objectBuffers.lightIndirectBuffer,
					objectBuffers.lightIndirectDetailMap);
			}
		}
	}

#ifdef MIRRORS
	// Delete mirrors.
	for (Mirrors::const_iterator i=mirrors.begin();i!=mirrors.end();++i)
	{
		delete getTexture(i->second,false,false);
		delete i->second;
	}
#endif
}


//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene

RendererOfScene* RendererOfScene::create(const char* pathToShaders)
{
	return new RendererOfSceneImpl(pathToShaders);
}

}; // namespace
