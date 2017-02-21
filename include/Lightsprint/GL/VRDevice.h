//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file PluginOpenVR.h
//! \brief LightsprintGL | VR device interface
//----------------------------------------------------------------------------

#ifndef VRDEVICE_H
#define VRDEVICE_H

#include "Plugin.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Interface for VR devices

//! Common features of VR devices, e.g. positions of controllers.
//
//! By creating VR device, your program shows up on HMD and you are expected to start rendering soon.
//! You render simply by adding stereo plugin to plugin chain, which calls either oculus or openvr plugin,
//! depending on your device.
//!
//! Note that there is no need to expose functions specific for rendering
//! (see how oculus/openvr plugins render).
class RR_GL_API VRDevice
{
public:
	//! Returns HMD position and rotation.
	virtual void getPose(rr::RRVec3& outPos, rr::RRVec3& outRot) = 0;

	//! Calls getPose() and updates camera position and rotation accordingly.
	//
	//! It does not change FOV, screenCenter, eyeSeparation.
	//! It is less flexible, but simpler alternative to getPose().
	void updateCamera(rr::RRCamera& camera);
	
	//! Releases HMD, so it can be acquired by other programs.
	virtual ~VRDevice() {};
};


}; // namespace


#endif
