//----------------------------------------------------------------------------
//! \file PluginToneMapping.h
//! \brief LightsprintGL | tone mapping adjustment
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

//! Automatically adjusts tone mapping operator (brightness parameter) to simulate eye response in time.
//!
//! Only brightness parameter is adjusted here, image in render target does not change, it's your responsibility
//! to pass brightness to renderer in next frame to achieve visible effect.
class RR_GL_API PluginParamsToneMapping : public PluginParams
{
public:
	//! Specified 'multiply screen by brightness' operator that will be adjusted.
	rr::RRVec3& brightness;
	//! Number of seconds since last call to this function.
	rr::RRReal secondsSinceLastAdjustment;
	//! Average pixel intensity we want to see after tone mapping, in 0..1 range, e.g. 0.5.
	rr::RRReal targetIntensity;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsToneMapping(const PluginParams* _next, rr::RRVec3& _brightness, rr::RRReal _secondsSinceLastAdjustment, rr::RRReal _targetIntensity) : brightness(_brightness), secondsSinceLastAdjustment(_secondsSinceLastAdjustment), targetIntensity(_targetIntensity) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const;
};

}; // namespace

#endif
