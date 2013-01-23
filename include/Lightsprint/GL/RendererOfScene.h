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
	//! \param _uberProgramSetup
	//!  Specifies shader properties to be allowed during render.
	//!  For rendering with all light and material features,
	//!  use UberProgramSetup::enableAllLights() and UberProgramSetup::enableAllMaterials().
	//! \param _camera
	//!  Camera scene is rendered from.
	//! \param _stereoMode
	//!  One of camera stereo modes, or SM_MONO for common non-stereo render.
	//! \param _lights
	//!  Set of lights, source of direct illumination in rendered scene.
	//! \param _renderingFromThisLight
	//!  When rendering shadows into shadowmap, set it to respective light, otherwise NULL.
	//! \param _updateLayers
	//!  False = renders illumination as it is stored in buffers, without updating it.
	//!  True = updates illumination in _layerLightmap and _layerEnvironment layers before rendering it. Updates only outdated buffers, only buffers being rendered.
	//!  Note that buffers are not allocated or deleted here.
	//!  You can allocate buffers in advance by calling RRDynamicSolver::allocateBuffersForRealtimeGI() once.
	//! \param _layerLightmap
	//!  Indirect illumination is taken from and possibly updated to given layer.
	//!  Function touches only existing buffers, does not allocate new ones.
	//! \param _layerEnvironment
	//!  Environment is taken from and possibly updated to given layer.
	//!  Function touches only existing buffers, does not allocate new ones.
	//! \param _layerLDM
	//!  Specifies source of light detail maps. Function only reads them.
	//! \param _clipPlanes
	//!  Specifies clipping of rendered geometry, pass NULL for no clipping.
	//! \param _srgbCorrect
	//!  True = calculates illumination in slower but more accurate sRGB correct way. Has no effect on very old GPUs.
	//! \param _brightness
	//!  Specifies global brightness. NULL for default brightness 1.
	//! \param _gamma
	//!  Specifies global gamma (contrast) factor. Default is 1 for standard pipeline, 2.2 for sRGB correct pipeline.
	virtual void render(
		rr::RRDynamicSolver* _solver,
		const UberProgramSetup& _uberProgramSetup,
		const rr::RRCamera& _camera,
		StereoMode _stereoMode,
		const RealtimeLights* _lights,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLayers,
		unsigned _layerLightmap,
		unsigned _layerEnvironment,
		unsigned _layerLDM,
		const ClipPlanes* _clipPlanes,
		bool _srgbCorrect,
		const rr::RRVec4* _brightness,
		float _gamma) = 0;

	virtual class RendererOfMesh* getRendererOfMesh(const rr::RRMesh* mesh) = 0;
	virtual class TextureRenderer* getTextureRenderer() = 0;
};

}; // namespace

#endif
