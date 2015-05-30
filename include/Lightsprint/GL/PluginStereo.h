//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
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

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Stereo plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! <table border=0><tr align=top><td>
//! \image html stereo_interlaced.png
//! </td><td>
//! \image html stereo_sidebyside.jpg
//! </td></tr><tr align=top><td>
//! \image html stereo_topdown.jpg
//! </td><td>
//! \image html stereo_oculus.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsStereo : public PluginParams
{
public:
	// For Oculus Rift only, left+right textures to render to.
	unsigned oculusW[2];
	unsigned oculusH[2];
	unsigned oculusTextureId[2];

	//! For Oculus Rift only, ovrFovPort DefaultEyeFov[2]. We type it to void* to avoid Oculus SDK dependency.
	const void* oculusTanHalfFov;

	//! Convenience ctor, for setting plugin parameters. Additional Oculus Rift parameters are cleared.
	PluginParamsStereo(const PluginParams* _next) : oculusTanHalfFov(nullptr) {next=_next; oculusW[0]=0; oculusW[1]=0; oculusH[0]=0; oculusH[1]=0; oculusTextureId[0]=0; oculusTextureId[1]=0; oculusTanHalfFov=0;}

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace

#endif
