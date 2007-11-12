//----------------------------------------------------------------------------
//! \file Camera.h
//! \brief LightsprintGL | frustum with adjustable pos, dir, aspect, fov, near, far
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
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
	//! Rotation around Y axis, radians. For characters standing in Y axis, it controls their look to left/right.
	float    angle;
	//! Rotation around Z axis, radians. For characters looking into Z+ axis, it controls leaning.
	float    leanAngle;
	//! Rotation around X axis, radians, controls looking up/down.
	float    angleX;
	//! Camera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Camera's vertical field of view in degrees. Must be positive and less than 180.
	float    fieldOfView;
	//! Camera's near plane distance in world units. Must be positive.
	float    anear;
	//! Camera's far plane distance in world units. Must be greater than near.
	float    afar;

	// outputs, to be calculated by update() and possibly read by user

	//! View direction.
	rr::RRVec4 dir;
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
	//! == operator, true when inputs are equal
	bool operator==(const Camera& a) const;
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
	void update(); // converts inputs to outputs
	//! Rotates viewMatrix into one of 6 directions of point light. To be called after update().
	void rotateViewMatrix(unsigned instance);
	//! Sends our outputs to OpenGL pipeline, so that following primitives are
	//! transformed as if viewed by this camera.
	void setupForRender();
};

}; // namespace

#endif //CAMERA_H
