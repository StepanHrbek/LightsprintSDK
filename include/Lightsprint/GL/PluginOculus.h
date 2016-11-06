//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginOculus.h
//! \brief LightsprintGL | support for Oculus SDK 1.8 devices
//----------------------------------------------------------------------------

// This used to be part of PluginStereo where it logically belongs.
// But since there is much more Oculus code than stereo code, and
// since SteamVR code might come next, it's better to separate.

#ifndef PLUGINOCULUS_H
#define PLUGINOCULUS_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Oculus device

class RR_GL_API OculusDevice
{
public:
	// interface
	virtual ~OculusDevice() {};
	virtual bool isOk() {return false;};
	virtual void updateCamera(rr::RRCamera& camera) {};
	virtual void startFrame(unsigned mirrorW, unsigned mirrorH) {};
	virtual void submitFrame() {};

	// tools
	static void initialize(); // to be called first
	static OculusDevice* create(); // can be created and deleted repeatedly, but only one at a time
	static void shutdown(); // to be called last
};


/////////////////////////////////////////////////////////////////////////////
//
// Oculus plugin

//! Renders scene (calls next plugin) twice, once for left eye, once for right eye.
//
//! <table border=0><tr align=top><td>
//! \image html stereo_oculus.jpg
//! </td></tr></table>
class RR_GL_API PluginParamsOculus : public PluginParams
{
public:
	OculusDevice* oculusDevice;

	//! Convenience ctor, for setting plugin parameters. Additional Oculus Rift parameters are cleared.
	PluginParamsOculus(const PluginParams* _next) : oculusDevice(nullptr) {next=_next; }

	//! Access to actual plugin code, called by Renderer.
	virtual PluginRuntime* createRuntime(const PluginCreateRuntimeParams& params) const;
};

}; // namespace


#endif
