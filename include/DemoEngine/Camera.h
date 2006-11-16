// --------------------------------------------------------------------------
// DemoEngine
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

#include <GL/glew.h>
#include "DemoEngine.h"


/////////////////////////////////////////////////////////////////////////////
//
// Camera

class DE_API Camera
{
public:
	// inputs
	GLfloat  pos[3];
	float    angle;
	float    height;
	GLdouble aspect;
	GLdouble fieldOfView;
	GLdouble anear;
	GLdouble afar;

	// outputs
	GLfloat  dir[4];
	GLdouble viewMatrix[16];
	GLdouble inverseViewMatrix[16];
	GLdouble frustumMatrix[16];
	GLdouble inverseFrustumMatrix[16];

	// tools
	typedef void (Camera::*Move)(float units);
	void moveForward(float units);
	void moveBack(float units);
	void moveRight(float units);
	void moveLeft(float units);
	void update(float back); // converts inputs to outputs
	void setupForRender();
};

#endif //CAMERA_H
