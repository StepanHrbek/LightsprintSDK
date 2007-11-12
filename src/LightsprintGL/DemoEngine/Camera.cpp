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
	update();
}

Camera::Camera(const rr::RRLight& light)
{
	pos = light.position;
	angleX = (light.type==rr::RRLight::SPOT) ? asin(light.direction[1]) : 0;
	angle = (light.type==rr::RRLight::SPOT && fabs(cos(angleX))>0.0001f) ? asin(light.direction[0]/cos(angleX)) : 0;
	leanAngle = 0;
	aspect = 1;
	fieldOfView = (light.type==rr::RRLight::SPOT) ? light.outerAngleRad*360/(float)M_PI : 90;
	anear = 0.5f;
	afar = 100;
	update();
}

bool Camera::operator==(const Camera& a) const
{
	return pos[0]==a.pos[0] && pos[1]==a.pos[1] && pos[2]==a.pos[2] && angle==a.angle && leanAngle==a.leanAngle && angleX==a.angleX && aspect==a.aspect && fieldOfView==a.fieldOfView && anear==a.anear && afar==a.afar;
}

void Camera::update()
{
	dir[0] = sin(angle)*cos(angleX);
	dir[1] = sin(angleX);
	dir[2] = cos(angle)*cos(angleX);
	dir[3] = 1.0;

	// leaning
	rr::RRVec3 tmpup(0,1,0);
	rr::RRVec3 tmpright;
	dir.normalize();
	#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
	CROSS(dir,tmpup,tmpright);
	float s = sin(leanAngle);
	float c = cos(leanAngle);
	up = tmpup*c+tmpright*s;
	right = tmpup*s-tmpright*c;

	buildLookAtMatrix(viewMatrix,
		pos[0],pos[1],pos[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		up[0], up[1], up[2]);
	buildPerspectiveMatrix(frustumMatrix, fieldOfView, aspect, anear, afar);
	invertMatrix(inverseViewMatrix, viewMatrix);
	invertMatrix(inverseFrustumMatrix, frustumMatrix);
}

void Camera::rotateViewMatrix(unsigned instance)
{
	RR_ASSERT(instance<6);
	switch(instance)
	{
		case 0: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]+1,pos[1],pos[2],0,1,0); break;
		case 1: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]-1,pos[1],pos[2],0,1,0); break;
		case 2: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0],pos[1]+1,pos[2],0,0,1); break;
		case 3: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0],pos[1]-1,pos[2],0,0,1); break;
		case 4: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0],pos[1],pos[2]+1,0,1,0); break;
		case 5: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0],pos[1],pos[2]-1,0,1,0); break;
/*		case 0: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],up[0],up[1],up[2]); break;
		case 1: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]-dir[0],pos[1]-dir[1],pos[2]-dir[2],-up[0],-up[1],-up[2]); break;
		case 2: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]+right[0],pos[1]+right[1],pos[2]+right[2],up[0],up[1],up[2]); break;
		case 3: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]-right[0],pos[1]-right[1],pos[2]-right[2],up[0],up[1],up[2]); break;
		case 4: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]+up[0],pos[1]+up[1],pos[2]+up[2],-dir[0],-dir[1],-dir[2]); break;
		case 5: buildLookAtMatrix(viewMatrix,pos[0],pos[1],pos[2],pos[0]-up[0],pos[1]-up[1],pos[2]-up[2],dir[0],dir[1],dir[2]); break;*/
	}
}

void Camera::setupForRender()
{
	// set matrices in GL pipeline
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(frustumMatrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(viewMatrix);
}

void Camera::moveForward(float units)
{
	pos += dir*units;
}

void Camera::moveBack(float units)
{
	pos -= dir*units;
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
