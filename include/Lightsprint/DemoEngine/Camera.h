// --------------------------------------------------------------------------
// DemoEngine
// Frustum with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

#include "DemoEngine.h"
//#include "Lightsprint/RRCollider.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// Camera

//! Frustum with publicly visible parameters, suitable for cameras and spotlights.
class DE_API Camera //: public rr::RRAligned
{
public:
	// inputs, to be modified by user

	//! Position of camera (imaginary frustum apex).
	float    pos[3];
	//! Rotation around Y axis, radians. For characters standing in Y axis, it controls their look to left/right.
	float    angle;
	//! Rotation around Z axis, radians. For characters looking into Z axis, it controls leaning.
	float    leanAngle;
	//! Rotation around X axis, radians, controls looking up/down.
	float    angleX;
	//! Camera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Camera's horizontal field of view in degrees. Must be positive and less than 180.
	float    fieldOfView;
	//! Camera's near plane distance in world units. Must be positive.
	float    anear;
	//! Camera's far plane distance in world units. Must be greater than near.
	float    afar;

	// outputs, to be calculated by update() and possibly read by user

	//! View direction.
	float    dir[4];
	//! Up vector.
	float    up[3];
	//! Right vector.
	float    right[3];
	//! View matrix in format suitable for OpenGL.
	double   viewMatrix[16];
	//! Inverse view matrix in format suitable for OpenGL.
	double   inverseViewMatrix[16];
	//! Projection matrix in format suitable for OpenGL.
	double   frustumMatrix[16];
	//! Inverse projection matrix in format suitable for OpenGL.
	double   inverseFrustumMatrix[16];

	// tools, to be called by user

	// ! Initializes all inputs at once.
	//Camera(float posx, float posy, float posz, float angle, float leanAngle, float angleX, float aspect, float fieldOfView, float anear, float afar);
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
	void update(float back); // converts inputs to outputs
	//! Sends our outputs to OpenGL pipeline, so that following primitives are
	//! transformed as if viewed by this camera.
	void setupForRender();
};

}; // namespace

#endif //CAMERA_H
