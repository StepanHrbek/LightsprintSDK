// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Generic support for VR devices.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/VRDevice.h"

namespace rr_gl
{

	void VRDevice::updateCamera(rr::RRCamera& _camera)
	{
		rr::RRVec3 pos(0),rot(0);
		getPose(pos,rot);

		// apply vr rotation to our camera
		static rr::RRVec3 oldRot(0);
		rr::RRMatrix3x4 yawWithoutVR = rr::RRMatrix3x4::rotationByAxisAngle(rr::RRVec3(0,-1,0),_camera.getYawPitchRollRad().x-oldRot.x);
		rr::RRVec3 delta = rr::RRVec3(_camera.getYawPitchRollRad().x + rot.x-oldRot.x, rot.y, rot.z) - _camera.getYawPitchRollRad();
		_camera.setYawPitchRollRad(_camera.getYawPitchRollRad()+delta);
		oldRot = rot;

		// apply vr translation to our camera
		static rr::RRVec3 oldTrans(0);
		rr::RRVec3 trans = yawWithoutVR.getTransformedDirection(pos);
		_camera.setPosition(_camera.getPosition()+trans-oldTrans);
		oldTrans = trans;
	}

}; // namespace
