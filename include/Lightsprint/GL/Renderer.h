//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file Renderer.h
//! \brief LightsprintGL | plugin based renderer
//----------------------------------------------------------------------------

#ifndef RENDERER_H
#define RENDERER_H

#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRMesh.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// NamedCounter

//! Counter with ASCII name, helps plugin developers gather rendering statistics.
//
//! Plugins own counters and increment them as they wish.
//! Application reads them via Renderer::getCounters().
class RR_GL_API NamedCounter
{
public:
	unsigned count; ///< Count, zeroed at creation time, incremented by plugin, read and possibly modified by application.
	const char* name; ///< Name, set at creation time, never modified or freed.
	NamedCounter* next; ///< Pointer to next counter, nullptr for end of list.

	NamedCounter* init(const char* _name, NamedCounter* _next)
	{
		count = 0;
		name = _name;
		next = _next;
		return this;
	}
};

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
//! <b>Plugin capabilities</b> \n
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
//! <b>Plugin requirements</b> \n
//! Minimal requirements for existing plugins: OpenGL 2.0 or OpenGL ES 2.0.
//! 3.x and 4.x functions are used when available, for higher quality and speed.
//!
//! <b>How plugins call each other</b> \n
//! Each plugin is expected to call next plugin in chain at some point, but it is free to decide when to call it,
//! how many times to call it, and what plugin parameters to modify.
//! So for example
//! - PluginBloom is pretty simple, it calls next plugin first, then applies bloom postprocess.
//! - PluginScene calls next plugin after rendering opaque faces, but before rendering transparent faces.
//! - PluginStereo calls next plugin once for left eye, once for right eye.
//! - PluginCube calls next plugin 6 times, once for each side of given cubemap.
//!
//! <b>Plugin order</b> \n
//! When creating plugin chain, you are free to change order of plugins to create various effects.
//! If not sure, use this order of standard plugins:
//! - PluginParamsSky
//! - PluginParamsScene
//! - PluginParamsSSGI
//! - PluginParamsContours
//! - PluginParamsDOF
//! - PluginParamsLensFlare
//! - PluginParamsPanorama
//! - PluginParamsBloom
//! - PluginParamsStereo
//! - PluginParamsAccumulation
//! - PluginParamsToneMapping
//! - PluginParamsToneMappingAdjustment
//! - PluginParamsFPS
//! - PluginParamsShowDDI
//!
//! <b>Using plugins</b> \n
//! Example of calling renderer with plugin chain (see samples for complete code):
//! \code
//! // declare first plugin in chain, to render skybox on background
//! PluginParamsSky ppSky(nullptr,solver);
//!
//! // declare second plugin, to render objects over background
//! PluginParamsScene ppScene(&ppSky,solver);
//! ppScene.foo = bar; // (set additional parameters)
//!
//! // declare third plugin, to add SSGI postprocess on top
//! PluginParamsSSGI ppSSGI(&ppScene,1,0.3f,0.1f);
//!
//! // declare parameters shared by all plugins
//! PluginParamsShared ppShared;
//! ppShared.foo = bar; // (set additional parameters)
//!
//! // here we just run the last plugins in chain to do its work (third plugin calls second plugin etc)
//! solver->getRenderer()->render(&ppSSGI,ppShared);
//! \endcode
//!
//! <b>Creating plugins</b> \n
//! Creating new plugins is easy as well.
//! Plugins contain virtually no boilerplate code, they are just constructor, render() function and destructor.
//! For source code licensee, it is recommended to take source code of one of plugins and modify it.

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

	//! Returns named counters exposed by plugins. You can freely modify counts, e.g. zero them at the beginning of frame.
	virtual class NamedCounter* getCounters() = 0;

	virtual ~Renderer() {};
};

}; // namespace

#endif
