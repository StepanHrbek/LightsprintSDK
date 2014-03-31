//----------------------------------------------------------------------------
//! \file PluginToneMapping.h
//! \brief LightsprintGL | tone mapping
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2008-2014
//! All rights reserved
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
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
