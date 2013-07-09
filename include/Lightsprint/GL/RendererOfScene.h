//----------------------------------------------------------------------------
//! \file RendererOfScene.h
//! \brief LightsprintGL | renders contents of RRDynamicSolver instance
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDEREROFSCENE_H
#define RENDEREROFSCENE_H

#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/RealtimeLight.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// RendererOfScene


enum StereoMode
{
	SM_MONO             =0, // common non-stereo mode
	SM_INTERLACED       =2, // interlaced, with top scanline visible by right eye
	SM_INTERLACED_SWAP  =3, // interlaced, with top scanline visible by left eye
	SM_SIDE_BY_SIDE     =4, // left half is left eye
	SM_SIDE_BY_SIDE_SWAP=5, // left half is right eye
	SM_TOP_DOWN         =6, // top half is left eye
	SM_TOP_DOWN_SWAP    =7, // top half is right eye
};

//! Collection of parameters passed to renderer.
//
//! All parameters have safe default values set automatically.
//! The only parameter you always have to set is camera, default NULL won't work.
struct RenderParameters
{
	//! Specifies shader properties to be allowed during render.
	//! For rendering with all light and material features,
	//! use UberProgramSetup::enableAllLights() and UberProgramSetup::enableAllMaterials().
	UberProgramSetup uberProgramSetup;

	//! Pointer to your camera, to be used for rendering.
	const rr::RRCamera* camera;

	//! One of camera stereo modes, or SM_MONO for common non-stereo render.
	StereoMode stereoMode;

	//! When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	const rr::RRLight* renderingFromThisLight;

	//! False = renders illumination as it is stored in buffers, without updating it.
	//! True = updates illumination in _layerLightmap and _layerEnvironment layers before rendering it. Updates only outdated buffers, only buffers being rendered.
	//! Note that renderer does not allocated or deleted buffers.
	//! You can allocate buffers in advance by calling RRDynamicSolver::allocateBuffersForRealtimeGI() once.
	bool updateLayers;

	//! Indirect illumination is taken from and possibly updated in given layer.
	//! Renderer touches only existing buffers, does not allocate new ones.
	unsigned layerLightmap;

	//! Environment is taken from and possibly updated in given layer.
	//! Renderer touches only existing buffers, does not allocate new ones.
	unsigned layerEnvironment;

	//! Specifies source of light detail maps. Renderer only reads them.
	unsigned layerLDM;

	//! For diagnostic use only. 0=automatic, 1=force rendering singleobjects, 2=force rendering multiobject.
	unsigned forceObjectType;

	//! Specifies time from start of animation. At the moment, only water shader uses it for animated waves.
	float animationTime;

	//! Specifies clipping of rendered geometry.
	ClipPlanes clipPlanes;

	//! True = calculates illumination in slower but more accurate sRGB correct way.
	//! However, transparency is also calculated in sRGB, which makes it look different.
	//! Has no effect on very old GPUs that don't support sRGB textures.
	bool srgbCorrect;

	//! Specifies global brightness.
	rr::RRVec4 brightness;

	//! Specifies global gamma (contrast) factor. Default is 1 for standard pipeline, 2.2 for sRGB correct pipeline.
	float gamma;

	RenderParameters()
	{
		camera = NULL;
		stereoMode = SM_MONO;
		renderingFromThisLight = NULL;
		updateLayers = false;
		layerLightmap = UINT_MAX;
		layerEnvironment = UINT_MAX;
		layerLDM = UINT_MAX;
		forceObjectType = 0;
		animationTime = 0;
		clipPlanes.clipPlane = rr::RRVec4(1,0,0,0);
		clipPlanes.clipPlaneXA = 0;
		clipPlanes.clipPlaneXB = 0;
		clipPlanes.clipPlaneYA = 0;
		clipPlanes.clipPlaneYB = 0;
		clipPlanes.clipPlaneZA = 0;
		clipPlanes.clipPlaneZB = 0;
		srgbCorrect = false;
		brightness = rr::RRVec4(1);
		gamma = 1;
	}
};

//! OpenGL renderer of scene in RRDynamicSolver.
//
//! Renders scene from solver.
//! Takes live illumination from solver or computed illumination from layer.
class RR_GL_API RendererOfScene : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates renderer.
	//
	//! \param pathToShaders
	//!  Path to directory with shaders.
	//!  Must be terminated with slash (or be empty for current dir).
	static RendererOfScene* create(const char* pathToShaders);
	virtual ~RendererOfScene() {};

	//! Renders scene from solver into current render target and current viewport.
	//
	//! To render to texture, set render target with FBO::setRenderTarget().
	//! When rendering sRGB correctly, target texture must be sRGB,
	//! make it sRGB with Texture::reset(,,true) before FBO::setRenderTarget().
	//!
	//! Render target is not cleared automatically, so you may want to glClear() both color and depth before calling render().
	//!
	//! For correct results, render target must contain at least depth and RGB channels.
	//! When rendering with mirror reflections, render target must contain also alpha channel.
	//! Stencil buffer is not used.
	//!
	//! To render to rectangle within render target, use glViewport().
	//! (Note that when rendering with nondefault viewport, mirrors and mono camera, render target's alpha channel is cleared by glClear().
	//! If you don't want it to be cleared outside viewport, enabled scissor test with scissor area identical to viewport.)
	//!
	//! \param _solver
	//!  Source of static and dynamic objects, environment and illumination. Direct lights from solver are ignored.
	//! \param _lights
	//!  Set of lights, source of direct illumination in rendered scene.
	//! \param _renderParameters
	//!  Other rendering parameters.
	virtual void render(rr::RRDynamicSolver* _solver, const RealtimeLights* _lights, const RenderParameters& _renderParameters) = 0;


	virtual class RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh) = 0;
	virtual class TextureRenderer* getTextureRenderer() = 0;
};

}; // namespace

#endif
