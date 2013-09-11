// --------------------------------------------------------------------------
// Renderer implementation that renders contents of RRDynamicSolver instance.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#define MIRRORS // enables implementation of mirrors, marks mirror source code
#define OCCLUSION_QUERY // enables mirror optimization; usually helps, but makes CPU wait for GPU, kills all chances for speedup on dual GPU

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

// COPY_TO_CUBE:
//  -extra copying
//  -cube must not be bigger than current render target viewport/scissor
//  +multisampled
// rendering directly into cube:
//  -not srgb correct
//  +can be optimized (later) to single pass with geometry shader
//#define COPY_TO_CUBE

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
	rr::RRBuffer* mirrorColorMap;
#endif
	rr::RRBuffer* lightIndirectBuffer;
	rr::RRBuffer* lightIndirectDetailMap;
	UberProgramSetup objectUberProgramSetup; // only for blended facegroups. everything is set except for MATERIAL_XXX, use enableUsedMaterials()
};


//////////////////////////////////////////////////////////////////////////////
//
// for unordered_map<UberProgramSetup,>
//

std::size_t hash_value(const UberProgramSetup& b)
{
	unsigned* p = (unsigned*)&b;
	return p[0]+p[1];
}

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfSceneImpl

class RendererOfSceneImpl : public RendererOfScene
{
public:
	RendererOfSceneImpl(const char* pathToShaders);
	virtual ~RendererOfSceneImpl();

	virtual void render(rr::RRDynamicSolver* _solver, const RealtimeLights* _lights, const RenderParameters& _renderParameters);
	virtual void renderToCube(rr::RRDynamicSolver* _solver, const RealtimeLights* _lights, const RenderParameters& _renderParameters, Texture* _cubeTexture);

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
	Texture* stereoTexture; // for render()
	Texture* panoramaTexture; // for render()
	Texture* depthTexture; // for renderToCube()
	UberProgram* stereoUberProgram;
#ifdef MIRRORS
	rr::RRBuffer* mirrorDepthMap;
	rr::RRBuffer* mirrorMaskMap;
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
	uberProgram = UberProgram::create(tmpstr("%subershader.vs",pathToShaders),tmpstr("%subershader.fs",pathToShaders));

	stereoTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST);
	stereoUberProgram = UberProgram::create(tmpstr("%sstereo.vs",pathToShaders),tmpstr("%sstereo.fs",pathToShaders));

	panoramaTexture = new Texture(rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,1,1,6,rr::BF_RGB,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR);
	depthTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR);

#ifdef MIRRORS
	mirrorDepthMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
	mirrorMaskMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGB,true,RR_GHOST_BUFFER);
#endif
	recursionDepth = 0;
}

