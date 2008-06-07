//----------------------------------------------------------------------------
//! \file Camera.h
//! \brief LightsprintGL | frustum with adjustable pos, dir, aspect, fov, near, far
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

#include "DemoEngine.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Camera

//! Frustum with publicly visible parameters, suitable for cameras and spotlights.
//
//! With all angles reset to 0, camera direction is (0,0,1), Z+,
//! and up vector is (0,1,0), Y+.
class RR_GL_API Camera : public rr::RRUniformlyAllocated
{
public:
	// inputs, to be modified by user

	//! Position of camera (imaginary frustum apex).
	rr::RRVec3 pos;
	//! Rotation around Y axis, radians. For characters standing in Y axis, it controls their look to the left/right. Ignored if !updateDirFromAngles.
	float    angle;
	//! Rotation around Z axis, radians. For characters looking into Z+ axis, it controls leaning. Used for up vector even if !updateDirFromAngles.
	float    leanAngle;
	//! Rotation around X axis, radians, controls looking up/down. Ignored if !updateDirFromAngles.
	float    angleX;
	//! Camera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Camera's vertical field of view in degrees. Must be positive and less than 180.
	float    fieldOfView;
	//! Camera's near plane distance in world units. Must be positive.
	float    anear;
	//! Camera's far plane distance in world units. Must be greater than near.
	float    afar;
	//! Whether camera is orthogonal, set for directional lights.
	bool     orthogonal;
	//! Only if orthogonal: World space distance between closest points projected to top and bottom of screen.
	float    orthoSize;
	//! True(default): you set angle+leanAngle+angleX, update() computes direction from angles. False: you set direction, update() doesn't touch it.
	bool     updateDirFromAngles;

	// inputs or outputs

	//! View direction. Input if !updateDirFromAngles, output if updateDirFromAngles.
	rr::RRVec4 dir;

	// outputs, to be calculated by update() and possibly read by user

	//! Up vector.
	rr::RRVec3 up;
	//! Right vector.
	rr::RRVec3 right;
	//! View matrix in format suitable for OpenGL.
	double   viewMatrix[16];
	//! Inverse view matrix in format suitable for OpenGL.
	double   inverseViewMatrix[16];
	//! Projection matrix in format suitable for OpenGL.
	double   frustumMatrix[16];
	//! Inverse projection matrix in format suitable for OpenGL.
	double   inverseFrustumMatrix[16];

	// tools, to be called by user

	//! Initializes all inputs at once.
	Camera(float posx, float posy, float posz, float angle, float leanAngle, float angleX, float aspect, float fieldOfView, float anear, float afar);
	//! Initializes all inputs from RRLight.
	Camera(const rr::RRLight& light);
	//! Sets camera direction. Doesn't have to be normalized. Alternatively, you can write directly to angles or dir, depending on updateDirFromAngles flag.
	void setDirection(const rr::RRVec3& dir);
	//! == operator, true when inputs are equal.
	bool operator==(const Camera& a) const;
	//! != operator, true when inputs differ.
	bool operator!=(const Camera& a) const;
	//! Type of moveForward, moveBackward, moveRight and moveLeft for convenient mapping to keys.
	typedef void (Camera::*Move)(float units);
	//! Moves camera to given distance in world space.
	void moveForward(float units);
	//! Moves camera to given distance in world space.
	void moveBack(float units);
	//! Moves camera to given distance in world space.
	void moveRight(float units);
	//! Moves camera to given distance in world space.
	void moveLeft(float units);
	//! Moves camera to given distance in world space.
	void moveUp(float units);
	//! Moves camera to given distance in world space.
	void moveDown(float units);
	//! Leans camera = rotates around z axis.
	void lean(float units);
	//! Mirrors camera for reflection rendering. Second call takes changes back.
	//! \param altitude Altitude of mirroring plane.
	void mirror(float altitude);
	//! Updates all outputs, recalculates them from inputs.
	//
	//! \param observer
	//!  When observer is set, camera is moved to cover area around observer.
	//!  Typically, observer is player and this is directional (sun) light with shadows;
	//!  updated sun travels with player.
	//! \param shadowmapSize
	//!  Minimum of shadowmap width and height.
	//!  Must be set when observer is set, ignored otherwise.
	void update(const Camera* observer=NULL, unsigned shadowmapSize=0);
	//! Rotates viewMatrix into one of 6 directions of point light. To be called after update().
	void rotateViewMatrix(unsigned instance);
	//! Sends our outputs to OpenGL pipeline, so that following primitives are
	//! transformed as if viewed by this camera.
	void setupForRender();
};

}; // namespace

#endif //CAMERA_H
