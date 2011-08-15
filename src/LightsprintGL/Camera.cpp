// --------------------------------------------------------------------------
// Camera with adjustable pos, dir, aspect, fov, near, far.
// Copyright (C) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#if defined(_MSC_VER) && _MSC_VER>=1600
	#include <functional>
#endif
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
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	setFieldOfViewVerticalDeg(90);
	setRange(0.1f,100);
	orthogonal = 0;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
	updateDirFromAngles = true;
	origin = NULL;
	update();
}

Camera::Camera(float _posx, float _posy, float _posz, float _angle, float _leanAngle, float _angleX, float _aspect, float _fieldOfViewVerticalDeg, float _anear, float _afar)
{
	pos[0] = _posx;
	pos[1] = _posy;
	pos[2] = _posz;
	angle = _angle;
	leanAngle = _leanAngle;
	angleX = _angleX;
	aspect = _aspect; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	setFieldOfViewVerticalDeg(_fieldOfViewVerticalDeg); // aspect must be already set
	setRange(_anear,_afar);
	orthogonal = 0;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
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
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	setFieldOfViewVerticalDeg( (light.type==rr::RRLight::SPOT) ? RR_RAD2DEG(light.outerAngleRad)*2 : 90 ); // aspect must be already set
	setRange( (light.type==rr::RRLight::DIRECTIONAL) ? 10.f : .1f, (light.type==rr::RRLight::DIRECTIONAL) ? 200.f : 100.f );
	orthogonal = (light.type==rr::RRLight::DIRECTIONAL) ? 1 : 0;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
	updateDirFromAngles = true;
	origin = &light;
	update();
}

void Camera::setDirection(const rr::RRVec3& _dir)
{
	dir = _dir.normalized();
	angleX = asin(dir.y);
	float d = _dir.x*_dir.x+_dir.z*_dir.z;
	if (d)
	{
		float sin_angle = _dir.x/sqrt(d);
		angle = asin(RR_CLAMPED(sin_angle,-1,1));
		if (_dir.z<0) angle = (rr::RRReal)(RR_PI-angle);
	}
	else
	{
		// We are looking straight up or down. Keep old angle, don't reset it.
	}
	leanAngle = 0;
}

void Camera::setAspect(float _aspect, float _effectOnFOV)
{
	if (_finite(_aspect) && _aspect!=aspect) // never set NaN, != would never succeed and NaN would stay set forever (at least in 2011-01 release static x64 configuration)
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
}

void Camera::setFieldOfViewVerticalDeg(float _fieldOfViewVerticalDeg)
{
	fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
}

void Camera::setNear(float _near)
{
	anear = _near;
	if (afar<=anear) afar = anear+100;
}

void Camera::setFar(float _far)
{
	afar = _far;
	if (anear>=afar) anear = (afar>0)?afar/10:afar-1;
}

void Camera::setRange(float _near, float _far)
{
	if (_near<_far)
	{
		anear = _near;
		afar = _far;
	}
}

rr::RRVec2 Camera::getPositionInWindow(rr::RRVec3 worldPosition) const
{
	int viewport[4] = {-1,-1,2,2};
	double positionInWindow[3];
	gluProject(worldPosition[0],worldPosition[1],worldPosition[2],viewMatrix,frustumMatrix,viewport,positionInWindow,positionInWindow+1,positionInWindow+2);
	return rr::RRVec2((rr::RRReal)positionInWindow[0],(rr::RRReal)positionInWindow[1]);
}

rr::RRVec3 Camera::getRayOrigin(rr::RRVec2 posInWindow) const
{
	if (!orthogonal)
		return pos;
	return
		pos
		+ right * (posInWindow[0]+screenCenter[0]) * orthoSize * aspect
		- up    * (posInWindow[1]-screenCenter[1]) * orthoSize
		;
}