RendererOfSceneImpl::~RendererOfSceneImpl()
{
#ifdef MIRRORS
	delete mirrorMaskMap;
	delete mirrorDepthMap;
#endif
	// panorama
	if (depthTexture)
		delete depthTexture->getBuffer();
	RR_SAFE_DELETE(depthTexture);
	if (panoramaTexture)
		delete panoramaTexture->getBuffer();
	RR_SAFE_DELETE(panoramaTexture);

	// stereo
	RR_SAFE_DELETE(stereoUberProgram);
	if (stereoTexture)
		delete stereoTexture->getBuffer();
	RR_SAFE_DELETE(stereoTexture);

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

extern float getMipLevel(const rr::RRMaterial* material);

#ifdef MIRRORS
// here we map planes to mirrors, so that two objects with the same plane share mirror
struct PlaneCompare // comparing RRVec4 looks strange, so we do it here rather than in public RRVec4 operator<
{
	bool operator()(const rr::RRVec4& a, const rr::RRVec4& b) const
	{
		return a.x<b.x || (a.x==b.x && (a.y<b.y || (a.y==b.y && (a.z<b.z || (a.z==b.z && a.w<b.w)))));
	}
};
#endif

void RendererOfSceneImpl::renderToCube(rr::RRDynamicSolver* _solver, const RealtimeLights* _lights, const RenderParameters& _renderParameters, Texture* _cubeTexture)
{
	// copy, so that we can modify some parameters
	RenderParameters _ = _renderParameters;

	if (!_cubeTexture || !depthTexture)
		return;

	// resize depth
	unsigned size = _cubeTexture->getBuffer()->getWidth();
	if (depthTexture->getBuffer()->getWidth()!=size)
	{
		depthTexture->getBuffer()->reset(rr::BT_2D_TEXTURE,size,size,1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
		depthTexture->reset(false,false,false);
	}

#ifdef COPY_TO_CUBE
	// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
	// it could be faster, althout I did not see any speedup
	PreserveFlag p0(GL_SCISSOR_TEST,true);
	PreserveScissor p1;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	glScissor(viewport[0],viewport[1],size,size);
	glViewport(viewport[0],viewport[1],size,size);
#else
	// all attempts at srgb correct render to texture failed so far
	_.srgbCorrect = false;

	PreserveFBO p0;
	PreserveViewport p1;
	PreserveFlag p2(GL_SCISSOR_TEST,false);
	glViewport(0,0,size,size);
	FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,depthTexture);
#endif

	// render 6 times
	rr::RRVec3 s_viewAngles[6] = // 6x yawPitchRollRad
	{
		// if we capture empty scene with skybox, we get original skybox:
		rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
		rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
		rr::RRVec3(0,RR_PI/2,0), // BOTTOM
		rr::RRVec3(0,-RR_PI/2,0), // TOP
		rr::RRVec3(RR_PI,0,RR_PI), // BACK
		rr::RRVec3(0,0,RR_PI), // FRONT
		/* if we capture empty scene with skybox, we get sky rotated by 180 degrees:
		rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
		rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
		rr::RRVec3(0,RR_PI/2,RR_PI), // BOTTOM
		rr::RRVec3(0,-RR_PI/2,RR_PI), // TOP
		rr::RRVec3(0,0,RR_PI), // FRONT
		rr::RRVec3(RR_PI,0,RR_PI), // BACK
		// if we capture empty scene with skybox, we get sky rotated by 90 degrees:
		rr::RRVec3(0,0,RR_PI), // FRONT
		rr::RRVec3(RR_PI,0,RR_PI), // BACK
		rr::RRVec3(0,RR_PI/2,RR_PI*3/2), // BOTTOM
		rr::RRVec3(0,-RR_PI/2,RR_PI/2), // TOP
		rr::RRVec3(-RR_PI/2,0,RR_PI), // LEFT
		rr::RRVec3(RR_PI/2,0,RR_PI), // RIGHT
		*/
	};
	rr::RRCamera camera = *_.camera;
	camera.setAspect(1);
	camera.setFieldOfViewVerticalDeg(90);
	camera.setOrthogonal(false);
	camera.setScreenCenter(rr::RRVec2(0,0));
	_.camera = &camera;
	_.panoramaMode = PM_OFF;
	//_.stereoMode = SM_MONO;
	for (unsigned side=0;side<6;side++)
	{
		camera.setYawPitchRollRad(s_viewAngles[side]);
#ifndef COPY_TO_CUBE
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,_cubeTexture);
#endif
		glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
		render(_solver,_lights,_);
#ifdef COPY_TO_CUBE
		_cubeTexture->bindTexture();
		glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,0,0,viewport[0],viewport[1],size,size);
#endif
	}

#ifdef COPY_TO_CUBE
	// restore viewport after rendering panorama (it could be non-default, e.g. when enhanced sshot is enabled)
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
#endif
}

