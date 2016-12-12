//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file Plugin.h
//! \brief LightsprintGL | renderer plugin interface
//----------------------------------------------------------------------------

#ifndef PLUGIN_H
#define PLUGIN_H

#include "Renderer.h"
#include <array>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginCreateRuntimeParams

//! Parameters sent to plugins at creation time. They are the same for all plugins.
class RR_GL_API PluginCreateRuntimeParams
{
public:
	rr::RRString pathToShaders;
	rr::RRString pathToMaps;
	NamedCounter*& counters; ///< Renderer maintains linked list of all counters, this is its end, always nullptr. Plugins can expose their own counters by appending them here, i.e. counters = &myCounter.
	
	PluginCreateRuntimeParams(NamedCounter*& _counters) : counters(_counters) 
	{
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParams

//! Parameters sent to plugins at render time. This is base class, plugins extend it, add custom parameters.
//
//! Using plugins is very simple,
//! see rr_gl::Renderer page for details.
class RR_GL_API PluginParams
{
public:
	//! Pointer to next plugin in chain, see rr_gl::Renderer for more details.
	const PluginParams* next;
	//! Creates plugin runtime. You don't need to call it, rr_gl::Renderer manages its own set of runtimes.
	virtual class PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const = 0;
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
	std::array<unsigned,4> viewport;

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
		camera = nullptr;
		viewport = {0,0,0,0};
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
//!
//! By default, plugins are reentrant, but they can change their reentrancy level in ctor, renderer will ensure that it is not exceeded.
class RR_GL_API PluginRuntime
{
public:
	virtual void render(Renderer&, const PluginParams&, const PluginParamsShared&) = 0;
	virtual ~PluginRuntime() {};

	// reentrancy check
	PluginRuntime() {reentrancy=100; rendering=0;}
	unsigned reentrancy; // how many times render() can run at once, 1 for non reentrant, 2 for minimal level of reentrancy etc
	unsigned rendering; // how many times render() is already running
};

}; // namespace

#endif
