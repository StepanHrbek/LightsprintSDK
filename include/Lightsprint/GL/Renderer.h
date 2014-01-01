//----------------------------------------------------------------------------
//! \file Renderer.h
//! \brief LightsprintGL | plugin based renderer
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RENDERER_H
#define RENDERER_H

#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRMesh.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// Renderer

//! Modern plugin based renderer.
//
//! Renderer itself does not issue any rendering commands, it just calls plugins.
//! Plugins can directly render and/or recursively call renderer with modified set of plugins or parameters.
//! You can easily add custom plugins to add functionality.
//! For a complete list of our plugins, see PluginParams.
//!
//! With our plugins, you can render
//! - realtime global illumination from lights, emissive materials, sky, custom sources
//! - unlimited number of realtime lights
//! - linear and area spotlights with realtime penumbra shadows
//! - sky: cubemap, cross, equirectangular, HDR, LDR, blending
//! - effects: SSGI, DOF, tone mapping, bloom, lens flare etc
//! - stereo: side-by-side, top-down, interlaced
//! - panorama: equirectangular, little planet
//! - separately enabled/disabled light features:
//!   - color
//!   - projected texture
//!   - polynomial, exponential, none or physically correct distance attenuation models
//!   - shadows with variable softness, resolution, automatically cascaded in outdoor
//! - separately enabled/disabled material features:
//!   - diffuse reflection: none, constant, per vertex, per pixel
//!   - specular reflection: none, constant, per vertex, per pixel
//!   - emission: none, constant, per pixel
//!   - transparency: none, constant, per pixel / blend or 1bit alpha keying
//!   - bump: normal map, height map with parallax mapping
//!
//! Minimal requirements: OpenGL 2.0 or OpenGL ES 2.0.
//! 3.x and 4.x functions are used when available, for higher quality and speed.
//!
//! Each plugin is expected to call next plugin in chain at some point, but it is free to decide when to call it,
//! how many times to call it, and what plugin parameters to modify.
//! So for example
//! - PluginBloom is pretty simple, it calls next plugin first, then applies bloom postprocess.
//! - PluginScene calls next plugin after rendering opaque faces, but before rendering transparent faces.
//! - PluginStereo calls next plugin once for left eye, once for right eye.
//! - PluginCube calls next plugin 6 times, once for each side of given cubemap.
//!
//! When creating plugin chain, you are free to change order of plugins to create various effects.
//! If not sure, use this order of standard plugins:
//! - PluginParamsSky
//! - PluginParamsScene
//! - PluginParamsBloom
//! - PluginParamsSSGI
//! - PluginParamsDOF
//! - PluginParamsLensFlare
//! - PluginParamsPanorama
//! - PluginParamsFPS
//! - PluginParamsStereo
//! - PluginParamsToneMapping
//! - PluginParamsShowDDI
class RR_GL_API Renderer : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates renderer.
	//
	//! \param pathToShaders
	//!  Path to directory with shaders. It is passed to plugins later, renderer itself does not use it.
	//!  Must be terminated with slash (or be empty for current dir).
	//! \param pathToMaps
	//!  Path to directory with maps. It is passed to plugins later, renderer itself does not use it.
	//!  Must be terminated with slash (or be empty for current dir).
	static Renderer* create(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps);

	//! Runs given plugins to do their work.
	//
	//! Existing plugins (all but PluginCube) render into current render target and current viewport.
	//!
	//! Existing plugins do not clear automatically, so you might want to glClear() both color and depth before calling render().
	//!
	//! To render to texture, set render target with FBO::setRenderTarget().
	//! When rendering sRGB correctly, target texture must be sRGB,
	//! make it sRGB with Texture::reset(,,true) before FBO::setRenderTarget().
	//!
	//! For correct results with existing plugins, render target must contain at least depth and RGB channels.
	//! When rendering with mirror reflections, render target must contain also alpha channel.
	//! Stencil buffer is not used.
	//!
	//! To render to rectangle within render target, use glViewport().
	//! (Note that when rendering with nondefault viewport, mirrors and mono camera, render target's alpha channel is cleared by glClear().
	//! If you don't want it to be cleared outside viewport, enable scissor test with scissor area identical to viewport.)
	virtual void render(const class PluginParams* pp, const struct PluginParamsShared& sp) = 0;

	//! Helper function, provides plugins with single preallocated texture renderer.
	virtual TextureRenderer* getTextureRenderer() = 0;

	//! Helper function, for internal use.
	virtual class RendererOfMesh* getMeshRenderer(const rr::RRMesh* mesh) = 0;

	virtual ~Renderer() {};
};

}; // namespace

#endif