void RendererOfSceneImpl::render(rr::RRDynamicSolver* _solver, const RealtimeLights* _lights, const RenderParameters& _renderParameters)
{
	if (!_solver || !_renderParameters.camera)
	{
		RR_ASSERT(0);
		return;
	}

	// copy, so that we can modify some parameters
	RenderParameters _ = _renderParameters;

	// handle stereo modes by calling render() twice
	if (_.stereoMode!=SM_MONO && stereoUberProgram && stereoTexture)
	{
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);

		// why rendering to multisampled screen rather than 1-sampled texture?
		//  we prefer quality over minor speedup
		// why not rendering to multisampled texture?
		//  it needs extension, possible minor speedup is not worth adding extra renderpath
		// why not quad buffered rendering?
		//  because it works only with quadro/firegl, this is GPU independent
		// why not stencil buffer masked rendering to odd/even lines?
		//  because lines blur with multisampled screen (even if multisampling is disabled)
		rr::RRCamera leftEye, rightEye;
		_.camera->getStereoCameras(leftEye,rightEye);
		bool swapEyes = _.stereoMode==SM_INTERLACED_SWAP || _.stereoMode==SM_TOP_DOWN || _.stereoMode==SM_SIDE_BY_SIDE_SWAP;

		{
			// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
			// it could be faster, althout I did not see any speedup
			PreserveFlag p0(GL_SCISSOR_TEST,true);
			PreserveScissor p1;

			// render left
			GLint viewport1eye[4] = {viewport[0],viewport[1],viewport[2],viewport[3]};
			if (_.stereoMode==SM_SIDE_BY_SIDE || _.stereoMode==SM_SIDE_BY_SIDE_SWAP)
				viewport1eye[2] /= 2;
			else
				viewport1eye[3] /= 2;
			glViewport(viewport1eye[0],viewport1eye[1],viewport1eye[2],viewport1eye[3]);
			glScissor(viewport1eye[0],viewport1eye[1],viewport1eye[2],viewport1eye[3]);
			_.camera = swapEyes?&rightEye:&leftEye;
			StereoMode backup = _.stereoMode;
			_.stereoMode = SM_MONO;
			render(_solver,_lights,_);
			_.stereoMode = backup;

			// render right
			// (it does not update layers as they were already updated when rendering left eye. this could change in future, if different eyes see different objects)
			if (_.stereoMode==SM_SIDE_BY_SIDE || _.stereoMode==SM_SIDE_BY_SIDE_SWAP)
				viewport1eye[0] += viewport1eye[2];
			else
				viewport1eye[1] += viewport1eye[3];
			glViewport(viewport1eye[0],viewport1eye[1],viewport1eye[2],viewport1eye[3]);
			glScissor(viewport1eye[0],viewport1eye[1],viewport1eye[2],viewport1eye[3]);
			_.camera = swapEyes?&leftEye:&rightEye;
			_.stereoMode = SM_MONO;
			render(_solver,_lights,_);
			_.stereoMode = backup;
		}

		// composite
		if (_.stereoMode==SM_INTERLACED || _.stereoMode==SM_INTERLACED_SWAP)
		{
			// turns top-down images to interlaced
			glViewport(viewport[0],viewport[1]+(viewport[3]%2),viewport[2],viewport[3]/2*2);
			stereoTexture->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,viewport[0],viewport[1],viewport[2],viewport[3]/2*2,0);
			Program* stereoProgram = stereoUberProgram->getProgram("");
			if (stereoProgram)
			{
				stereoProgram->useIt();
				stereoProgram->sendTexture("map",stereoTexture);
				stereoProgram->sendUniform("mapHalfHeight",float(viewport[3]/2));
				glDisable(GL_CULL_FACE);
				glBegin(GL_POLYGON);
					glVertex2f(-1,-1);
					glVertex2f(-1,1);
					glVertex2f(1,1);
					glVertex2f(1,-1);
				glEnd();
			}
		}

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);

		return;
	}

	// handle panorama modes by calling render 6 times
	if (_.panoramaMode!=PM_OFF && panoramaTexture)
	{
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);

		// resize cube
		unsigned size = (unsigned)sqrtf(viewport[2]*viewport[3]/6.f+1);
		size = RR_MIN3(size,(unsigned)viewport[2],(unsigned)viewport[3]);
		if (panoramaTexture->getBuffer()->getWidth()!=size)
		{
			panoramaTexture->getBuffer()->reset(rr::BT_CUBE_TEXTURE,size,size,6,rr::BF_RGB,true,RR_GHOST_BUFFER);
			panoramaTexture->reset(false,false,false);
		}

		renderToCube(_solver,_lights,_,panoramaTexture);

		// composite
		textureRenderer->render2D(panoramaTexture,NULL,1,0,0,1,1,-1,(_.panoramaMode==PM_LITTLE_PLANET)?"#define PANORAMA_MODE 2\n":"#define PANORAMA_MODE 1\n");

		return;
	}

	// Ensure sRGB correctness.
	if (!Workaround::supportsSRGB())
		_.srgbCorrect = false;
	PreserveFlag p0(GL_FRAMEBUFFER_SRGB,_.srgbCorrect);
	if (_.srgbCorrect)
	{
		_.gamma *= 2.2f;
	}

#ifdef MIRRORS
	GLint hasAlphaBits = -1; // -1=unknown, 0=no, 1,2,4,8...=yes
	// get viewport size, so we know how large mirror textures to use
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	// here we map planes to mirrors, so that two objects with the same plane share mirror
	typedef std::map<rr::RRVec4,rr::RRBuffer*,PlaneCompare> Mirrors;
	Mirrors mirrors;
