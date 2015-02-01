//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginAccumulation.h
//! \brief LightsprintGL | accumulates frames, outputs average
//----------------------------------------------------------------------------

#ifndef PLUGINACCUMULATION_H
#define PLUGINACCUMULATION_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Accumulation plugin

//! Accumulates frames in time, outputs average of previous frames.
class RR_GL_API PluginParamsAccumulation : public PluginParams
{
public:
	//! When set, old images are lost, accumulation is restarted.
	bool restart;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsAccumulation(const PluginParams* _next, bool _restart) : restart(_restart) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
