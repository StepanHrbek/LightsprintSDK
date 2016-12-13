//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginStereo.h
//! \brief LightsprintGL | stereo render
//----------------------------------------------------------------------------

#ifndef PLUGINSTEREO_H
#define PLUGINSTEREO_H

#include "Plugin.h"
#include "VRDevice.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Stereo plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! Stereo mode is selected by rr::RRCamera::stereoMode.
//! For SM_OCULUS_RIFT and SM_OPENVR modes, stereo plugin calls specialized
//! oculus and openvr plugins, other modes are implemented internally.
//! <table border=0><tr align=top><td>
//! \image html stereo_interlaced.png
//! </td><td>
//! \image html stereo_sidebyside.jpg
//! </td><td>
//! \image html stereo_topdown.jpg
//! </td><td>
//! \image html stereo_oculus.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsStereo : public PluginParams
{
public:
	//! VR device to use for rendering in SM_OCULUS_RIFT and SM_OPENVR modes.
	VRDevice* vrDevice;

	//! Convenience ctor, for setting plugin parameters.
	PluginParamsStereo(const PluginParams* _next, VRDevice* _vrDevice) : vrDevice(_vrDevice) {next=_next;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