#endif

	// Will we render multiobject or individual objects?
	//unsigned lightIndirectVersion = _solver?_solver->getSolutionVersion():0;
	bool needsIndividualStaticObjectsForEverything =
		// optimized render is faster and supports rendering into shadowmaps (this will go away with colored shadows)
		!_.renderingFromThisLight

		// disabled, this forced multiobj even if user expected lightmaps from 1obj to be rendered
		// should be no loss, using multiobj in scenes with 1 object was faster only in old renderer, new renderer makes no difference
		//&& _solver->getStaticObjects().size()>1

		&& (
			// optimized render can't render LDM for more than 1 object
			(_.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP && _.layerLDM!=UINT_MAX)
			// if we are to use provided indirect, take it always from 1objects
			// ([#12] if we are to update indirect, we update and render it in 1object or multiobject, whatever is faster. so both buffers must be allocated)
			|| ((_.uberProgramSetup.LIGHT_INDIRECT_VCOLOR||_.uberProgramSetup.LIGHT_INDIRECT_MAP) && !_.updateLayers && _.layerLightmap!=UINT_MAX)
			// optimized render would look bad with single cube per-scene (sometimes such cube does not exist at all)
			|| ((_.uberProgramSetup.MATERIAL_SPECULAR && _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) && _.layerEnvironment!=UINT_MAX)
			|| ((_.uberProgramSetup.MATERIAL_DIFFUSE && _.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE) && _.layerEnvironment!=UINT_MAX)
#ifdef MIRRORS
			// optimized render would look bad without mirrors in static parts
			|| (_.uberProgramSetup.MATERIAL_DIFFUSE && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE)
			|| (_.uberProgramSetup.MATERIAL_SPECULAR && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR)
#endif
		);

	if (_.forceObjectType==1) needsIndividualStaticObjectsForEverything = true;
	if (_.forceObjectType==2) needsIndividualStaticObjectsForEverything = false;


	// Will we render opaque parts from multiobject and blended parts from 1objects?
	// It's optimizations, makes render 10x faster in diacor (25k 1objects), compared to rendering everything from 1objects.
	bool needsIndividualStaticObjectsOnlyForBlending =
		!needsIndividualStaticObjectsForEverything
		&& !_.renderingFromThisLight
		&& (
			// optimized render can't sort
			_.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && !_.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT
		);

	rr::RRReportInterval report(rr::INF3,"Rendering %s%s scene...\n",needsIndividualStaticObjectsForEverything?"1obj":"multiobj",needsIndividualStaticObjectsOnlyForBlending?"+1objblend":"");

	// Preprocess scene, gather information about shader classes. Update cubemaps. Gather mirrors that need update.
	perObjectBuffers[recursionDepth].clear();
	for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
		i->second->clear(); // saves lots of new/delete, but makes cycling over map slower, empty fields make cycle longer. needs benchmarking to make sure it's faster than plain nonBlendedFaceGroupsMap[recursionDepth].clear();
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
				if (_.uberProgramSetup.FORCE_2D_POSITION) continue;
				objects = &_solver->getDynamicObjects();
				break;
		}
		for (unsigned i=0;i<objects->size();i++)
		{
			rr::RRObject* object = (*objects)[i];
			if (object)// && !_camera->frustumCull(object))
			{
				const rr::RRMesh* mesh = object->getCollider()->getMesh();
				rr::RRObjectIllumination& illumination = object->illumination;

				PerObjectBuffers objectBuffers;
				objectBuffers.object = object;
				objectBuffers.meshRenderer = rendererOfMeshCache.getRendererOfMesh(mesh);
				rr::RRBuffer* lightIndirectVcolor = _.uberProgramSetup.LIGHT_INDIRECT_VCOLOR ? onlyVbuf(illumination.getLayer(_.layerLightmap)) : NULL;
				rr::RRBuffer* lightIndirectMap = _.uberProgramSetup.LIGHT_INDIRECT_MAP ? onlyLmap(illumination.getLayer(_.layerLightmap)) : NULL;
				objectBuffers.lightIndirectBuffer = lightIndirectVcolor?lightIndirectVcolor:lightIndirectMap;
				objectBuffers.lightIndirectDetailMap = _.uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? onlyLmap(illumination.getLayer(_.layerLDM)) : NULL;
				objectBuffers.reflectionEnvMap = (_.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR) ? onlyCube(illumination.getLayer(_.layerEnvironment)) : NULL;
#ifdef MIRRORS
				objectBuffers.mirrorColorMap = NULL;
				objectBuffers.mirrorPlane = rr::RRVec4(0);
				if ((_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE || _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR) && !onlyCube(illumination.getLayer(_.layerEnvironment)))
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
								if (material && (material->sideBits[0].renderFrom || material->sideBits[1].renderFrom)
									&& ((_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR && material->specularReflectance.color.sum()>0)
										// allocate mirror only because of diffuse reflection?
										// yes, but only if...
										|| (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE && material->diffuseReflectance.color.sum()>0
											// ...we don't have better data,
											&& !objectBuffers.lightIndirectBuffer
											// ...and plane is not small
											&& size.maxi()>=100
										)))
								{
									// does current framebuffer have A?
									if (hasAlphaBits==-1)
									{
										glGetIntegerv(GL_ALPHA_BITS,&hasAlphaBits);
										if (hasAlphaBits<0)
											hasAlphaBits = 0;
										if (!hasAlphaBits)
										{
											RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Mirror reflections disabled, current framebuffer lacks alpha channel.\n"));
										}
									}
									if (hasAlphaBits)
									{
										// find or allocate 1x1 mirror
										objectBuffers.mirrorPlane = mirrorPlane;
										Mirrors::const_iterator i = mirrors.find(mirrorPlane);
										if (i!=mirrors.end())
											objectBuffers.mirrorColorMap = i->second;
										else
										{
											objectBuffers.mirrorColorMap = rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
											mirrors.insert(std::pair<rr::RRVec4,rr::RRBuffer*>(objectBuffers.mirrorPlane,objectBuffers.mirrorColorMap));
										}

										// calculate optimal resolution for current specularModel+shininess
										unsigned mirrorW;
										unsigned mirrorH;
										if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
										{
											// mipmapped mirror should be power of two
											//    GPU generated npot mipmaps are poor, unstable (pixels slide as resolution changes),
											//    large areas of npot LIGHT_INDIRECT_MIRROR_DIFFUSE could reflect 0/0=undefined,
											//    adding texture2D(lightIndirectMirrorMap, mirrorCenterSmooth)*0.01 inside dividedByAlpha() avoids undefined, but creates perfect mirror, look wrong too
											//    +8.5 increases resolution, protects against shimmering
											float factor = RR_CLAMPED(powf(2.f,getMipLevel(material)+8.5f)/(viewport[2]+viewport[3]),0.1f,1);
											mirrorW = 1; while (mirrorW*2<=viewport[2]*factor) mirrorW *= 2;
											mirrorH = 1; while (mirrorH*2<=viewport[3]*factor) mirrorH *= 2;
										}
										else
										{
											// non-mipmapped mirror is better npot
											//    bluriness changes smoothly, not in big jumps
											float factor = RR_CLAMPED(powf(2.f,getMipLevel(material)+6.f)/(viewport[2]+viewport[3]),0.1f,1); // 0.001 instead of 0.1 would allow lowres mirror for rough surfaces. would look more rough, but suffers from terrible shimmering. it is tax for speed, user can switch to higher quality mipmapped mirrors
											mirrorW = (unsigned)(viewport[2]*factor);
											mirrorH = (unsigned)(viewport[3]*factor);
										}

										// adjust mirror resolution to be max of all optimal resolutions
										// (we might get here multiple times, with different mirrorW/H because of different shininess)
										mirrorW = RR_MAX(objectBuffers.mirrorColorMap->getWidth(),mirrorW);
										mirrorH = RR_MAX(objectBuffers.mirrorColorMap->getHeight(),mirrorH);
										//rr::RRReporter::report(rr::INF1,"%d*%d\n",mirrorW,mirrorH);
										objectBuffers.mirrorColorMap->reset(rr::BT_2D_TEXTURE,mirrorW,mirrorH,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);

										// don't break, cycle through all facegroups in plane, they might have higher shininess and increase resolution
									}
								}
							}
						}
					}
				}
