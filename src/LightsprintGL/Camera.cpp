// --------------------------------------------------------------------------
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include "Lightsprint/GL/Camera.h"
#include "CameraObjectDistance.h"
#include "matrix.h"

namespace rr_gl
{

////////////////////////////////////////////////////////////////////////
//
// Camera

Camera::Camera()
{
	pos = rr::RRVec3(0);
	angle = 0;
	leanAngle = 0;
	angleX = 0;
	setAspect(1);
	setFieldOfViewVerticalDeg(90);
	setRange(0.1f,100);
	orthogonal = 0;
	orthoSize = 0;
	updateDirFromAngles = true;
	origin = NULL;
	update();
}

Camera::Camera(GLfloat _posx, GLfloat _posy, GLfloat _posz, float _angle, float _leanAngle, float _angleX, float _aspect, float _fieldOfViewVerticalDeg, float _anear, float _afar)
{
	pos[0] = _posx;
	pos[1] = _posy;
	pos[2] = _posz;
	angle = _angle;
	leanAngle = _leanAngle;
	angleX = _angleX;
	setAspect(_aspect);
	setFieldOfViewVerticalDeg(_fieldOfViewVerticalDeg); // aspect must be already set
	setRange(_anear,_afar);
	orthogonal = 0;
	orthoSize = 0;
	updateDirFromAngles = true;
	origin = NULL;
	update();
}

Camera::Camera(rr::RRLight& light)
{
	pos = light.position;
	RR_ASSERT(light.type!=rr::RRLight::DIRECTIONAL || fabs(light.direction.length2()-1)<0.01f); // direction must be normalized (only for directional light)
	if (light.type==rr::RRLight::POINT)
	{
		angleX = 0;
		angle = 0;
		leanAngle = 0;
	}
	else
	{
		setDirection(light.direction);
	}
	setAspect(1);
	setFieldOfViewVerticalDeg( (light.type==rr::RRLight::SPOT) ? RR_RAD2DEG(light.outerAngleRad)*2 : 90 ); // aspect must be already set
	setRange( (light.type==rr::RRLight::DIRECTIONAL) ? 10.f : .1f, (light.type==rr::RRLight::DIRECTIONAL) ? 200.f : 100.f );
	orthogonal = (light.type==rr::RRLight::DIRECTIONAL) ? 1 : 0;
	orthoSize = 100;
	updateDirFromAngles = true;
	origin = &light;
	update();
}

void Camera::setDirection(const rr::RRVec3& _dir)
{
	dir = _dir.normalized();
	angleX = asin(dir[1]);
	if (fabs(cos(angleX))>0.0001f)
	{
		float sin_angle = dir[0]/cos(angleX);
		RR_CLAMP(sin_angle,-1,1);
		angle = asin(sin_angle);
		if (dir[2]<0) angle = (rr::RRReal)(RR_PI-angle);
	}
	else
		angle = 0;	
	leanAngle = 0;
}

void Camera::setAspect(float _aspect, float _effectOnFOV)
{
	float oldAspect = aspect;
	aspect = RR_CLAMPED(_aspect,0.001f,1000);
	if (_effectOnFOV)
	{
		double v0 = RR_DEG2RAD(fieldOfViewVerticalDeg);
		double v1 = atan(tan(v0*0.5)*oldAspect/aspect)*2;
		setFieldOfViewVerticalDeg((float)RR_RAD2DEG(v0+_effectOnFOV*(v1-v0)));
	}
}

void Camera::setFieldOfViewVerticalDeg(float _fieldOfViewVerticalDeg)
{
	fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
}

void Camera::setNear(float _near)
{
	anear = RR_MAX(0.00000001f,_near);
}

void Camera::setFar(float _far)
{
	afar = RR_MAX(anear*2,_far);
}

void Camera::setRange(float _near, float _far)
{
	anear = RR_MAX(0.00000001f,_near);
	afar = RR_MAX(anear*2,_far);
}

void Camera::setPosDirRangeRandomly(const rr::RRObject* object)
{
	// generate new values
	rr::RRVec3 newPos, newDir;
	float maxDistance;
	object->generateRandomCamera(newPos,newDir,maxDistance);
	// set them
	pos = newPos;
	setDirection(newDir);
	setRange(maxDistance/500,maxDistance);
}

rr::RRVec3 Camera::getDirection(rr::RRVec2 posInWindow) const
{
	if (orthogonal)
		return dir;
	// CameraObjectDistance uses length of our result, don't normalize
	return
		dir.RRVec3::normalized()
		+ right * ( posInWindow[0] * tan(getFieldOfViewHorizontalRad()/2) )
		- up    * ( posInWindow[1] * tan(getFieldOfViewVerticalRad()  /2) )
		;
}

void Camera::setRangeDynamically(const rr::RRObject* object, bool water, float waterLevel)
{
	if (!object)
		return;

	// get scene size
	rr::RRVec3 aabbMini,aabbMaxi;
	object->getCollider()->getMesh()->getAABB(&aabbMini,&aabbMaxi,NULL);
	float objectSize = (aabbMaxi-aabbMini).length();

	// get scene distance
	CameraObjectDistance cod(object,water,waterLevel);
	cod.addCamera(this);

	// set range
	if (cod.getDistanceMax()>=cod.getDistanceMin())
	{
		// some rays intersected scene, use scene size and distance
		float newFar = 2 * RR_MAX(objectSize,cod.getDistanceMax());
		float min = cod.getDistanceMin()/2;
		float relativeSceneProximity = RR_CLAMPED(newFar/min,10,100)*10; // 100..1000, 100=looking from distance, 1000=closeup
		float newNear = RR_CLAMPED(min,0.0001f,newFar/relativeSceneProximity);
		setRange( newNear, newFar );
	}
	else
	{
		// rays did not intersect scene, use scene size
		// or even better, don't change range at all
	}
}

bool Camera::operator==(const Camera& a) const
{
	return pos[0]==a.pos[0] && pos[1]==a.pos[1] && pos[2]==a.pos[2]
		&& angle==a.angle && leanAngle==a.leanAngle && angleX==a.angleX
		&& aspect==a.aspect && fieldOfViewVerticalDeg==a.fieldOfViewVerticalDeg
		&& anear==a.anear && afar==a.afar;
}

bool Camera::operator!=(const Camera& a) const
{
	return !(*this==a);
}

void Camera::update(const Camera* observer, float maxShadowArea)
{
	// update dir
	if (updateDirFromAngles)
	{
		dir[0] = sin(angle)*cos(angleX);
		dir[1] = sin(angleX);
		dir[2] = cos(angle)*cos(angleX);
	}

	// - leaning
	rr::RRVec3 tmpup(0,1,0);
	rr::RRVec3 tmpright;
	dir.RRVec3::normalize();
	#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
	CROSS(dir,tmpup,tmpright);
	CROSS(tmpright,dir,tmpup);
	float s = sin(leanAngle);
	float c = cos(leanAngle);
	up = (tmpup*c+tmpright*s).normalized();
	right = (tmpright*c-tmpup*s).normalized();

	// update pos (the same for all cascade steps)
	if (observer)
	{
		// update matrices
		//update(NULL,0);
		// update visible range to roughly match observer range
		orthoSize = RR_MIN(maxShadowArea,observer->afar);
		afar = orthoSize*2.6f; // light must have 2x bigger range because it goes in 2 directions from observer. for unknown reason, sponza needs at least 2.6
		anear = afar/10;
		// set new pos
		pos = observer->pos - dir*((afar-anear)*0.5f);// + observer->dir*(orthoSize*0.5f);
		/*/ adjust pos
		RRMatrix4x4 viewProjMatrix = viewMatrix*frustumMatrix;
		RRVec2 screenPos = pos * viewProjMatrix;
		RRVec3 rightDir = RRVec3(1,-1,-1)*viewProjMatrix - RRVec3(-1,-1,-1)*viewProjMatrix;
		RRVec3 upDir = RRVec3(-1,1,-1)*viewProjMatrix - RRVec3(-1,-1,-1)*viewProjMatrix;
		RRReal tmp;
		RRReal rightFix = modf(screenPos[0]*(shadowmapSize/2),&tmp)/shadowmapSize;
		RRReal upFix = modf(-screenPos[1]*(shadowmapSize/2),&tmp)/shadowmapSize;
		pos += rightFix*rightDir + upFix*upDir;*/
	}

	// update viewMatrix
	buildLookAtMatrix(viewMatrix,
		pos[0],pos[1],pos[2],
		pos[0]+dir[0],pos[1]+dir[1],pos[2]+dir[2],
		up[0], up[1], up[2]);

	// update frustumMatrix
	for (unsigned i=0;i<16;i++) frustumMatrix[i] = 0;
	if (orthogonal)
	{
		frustumMatrix[0] = 1/(orthoSize*aspect);
		frustumMatrix[5] = 1/orthoSize;
		frustumMatrix[10] = -1/(afar-anear);
		frustumMatrix[14] = -(anear+afar)/(afar-anear);
		frustumMatrix[15] = 1;
	}
	else
	{
		frustumMatrix[0] = 1/(tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2)*aspect);
		frustumMatrix[5] = 1/tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2);
		frustumMatrix[10] = -(afar+anear)/(afar-anear);
		frustumMatrix[11] = -1;
		frustumMatrix[14] = -2*anear*afar/(afar-anear);
	}

	// update inverse matrices
	invertMatrix(inverseViewMatrix, viewMatrix);
	invertMatrix(inverseFrustumMatrix, frustumMatrix);

	// copy pos/dir to RRLight
	if (origin)
	{
		origin->position = pos;
		origin->direction = dir;
	}
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
	invertMatrix(inverseViewMatrix,viewMatrix);
}

static const Camera* s_renderCamera = NULL;

const Camera* Camera::getRenderCamera()
{
	return s_renderCamera;
}

void Camera::setupForRender() const
{
	s_renderCamera = this;
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