rr::RRVec3 Camera::getRayDirection(rr::RRVec2 posInWindow) const
{
	if (orthogonal)
		return dir.normalized();
	// CameraObjectDistance uses length of our result, don't normalize
	return
		dir.normalized()
		+ right * ( (posInWindow[0]+screenCenter[0]) * tan(getFieldOfViewHorizontalRad()/2) )
		- up    * ( (posInWindow[1]-screenCenter[1]) * tan(getFieldOfViewVerticalRad()  /2) )
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
	return pos[0]==a.pos[0]
		&& pos[1]==a.pos[1]
		&& pos[2]==a.pos[2]
		&& angle==a.angle
		&& leanAngle==a.leanAngle
		&& angleX==a.angleX
		//&& aspect==a.aspect ... aspect is usually just byproduct of window size, users don't want "scene was modified" just because window size changed
		&& fieldOfViewVerticalDeg==a.fieldOfViewVerticalDeg
		&& anear==a.anear
		&& afar==a.afar;
}

bool Camera::operator!=(const Camera& a) const
{
	return !(*this==a);
}

void Camera::update()
{
	// update dir
	if (updateDirFromAngles)
	{
		dir[0] = cos(angleX)*sin(angle);
		dir[1] = sin(angleX);
		dir[2] = cos(angleX)*cos(angle);
	}

	// - leaning
	rr::RRVec3 tmpup(0,1,0);
	if (fabs(fabs(dir[1])-1)<1e-7f)
	{
		// choose better start for camera up vector when looking straight up or down
		// without this code, straight up/down views would ignore angle
		tmpup[0] = sin(angle)*cos(angleX+0.1f);
		tmpup[1] = sin(angleX);
		tmpup[2] = cos(angle)*cos(angleX+0.1f);
	}
	rr::RRVec3 tmpright;
	dir.normalize();
	#define CROSS(a,b,res) res[0]=a[1]*b[2]-a[2]*b[1];res[1]=a[2]*b[0]-a[0]*b[2];res[2]=a[0]*b[1]-a[1]*b[0]
	CROSS(dir,tmpup,tmpright);
	CROSS(tmpright,dir,tmpup);
	float s = sin(leanAngle);
	float c = cos(leanAngle);
	up = (tmpup*c+tmpright*s).normalized();
	right = (tmpright*c-tmpup*s).normalized();

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
		frustumMatrix[12] = -screenCenter.x;
		frustumMatrix[13] = -screenCenter.y;
		frustumMatrix[14] = -(anear+afar)/(afar-anear);
		frustumMatrix[15] = 1;
	}
	else
	{
		frustumMatrix[0] = 1/(tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2)*aspect);
		frustumMatrix[5] = 1/tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2);
		frustumMatrix[8] = screenCenter.x;
		frustumMatrix[9] = screenCenter.y;
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

void Camera::mirror(float altitude)
{
	pos[1] = 2*altitude-pos[1];
	angleX = angleX*-1;
	screenCenter.y = -screenCenter.y;
	leanAngle = -leanAngle;
	// it is not completely mirrored, up stays {0,1,0} in update()
}


////////////////////////////////////////////////////////////////////////
//
// views

static float s_viewAngles[6][3] = // angle, angleX, leanAngle
{
	{RR_PI,-RR_PI/2,0}, // TOP
	{RR_PI,RR_PI/2,0}, // BOTTOM
	{RR_PI,0,0}, // FRONT
	{0,0,0}, // BACK
	{RR_PI/2,0,0}, // LEFT
	{-RR_PI/2,0,0}, // RIGHT
};

