// --------------------------------------------------------------------------
// DemoEngine
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#include <cmath>
#include "DemoEngine/Camera.h"
#include "matrix.h"

void Camera::update(float back)
{
	dir[0] = 3*sin(angle);
	dir[1] = -0.3f*height;
	dir[2] = 3*cos(angle);
	dir[3] = 1.0;
	buildLookAtMatrix(viewMatrix,
		pos[0]-back*dir[0],pos[1]-back*dir[1],pos[2]-back*dir[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		0, 1, 0);
	buildPerspectiveMatrix(frustumMatrix, fieldOfView, aspect, anear, afar);
	invertMatrix(inverseViewMatrix, viewMatrix);
	invertMatrix(inverseFrustumMatrix, frustumMatrix);
}
void Camera::setupForRender()
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(frustumMatrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(viewMatrix);
}

void Camera::moveForward(float units)
{
	for(int i=0;i<3;i++) pos[i]+=dir[i]*units;
}

void Camera::moveBack(float units)
{
	for(int i=0;i<3;i++) pos[i]-=dir[i]*units;
}

void Camera::moveRight(float units)
{
	pos[0]-=dir[2]*units;
	pos[2]+=dir[0]*units;
}

void Camera::moveLeft(float units)
{
	pos[0]+=dir[2]*units;
	pos[2]-=dir[0]*units;
}
