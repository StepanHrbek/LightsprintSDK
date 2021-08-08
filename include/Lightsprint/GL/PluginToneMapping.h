//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginToneMapping.h
//! \brief LightsprintGL | tone mapping
//----------------------------------------------------------------------------

#ifndef PLUGINTONEMAPPING_H
#define PLUGINTONEMAPPING_H

#include "Plugin.h"

namespace rr_gl
{
	
/////////////////////////////////////////////////////////////////////////////
//
// Tonemapping plugin

//! Performs basic adjustments to output colors (brightness, contrast, saturation etc).
class RR_GL_API PluginParamsToneMapping : public PluginParams
{
public:
	//! Parameters that affect output colors.
	const ToneParameters& tp;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsToneMapping(const PluginParams* _next, const ToneParameters& _tp) : tp(_tp) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