void Camera::setView(Camera::View view, const rr::RRObject* scene)
{
	// process RANDOM
	if (view==RANDOM)
	{
		orthogonal = false;
		if (scene)
		{
			// generate new values
			rr::RRVec3 newPos, newDir;
			float maxDistance;
			scene->generateRandomCamera(newPos,newDir,maxDistance);
			// set them
			pos = newPos;
			setDirection(newDir);
			setRange(maxDistance/500,maxDistance);
		}
		return;
	}

	// process OTHER and invalid inputs
	if (!(view==TOP || view==BOTTOM || view==FRONT || view==BACK || view==LEFT || view==RIGHT))
	{
		return;
	}

	// process TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT
	orthogonal = true;
	angle = s_viewAngles[view][0];
	angleX = s_viewAngles[view][1];
	leanAngle = s_viewAngles[view][2];
	if (scene)
	{
		rr::RRVec3 mini,maxi;
		scene->getCollider()->getMesh()->getAABB(&mini,&maxi,NULL);
		switch (view)
		{
			case TOP:
				orthoSize = RR_MAX(maxi[0]-mini[0],maxi[2]-mini[2])/2;
				//pos = rr::RRVec3((mini[0]+maxi[0])/2,maxi[1]+orthoSize,(mini[2]+maxi[2])/2);
				break;
			case BOTTOM:
				orthoSize = RR_MAX(maxi[0]-mini[0],maxi[2]-mini[2])/2;
				//pos = rr::RRVec3((mini[0]+maxi[0])/2,mini[1]-orthoSize,(mini[2]+maxi[2])/2);
				break;
			case LEFT:
				orthoSize = RR_MAX(maxi[1]-mini[1],maxi[2]-mini[2])/2;
				//pos = rr::RRVec3(mini[0]-orthoSize,(mini[1]+maxi[1])/2,(mini[2]+maxi[2])/2);
				break;
			case RIGHT:
				orthoSize = RR_MAX(maxi[1]-mini[1],maxi[2]-mini[2])/2;
				//pos = rr::RRVec3(maxi[0]+orthoSize,(mini[1]+maxi[1])/2,(mini[2]+maxi[2])/2);
				break;
			case FRONT:
				orthoSize = RR_MAX(maxi[0]-mini[0],maxi[1]-mini[1])/2;
				//pos = rr::RRVec3((mini[0]+maxi[0])/2,(mini[1]+maxi[1])/2,maxi[2]+orthoSize);
				break;
			case BACK:
				orthoSize = RR_MAX(maxi[0]-mini[0],maxi[1]-mini[1])/2;
				//pos = rr::RRVec3((mini[0]+maxi[0])/2,(mini[1]+maxi[1])/2,mini[2]-orthoSize);
			break;
		}
		// move pos to center, so that camera rotation keeps object in viewport
		// user application is advised to stop setting range dynamically
		pos = (maxi+mini)/2;
		anear = -(mini-maxi).length()/2;
		afar = -1.5f*anear;
	}
}

Camera::View Camera::getView() const
{
	if (orthogonal)
		for (View view=TOP;view<=RIGHT;view=(View)(view+1))
			if (angle==s_viewAngles[view][0] && angleX==s_viewAngles[view][1] && leanAngle==s_viewAngles[view][2])
				return view;
	return OTHER;
}


////////////////////////////////////////////////////////////////////////
//
// blend

// returns a*(1-alpha) + b*alpha;
template<class C>
C blendNormal(C a,C b,rr::RRReal alpha)
{
	// don't interpolate between identical values,
	//  results would be slightly different for different alphas (in win64, not in win32)
	//  some optimizations are enabled only when data don't change between frames (see shadowcast.cpp bool lightChanged)
	return (a==b) ? a : (a*(1-alpha) + b*alpha);
}

// returns a*(1-alpha) + b*alpha; (a and b are points on 360degree circle)
// using shortest path between a and b
rr::RRReal blendModulo(rr::RRReal a,rr::RRReal b,rr::RRReal alpha,rr::RRReal modulo)
{
	a = fmodf(a,modulo);
	b = fmodf(b,modulo);
	if (a<b-(modulo/2)) a += modulo; else
	if (a>b+(modulo/2)) a -= modulo;
	return blendNormal(a,b,alpha);
}

// linear interpolation
void Camera::blend(const Camera& a, const Camera& b, float blend)
{
	pos = blendNormal(a.pos,b.pos,blend);
	angle = blendModulo(a.angle,b.angle,blend,(float)(2*RR_PI));
	leanAngle = blendNormal(a.leanAngle,b.leanAngle,blend);
	angleX = blendNormal(a.angleX,b.angleX,blend);
	aspect = blendNormal(a.aspect,b.aspect,blend);
	fieldOfViewVerticalDeg = blendNormal(a.fieldOfViewVerticalDeg,b.fieldOfViewVerticalDeg,blend);
	anear = blendNormal(a.anear,b.anear,blend);
	afar = blendNormal(a.afar,b.afar,blend);
	orthogonal = a.orthogonal;
	orthoSize = blendNormal(a.orthoSize,b.orthoSize,blend);
	screenCenter = blendNormal(a.screenCenter,b.screenCenter,blend);
	updateDirFromAngles = a.updateDirFromAngles;
	dir = blendNormal(a.dir,b.dir,blend);
	update();
}

