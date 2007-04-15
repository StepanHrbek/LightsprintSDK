// --------------------------------------------------------------------------
// DemoEngine
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cmath>
#include "Lightsprint/DemoEngine/Camera.h"
#include "matrix.h"

namespace de
{

void Camera::update(float back)
{
	dir[0] = sin(angle)*cos(angleX);
	dir[1] = sin(angleX);
	dir[2] = cos(angle)*cos(angleX);
	dir[3] = 1.0;

	// leaning
	float up[3] = {0,1,0};
	float right[3];
	#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
	float dirSizeInv = 1/SIZE(dir);
	dir[0] *= dirSizeInv;
	dir[1] *= dirSizeInv;
	dir[2] *= dirSizeInv;
	#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
	CROSS(dir,up,right);
	float s = sin(leanAngle);
	float c = cos(leanAngle);

	buildLookAtMatrix(viewMatrix,
		pos[0]-back*dir[0],pos[1]-back*dir[1],pos[2]-back*dir[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		c*up[0]+s*right[0], c*up[1]+s*right[1], c*up[2]+s*right[2]);
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

void Camera::moveUp(float units)
{
	pos[1]+=units;
}

void Camera::moveDown(float units)
{
	pos[1]-=units;
}

void Camera::lean(float units)
{
	leanAngle+=units;
}

void Camera::mirror()
{
	pos[1] = -pos[1];
	angleX = -angleX;
	// it is not completely mirrored, up stays {0,1,0} in update()
}

}; // namespace