#endif

				objectBuffers.objectUberProgramSetup = _.uberProgramSetup;
				if (pass==2 && objectBuffers.objectUberProgramSetup.SHADOW_MAPS>1)
					objectBuffers.objectUberProgramSetup.SHADOW_MAPS = 1; // decrease shadow quality on dynamic objects
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = _.uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && objectBuffers.reflectionEnvMap;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = _.uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR && objectBuffers.reflectionEnvMap;
#ifdef MIRRORS
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = objectBuffers.mirrorColorMap && !objectBuffers.lightIndirectBuffer && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = objectBuffers.mirrorColorMap!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = objectBuffers.mirrorColorMap && _.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS;
#else
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = false;
#endif
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_CONST = _.uberProgramSetup.LIGHT_INDIRECT_CONST && !lightIndirectVcolor && !lightIndirectMap && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE && !objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE; // keep const only if no other indirect diffuse is available
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR = lightIndirectVcolor!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR2 = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_VCOLOR_PHYSICAL = lightIndirectVcolor!=NULL && !lightIndirectVcolor->getScaled();
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP = lightIndirectMap!=NULL;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_MAP2 = false;
				objectBuffers.objectUberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = objectBuffers.lightIndirectDetailMap!=NULL;
				objectBuffers.objectUberProgramSetup.OBJECT_SPACE = object->getWorldMatrix()!=NULL;
				if (_.srgbCorrect) // we changed gamma value, it has to be enabled in shader to have effect
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
						if (material->needsBlending() && _.uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND && !_.uberProgramSetup.LIGHT_INDIRECT_ENV_REFRACT)
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
									objectBuffers.eyeDistance = (_.camera->getPosition()-center).length();
								}
							}
						}
						else
						if (!needsIndividualStaticObjectsOnlyForBlending || pass!=1)
						{
							objectWillBeRendered = true;
							UberProgramSetup fgUberProgramSetup = objectBuffers.objectUberProgramSetup;
							fgUberProgramSetup.enableUsedMaterials(material,dynamic_cast<const rr::RRMeshArrays*>(mesh));
							fgUberProgramSetup.reduceMaterials(_.uberProgramSetup);
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

				if (_.updateLayers && objectWillBeRendered)
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
						_solver->updateEnvironmentMap(&illumination,_.layerEnvironment);
					}
				}
			}
		}
	}

	// copy camera to OpenGL fixed pipeline (to be removed)
	setupForRender(*_.camera);

	PreserveCullFace p1;
	PreserveCullMode p2;
	PreserveBlend p3;     // changed by RendererOfMesh (in MultiPass)
	PreserveColorMask p4; // changed by RendererOfMesh
	PreserveDepthMask p5; // changed by RendererOfMesh

	// Render non-sorted facegroups.
	for (unsigned applyingMirrors=0;applyingMirrors<2;applyingMirrors++) // 2 passes, LIGHT_INDIRECT_MIRROR should go _after_ !LIGHT_INDIRECT_MIRROR to increase speed and reduce light leaking under walls
	{
		for (unsigned transparencyToRGB=0;transparencyToRGB<2;transparencyToRGB++) // 2 passes, MATERIAL_TRANSPARENCY_TO_RGB(e.g. window) must go _after_ !MATERIAL_TRANSPARENCY_TO_RGB(e.g. wall), otherwise in "light - wall - window" scene, window would paint to rgb shadowmap (wall does not write to rgb) and cast rgb shadow on wall
		for (ShaderFaceGroups::iterator i=nonBlendedFaceGroupsMap[recursionDepth].begin();i!=nonBlendedFaceGroupsMap[recursionDepth].end();++i)
		if (i->first.MATERIAL_TRANSPARENCY_TO_RGB == (transparencyToRGB!=0))
		if ((i->first.LIGHT_INDIRECT_MIRROR_DIFFUSE||i->first.LIGHT_INDIRECT_MIRROR_SPECULAR) == (applyingMirrors!=0))
		{
			rr::RRVector<FaceGroupRange>*& nonBlendedFaceGroups = i->second;
			if (nonBlendedFaceGroups && nonBlendedFaceGroups->size())
			{
				const UberProgramSetup& classUberProgramSetup = i->first;
				if (_.uberProgramSetup.MATERIAL_CULLING && !classUberProgramSetup.MATERIAL_CULLING)
				{
					// we are rendering with culling, but it was disabled in this class because front=back
					// setup culling at the beginning
					glDisable(GL_CULL_FACE);
				}
				MultiPass multiPass(*_.camera,_lights,_.renderingFromThisLight,classUberProgramSetup,uberProgram,&_.clipPlanes,_.srgbCorrect,&_.brightness,_.gamma);
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
						passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_.layerEnvironment));
#ifdef MIRRORS
						// set mirror
						passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorColorMap);