#if defined(_MSC_VER) && _MSC_VER>=1600

static inline float abs(float a)
{
	return fabs(a);
}

static inline rr::RRVec3 abs(rr::RRVec3 a)
{
	return a.abs();
}

static inline void ifBoth0SetBoth1(float& a,float& b)
{
	if (!a && !b) a=b=1;
}

static inline void ifBoth0SetBoth1(rr::RRVec3& a,rr::RRVec3& b)
{
	if (!a[0] && !b[0]) a[0]=b[0]=1;
	if (!a[1] && !b[1]) a[1]=b[1]=1;
	if (!a[2] && !b[2]) a[2]=b[2]=1;
}

template <class X, class Y>
static Y interpolAkima(unsigned numPoints, const X* x, std::function<Y (int)> y, X xx)
{
	// handle patological cases that can't be supported otherwise
	if (numPoints==0 || !x) return Y(0);
	if (numPoints==1) return y(0);
	
	// optimization: handle simple cases for speedup
	if (numPoints==2) return y(0)+(y(1)-y(0))*(xx-x[0])/(x[1]-x[0]);

	// find how many points we have before/after
	unsigned numPointsBeforeXx = 0;
	while (x[numPointsBeforeXx]<xx && numPointsBeforeXx<numPoints) numPointsBeforeXx++;
	unsigned numPointsAfterXx = numPoints-numPointsBeforeXx;

	// for testing only: do linear interpolation instead of Akima
	//return y[numPointsBeforeXx-1]+(y[numPointsBeforeXx]-y[numPointsBeforeXx-1])*(xx-x[numPointsBeforeXx-1])/(x[numPointsBeforeXx]-x[numPointsBeforeXx-1]);

	// create fixed size dataset
	X x0,x1,x2,x3,x4,x5;
	Y y0,y1,y2,y3,y4,y5;
	// - fill in know values
	if (numPointsBeforeXx>=3) { x0 = x[numPointsBeforeXx-3]; y0 = y(numPointsBeforeXx-3); }
	if (numPointsBeforeXx>=2) { x1 = x[numPointsBeforeXx-2]; y1 = y(numPointsBeforeXx-2); }
	if (numPointsBeforeXx>=1) { x2 = x[numPointsBeforeXx-1]; y2 = y(numPointsBeforeXx-1); }
	if (numPointsAfterXx >=1) { x3 = x[numPointsBeforeXx  ]; y3 = y(numPointsBeforeXx  ); }
	if (numPointsAfterXx >=2) { x4 = x[numPointsBeforeXx+1]; y4 = y(numPointsBeforeXx+1); }
	if (numPointsAfterXx >=3) { x5 = x[numPointsBeforeXx+2]; y5 = y(numPointsBeforeXx+2); }
	// - extrapolate missing and the most common invalid values (x[i]>=x[i+1])
	if (numPointsBeforeXx< 1 || x2>=x3) { x2 = x3*2-x4; y2 = y3*2-y4; }
	if (numPointsBeforeXx< 2 || x1>=x2) { x1 = x2*2-x3; y1 = y2*2-y3; }
	if (numPointsBeforeXx< 3 || x0>=x1) { x0 = x1*2-x2; y0 = y1*2-y2; }
	if (numPointsAfterXx < 1 || x3<=x2) { x3 = x2*2-x1; y3 = y2*2-y1; }
	if (numPointsAfterXx < 2 || x4<=x3) { x4 = x3*2-x2; y4 = y3*2-y2; }
	if (numPointsAfterXx < 3 || x5<=x4) { x5 = x4*2-x3; y5 = y4*2-y3; }

	// perform Akima interpolation
	Y m0 = (y1-y0)/(x1-x0);
	Y m1 = (y2-y1)/(x2-x1);
	Y m2 = (y3-y2)/(x3-x2);
	Y m3 = (y4-y3)/(x4-x3);
	Y m4 = (y5-y4)/(x5-x4);
	Y d0 = abs(m1-m0);
	Y d1 = abs(m2-m1);
	Y d2 = abs(m3-m2);
	Y d3 = abs(m4-m3);
	//Y z0 = (d2+d0) ? (d2*m1+d0*m2)/(d2+d0) : (m2+m1)/2;
	//Y z1 = (d3+d1) ? (d3*m2+d1*m3)/(d3+d1) : (m3+m2)/2;
	ifBoth0SetBoth1(d0,d2);
	ifBoth0SetBoth1(d1,d3);
	Y z0 = (d2*m1+d0*m2)/(d2+d0);
	Y z1 = (d3*m2+d1*m3)/(d3+d1);
	X b = x3-x2;
	X a = xx-x2;
	return y2+z0*a+(m2*3-z0*2-z1)*(a*a/b) + (z0+z1-m2*2)*(a*a*a/(b*b));
}

