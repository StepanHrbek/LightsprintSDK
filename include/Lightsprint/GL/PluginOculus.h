//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginOculus.h
//! \brief LightsprintGL | support for Oculus SDK 1.8 devices
//----------------------------------------------------------------------------

#ifndef PLUGINOCULUS_H
#define PLUGINOCULUS_H

#include "Plugin.h"
#include "VRDevice.h"

namespace rr_gl
{

//! Can be created and deleted repeatedly, but only one at a time.
RR_GL_API VRDevice* createOculusDevice();


/////////////////////////////////////////////////////////////////////////////
//
// Oculus plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! For camera in SM_OCULUS_RIFT mode, output is sent to Oculus Rift HMD.
//! All other modes fall back to mono rendering.
//! <table border=0><tr align=top><td>
//! \image html stereo_oculus.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsOculus : public PluginParams
{
public:
	VRDevice* vrDevice;

	//! Convenience ctor, for setting plugin parameters. Additional Oculus Rift parameters are cleared.
	PluginParamsOculus(const PluginParams* _next, VRDevice* _vrDevice) : vrDevice(_vrDevice) {next=_next; }

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace


#endif
