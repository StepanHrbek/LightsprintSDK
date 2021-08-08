//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginToneMappingAdjustment.h
//! \brief LightsprintGL | tone mapping adjustment
//----------------------------------------------------------------------------

#ifndef PLUGINTONEMAPPINGADJUSTMENT_H
#define PLUGINTONEMAPPINGADJUSTMENT_H

#include "Plugin.h"

namespace rr_gl
{
	
/////////////////////////////////////////////////////////////////////////////
//
// TonemappingAdjustment plugin

//! Automatically adjusts tone mapping operator (brightness parameter) to simulate eye response in time.
//!
//! Only brightness parameter is adjusted here, image in render target does not change, it's your responsibility
//! to pass brightness to renderer in next frame to achieve visible effect.
class RR_GL_API PluginParamsToneMappingAdjustment : public PluginParams
{
public:
	//! Specified 'multiply screen by brightness' operator that will be adjusted.
	rr::RRVec3& brightness;
	//! Number of seconds since last call to this function.
	rr::RRReal secondsSinceLastAdjustment;
	//! Average pixel intensity we want to see after tone mapping, in 0..1 range, e.g. 0.5.
	rr::RRReal targetIntensity;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsToneMappingAdjustment(const PluginParams* _next, rr::RRVec3& _brightness, rr::RRReal _secondsSinceLastAdjustment, rr::RRReal _targetIntensity) : brightness(_brightness), secondsSinceLastAdjustment(_secondsSinceLastAdjustment), targetIntensity(_targetIntensity) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