// Akima interpolation
void Camera::blendAkima(unsigned numCameras, const Camera** cameras, float* times, float time)
{
	// handle corner cases
	if (!numCameras || !cameras || !times)
	{
		RR_ASSERT(0);
		return;
	}
	if (numCameras==1)
	{
		*this = *cameras[0];
		return;
	}

	// init this with the most closely preceding camera (or first one if none is preceding)
	unsigned i = 1;
	while (i<numCameras && times[i]<time) i++;
	*this = *cameras[i-1];

	// interpolate
	#define BLEND_FLOAT(name) {name = interpolAkima<float,float>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	//#define BLEND_RRVEC2(name) {name = interpolAkima<float,rr::RRVec2>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	#define BLEND_RRVEC3(name) {name = interpolAkima<float,rr::RRVec3>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	#define BLEND_3FLOATS(name1,name2,name3) {rr::RRVec3 tmp = interpolAkima<float,rr::RRVec3>(numCameras,times,[&cameras](int i){return rr::RRVec3(cameras[i]->name1,cameras[i]->name2,cameras[i]->name3);},time); name1=tmp.x; name2=tmp.y; name3=tmp.z;}

	BLEND_RRVEC3(pos);
	BLEND_3FLOATS(angle,leanAngle,angleX);
	BLEND_3FLOATS(anear,afar,fieldOfViewVerticalDeg);
	BLEND_FLOAT(aspect);
	BLEND_3FLOATS(screenCenter.x,screenCenter.y,orthoSize);
	BLEND_RRVEC3(dir);
	update();
}

#else

void Camera::blendAkima(unsigned numCameras, const Camera** cameras, float* times, float time)
{
	rr::RRReporter::report(rr::WARN,"blendAkima() not yet implemented for this compiler, use VS 2010.\n");
}

#endif


////////////////////////////////////////////////////////////////////////
//
// fix

static unsigned makeFinite(float& f, float def)
{
	if (_finite(f))
		return 0;
	f = def;
	return 1;
}

static unsigned makeFinite(rr::RRVec2& v, const rr::RRVec2& def)
{
	return makeFinite(v[0],def[0])+makeFinite(v[1],def[1]);
}

static unsigned makeFinite(rr::RRVec3& v, const rr::RRVec3& def)
{
	return makeFinite(v[0],def[0])+makeFinite(v[1],def[1])+makeFinite(v[2],def[2]);
}

unsigned Camera::fixInvalidValues()
{
	unsigned numFixes =
		+ makeFinite(pos,rr::RRVec3(0))
		+ makeFinite(aspect,1)
		+ makeFinite(fieldOfViewVerticalDeg,90)
		+ makeFinite(anear,0.1f)
		+ makeFinite(afar,100)
		+ makeFinite(screenCenter,rr::RRVec2(0))
		+ makeFinite(orthoSize,100)
		+ makeFinite(orthoSize,100)
		;
	if (updateDirFromAngles)
		return numFixes
			+ makeFinite(angle,0)
			+ makeFinite(leanAngle,0)
			+ makeFinite(angleX,0);
	else
		return numFixes
			+ makeFinite(dir,rr::RRVec3(1));
}

}; // namespace
