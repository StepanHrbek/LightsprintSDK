// --------------------------------------------------------------------------
// DemoEngine
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cmath>
#include "Lightsprint/GL/Camera.h"
#include "matrix.h"

namespace rr_gl
{

Camera::Camera(GLfloat _posx, GLfloat _posy, GLfloat _posz, float _angle, float _leanAngle, float _angleX, float _aspect, float _fieldOfView, float _anear, float _afar)
{
	pos[0] = _posx;
	pos[1] = _posy;
	pos[2] = _posz;
	angle = _angle;
	leanAngle = _leanAngle;
	angleX = _angleX;
	aspect = _aspect;
	fieldOfView = _fieldOfView;
	anear = _anear;
	afar = _afar;
}

bool Camera::operator==(const Camera& a) const
{
	return pos[0]==a.pos[0] && pos[1]==a.pos[1] && pos[2]==a.pos[2] && angle==a.angle && leanAngle==a.leanAngle && angleX==a.angleX && aspect==a.aspect && fieldOfView==a.fieldOfView && anear==a.anear && afar==a.afar;
}

void Camera::update(float back)
{
	dir[0] = sin(angle)*cos(angleX);
	dir[1] = sin(angleX);
	dir[2] = cos(angle)*cos(angleX);
	dir[3] = 1.0;

	// leaning
	float tmpup[3] = {0,1,0};
	float tmpright[3];
	#define SIZE(a) sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2])
	float dirSizeInv = 1/SIZE(dir);
	dir[0] *= dirSizeInv;
	dir[1] *= dirSizeInv;
	dir[2] *= dirSizeInv;
	#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
	CROSS(dir,tmpup,tmpright);
	float s = sin(leanAngle);
	float c = cos(leanAngle);
	up[0] = c*tmpup[0]+s*tmpright[0];
	up[1] = c*tmpup[1]+s*tmpright[1];
	up[2] = c*tmpup[2]+s*tmpright[2];
	right[0] = s*tmpup[0]-c*tmpright[0];
	right[1] = s*tmpup[1]-c*tmpright[1];
	right[2] = s*tmpup[2]-c*tmpright[2];

	buildLookAtMatrix(viewMatrix,
		pos[0]-back*dir[0],pos[1]-back*dir[1],pos[2]-back*dir[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		up[0], up[1], up[2]);
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

void Camera::mirror(float altitude)
{
	pos[1] = 2*altitude-pos[1];
	angleX = -angleX;
	// it is not completely mirrored, up stays {0,1,0} in update()
}

}; // namespace