#endif
						// how many ranges can we render at once (the same mesh)?
						unsigned numRanges = 1;
						while (j+numRanges<nonBlendedFaceGroups->size() && (*nonBlendedFaceGroups)[j+numRanges].object==(*nonBlendedFaceGroups)[j].object) numRanges++;

						// render
						objectBuffers.meshRenderer->renderMesh(
							program,
							object,
							&(*nonBlendedFaceGroups)[j],numRanges,
							passUberProgramSetup,
							_.renderingFromThisLight?true:false,
							objectBuffers.lightIndirectBuffer,
							objectBuffers.lightIndirectDetailMap,
							_.animationTime);

						j += numRanges;
#ifdef MIRRORS
						//if (objectBuffers.mirrorColorMap)
						//{
						//	glDisable(GL_BLEND);
						//	textureRenderer->render2D(getTexture(objectBuffers.mirrorColorMap,false,false),NULL,1,0,0,0.5f,0.5f);
						//}
#endif
					}
				}
			}
		}
#ifdef MIRRORS
		// Update mirrors (after rendering all non-mirrors).
		if (!applyingMirrors && mirrors.size())
		{
			RR_ASSERT(!recursionDepth);
			recursionDepth = 1;

			RenderParameters mirror = _;
			mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_DIFFUSE = false; // Don't use mirror in mirror, to prevent update in update (infinite recursion).
			mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_SPECULAR = false;
			mirror.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS = false;
			mirror.uberProgramSetup.CLIP_PLANE = true;
			FBO oldState = FBO::getState();
			for (Mirrors::iterator i=mirrors.begin();i!=mirrors.end();++i)
			{
				glDisable(GL_BLEND);
				glEnable(GL_DEPTH_TEST);

				// clear A
				glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
				glClear(GL_COLOR_BUFFER_BIT);

#ifdef OCCLUSION_QUERY
				// occlusion query optimization: phase 1
				glBeginQuery(GL_SAMPLES_PASSED,1);
#endif
				// render shape of visible mirror pixels into A
				glDepthMask(GL_FALSE);
				UberProgramSetup mirrorMaskUberProgramSetup;
				mirrorMaskUberProgramSetup.MATERIAL_DIFFUSE = true;
				mirrorMaskUberProgramSetup.MATERIAL_CULLING = true;
				mirrorMaskUberProgramSetup.OBJECT_SPACE = true;
				Program* mirrorMaskProgram = mirrorMaskUberProgramSetup.getProgram(uberProgram);
				mirrorMaskProgram->useIt();
				for (unsigned j=0;j<perObjectBuffers[0].size();j++)
				{
					PerObjectBuffers& objectBuffers = perObjectBuffers[0][j];
					if (objectBuffers.mirrorPlane==i->first)
					{
						mirrorMaskUberProgramSetup.useWorldMatrix(mirrorMaskProgram,objectBuffers.object);
						FaceGroupRange fgrange(0, // does not have to be filled for RendererOfMesh
							0,objectBuffers.object->faceGroups.size()-1,0,objectBuffers.object->getCollider()->getMesh()->getNumTriangles());
						objectBuffers.meshRenderer->renderMesh(
							mirrorMaskProgram,
							objectBuffers.object,
							&fgrange,1,
							mirrorMaskUberProgramSetup,
							false,
							NULL,
							NULL,
							_.animationTime);
					}
				}

#ifdef OCCLUSION_QUERY
				// occlusion query optimization: phase 2
				GLuint mirrorVisible = 0;
				glEndQuery(GL_SAMPLES_PASSED);
				glGetQueryObjectuiv(1,GL_QUERY_RESULT,&mirrorVisible);
				if (!mirrorVisible)
				{
					// mirror is completely occluded, don't render mirrorColorMap, delete it
					// mirror might still be rendered later in final render, but mirrorColorMap will be NULL
					for (unsigned j=0;j<perObjectBuffers[0].size();j++)
						if (perObjectBuffers[0][j].mirrorColorMap==i->second)
							perObjectBuffers[0][j].mirrorColorMap = NULL;
					RR_SAFE_DELETE(i->second);
					continue;
				}
#endif
				// copy A to mirrorMaskMap.A
				getTexture(mirrorMaskMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE)->bindTexture();
				glCopyTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,viewport[0],viewport[1],viewport[2],viewport[3],0);

				// clear mirrorDepthMap=0, mirrorColorMap=0
				rr::RRBuffer* mirrorColorMap = i->second;
				mirrorDepthMap->reset(rr::BT_2D_TEXTURE,mirrorColorMap->getWidth(),mirrorColorMap->getHeight(),1,rr::BF_DEPTH,false,RR_GHOST_BUFFER);
				FBO::setRenderTarget(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,getTexture(mirrorDepthMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE));
				Texture* mirrorColorTex = new Texture(mirrorColorMap,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE); // new Texture instead of getTexture makes our texture deletable at the end of render()
				FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,mirrorColorTex);
				PreserveFlag p0(GL_SCISSOR_TEST,false);
				glViewport(0,0,mirrorColorMap->getWidth(),mirrorColorMap->getHeight());
				glDepthMask(GL_TRUE);
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
				// there seems to be precision problem in GF220, 285.62
				// steps to reproduce:
				//   run "sceneviewer.exe koupelna4-spectest.rr3" in window with all SV panels closed
				// what's wrong:
				//   some regions of mirror reflection (rendered in render() 20 lines below) don't receive light from second pass
				//   in other words, depth test GL_LEQUAL sometimes fails on equal Z
				//   error disappears when at least one SV panel is open
				// workaround 1:
				//   glClearDepth(0.5) (or more, not less) instead of 0
				//   -risk of culling fragments very close to near
				// workaround 2:
				//   use "gl_FragDepth = step(0.51,tex.a);" directly on uncleared buffer. this code was removed in revision 5498 to get ES compatibility
				//   -incompatible with vanilla OpenGL ES 2.0
				glClearDepth(0);
				glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
				glClearDepth(1);

				// write mirrorDepthMap=1 for pixels with mirrorMaskMap>0.7
				// we clear to 0 and then overwrite some pixels to 1 (rather than writing both in one pass) because vanilla OpenGL ES 2.0 does not have gl_FragDepth
				glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
				glDepthFunc(GL_ALWAYS); // depth test must stay enabled, otherwise depth would not be written
				glDisable(GL_CULL_FACE);
				getTextureRenderer()->render2D(getTexture(mirrorMaskMap),NULL,1,1,0,-1,1,1,"#define MIRROR_MASK_DEPTH\n"); // keeps depth test enabled
				glDepthFunc(GL_LEQUAL);

				// render scene into mirrorDepthMap, mirrorColorMap.rgb
				glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
				rr::RRVec4 mirrorPlane = i->first;
				bool cameraInFrontOfMirror = mirrorPlane.planePointDistance(_.camera->getPosition())>0;
				if (!cameraInFrontOfMirror) mirrorPlane = -mirrorPlane;
				mirrorPlane.w -= mirrorPlane.RRVec3::length()*_.camera->getFar()*1e-5f; // add bias, clip face in clipping plane, avoid reflecting mirror in itself
				rr::RRCamera mirrorCamera = *_.camera;
				mirrorCamera.mirror(mirrorPlane);
				mirrorCamera.setFar(mirrorCamera.getFar()*2); // should be far enough in majority of situations
				mirror.camera = &mirrorCamera;
				mirror.clipPlanes.clipPlane = mirrorPlane;
				// Q: how to make mirrors srgb correct?
				// A: current mirror is always srgb incorrect, srgb correct render works only into real backbuffer, not into texture.
				//    result is not affected by using GL_RGB vs GL_SRGB
				//    even if we render srgb correctly into texture(=writes encode), reads in final render will decode(=we get what we wrote in, conversion is lost)
				//    for srgb correct mirror, we would have to
				//    a) add LIGHT_INDIRECT_MIRROR_SRGB, and convert manually in shader (would increase already large number of shaders)
				//    b) make ubershader work with linear light (number of differences between correct and incorrect path would grow too much, difficult to maintain)
				//       (srgb incorrect path must remain because of OpenGL ES)
				mirror.srgbCorrect = false;
				mirror.brightness = rr::RRVec4(1);
				mirror.gamma = 1;
				render(_solver,_lights,mirror);

				// copy mirrorMaskMap to mirrorColorMap.A
				//if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
				{
					glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
					getTextureRenderer()->render2D(getTexture(mirrorMaskMap),NULL,1,1,0,-1,1,-1,"#define MIRROR_MASK_ALPHA\n"); // disables depth test
				}

				oldState.restore();
				setupForRender(*_.camera);
				glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);

				// build mirror mipmaps
				// must go after oldState.restore();, HD2400+Catalyst 12.1 sometimes (in RealtimeLights, not in SceneViewer) crashes when generating mipmaps for render target
				// mipmaps generated on GF220 are ugly, but GL_NICEST does not help (the only thing that helps is making mirror power of two)
				//glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
				if (_.uberProgramSetup.LIGHT_INDIRECT_MIRROR_MIPMAPS)
				{
					glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
					mirrorColorTex->bindTexture();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					glGenerateMipmapEXT(GL_TEXTURE_2D); // part of EXT_framebuffer_object
				}

				glDisable(GL_BLEND);
