//----------------------------------------------------------------------------
//! \file Plugin.h
//! \brief LightsprintGL | renderer plugin interface
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2011-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef PLUGIN_H
#define PLUGIN_H

#include "Renderer.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginParams

//! Parameters of single plugin, with link to next plugin in chain.
class RR_GL_API PluginParams
{
public:
	//! Pointer to next plugin in chain, see rr_gl::Renderer for more details.
	const PluginParams* next;
	//! Creates plugin runtime. You don't need to call it, rr_gl::Renderer manages its own set of runtimes.
	virtual class PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const = 0;
};

//! Parameters shared by all plugins.
//
//! This structure is passed to rr_gl::Renderer::render() together with plugin chain.
//! It contains parameters relevant for more than one plugin.
//! When plugin calls next plugin, it can pass this structure modified.
struct PluginParamsShared
{
	//! Camera we are rendering with. 
	const rr::RRCamera* camera;

	//! Viewport we are rendering to, in OpenGL format.
	//! This does not replace glViewport call, it's a copy of data passed to the most recent glViewport.
	//! Although it's never accessed in basic rendering, mirrors and other effects need it.
	unsigned viewport[4];

	//! True = calculates illumination in slower but more accurate sRGB correct way.
	//! However, transparency is also calculated in sRGB, which makes it look different.
	//! Has no effect on very old GPUs that don't support sRGB textures.
	bool srgbCorrect;

	//! Specifies global brightness.
	rr::RRVec4 brightness;

	//! Specifies global gamma (contrast) factor. Default is 1 for standard pipeline, 2.2 for sRGB correct pipeline.
	float gamma;

	PluginParamsShared()
	{
		camera = NULL;
		viewport[0] = 0;
		viewport[1] = 0;
		viewport[2] = 0;
		viewport[3] = 0;
		srgbCorrect = false;
		brightness = rr::RRVec4(1);
		gamma = 1;
	}
};

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntime

//! Runtime of single plugin, created and deleted by renderer.
//
//! Although it is possible to have two runtimes of the same type, renderer makes sure to create only one of each type and reuse them,
//! runtimes are not created and deleted often.
class RR_GL_API PluginRuntime
{
public:
	virtual void render(Renderer&, const PluginParams&, const PluginParamsShared&) = 0;
	virtual ~PluginRuntime() {};
};

}; // namespace

#endif
