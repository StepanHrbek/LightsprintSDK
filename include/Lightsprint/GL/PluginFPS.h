//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginFPS.h
//! \brief LightsprintGL | fps counter and renderer
//----------------------------------------------------------------------------

#ifndef PLUGINFPS_H
#define PLUGINFPS_H

#include "Plugin.h"
#include <deque>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FPS counter

class RR_GL_API FpsCounter
{
public:
	void addFrame();
	unsigned getFps();

//protected:
	std::deque<rr::RRTime> times;
};

/////////////////////////////////////////////////////////////////////////////
//
// FPS renderer

//! Adds fps number to lower right corner.
class RR_GL_API PluginParamsFPS : public PluginParams
{
public:
	//! FPS number to be rendered at lower right corner of viewport.
	FpsCounter& fpsCounter;

	//! Render also percentile graph.
	bool percentileGraph;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsFPS(const PluginParams* _next, FpsCounter& _fpsCounter, bool _percentileGraph) : fpsCounter(_fpsCounter), percentileGraph(_percentileGraph) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