//				textureRenderer->render2D(getTexture(mirrorMaskMap,false,false),NULL,1,1,0.7f,-0.3f,0.3f,0,"#define MIRROR_MASK_DEBUG\n"); // rendered up, MIRROR_MASK_DEBUG is necessary to show alpha as rgb
//				textureRenderer->render2D(getTexture(mirrorDepthMap,false,false),NULL,1,1,0.35f,-0.3f,0.3f,0);
//				textureRenderer->render2D(getTexture(mirrorColorMap,false,false),NULL,1,1,0.0f,-0.3f,0.3f,0); // rendered down
			}
			glDepthMask(GL_TRUE);
			glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
			recursionDepth = 0;
		}
#endif
	}

	// Render skybox.
 	if (!_.renderingFromThisLight && !_.uberProgramSetup.FORCE_2D_POSITION)
	{
		rr::RRReal envAngleRad0 = 0;
		const rr::RRBuffer* env0 = _solver->getEnvironment(0,&envAngleRad0);
		if (textureRenderer && env0)
		{
			rr::RRReal envAngleRad1 = 0;
			const rr::RRBuffer* env1 = _solver->getEnvironment(1,&envAngleRad1);
			float blendFactor = _solver->getEnvironmentBlendFactor();
			Texture* texture0 = (env0->getWidth()>2)
				? getTexture(env0,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env0,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				;
			Texture* texture1 = env1 ? ( (env1->getWidth()>2)
				? getTexture(env1,false,false) // smooth, no mipmaps (would break floats, 1.2->0.2), no compression (visible artifacts)
				: getTexture(env1,false,false,GL_NEAREST,GL_NEAREST) // used by 2x2 sky
				) : NULL;
			textureRenderer->renderEnvironment(*_.camera,texture0,envAngleRad0,texture1,envAngleRad1,blendFactor,&_.brightness,_.gamma,true);
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
			fgUberProgramSetup.reduceMaterials(_.uberProgramSetup);
			fgUberProgramSetup.validate();
			MultiPass multiPass(*_.camera,_lights,_.renderingFromThisLight,fgUberProgramSetup,uberProgram,&_.clipPlanes,_.srgbCorrect,&_.brightness,_.gamma);
			UberProgramSetup passUberProgramSetup;
			RealtimeLight* light;
			Program* program;
			while (program = multiPass.getNextPass(passUberProgramSetup,light))
			{
				// set transformation
				passUberProgramSetup.useWorldMatrix(program,object);

				// set envmaps
				passUberProgramSetup.useIlluminationEnvMap(program,object->illumination.getLayer(_.layerEnvironment));
#ifdef MIRRORS
				// set mirror
				passUberProgramSetup.useIlluminationMirror(program,objectBuffers.mirrorColorMap);
#endif
				// render
				objectBuffers.meshRenderer->renderMesh(
					program,
					object,
					&blendedFaceGroups[recursionDepth][i],1,
					passUberProgramSetup,
					_.renderingFromThisLight?true:false,
					objectBuffers.lightIndirectBuffer,
					objectBuffers.lightIndirectDetailMap,
					_.animationTime);
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
