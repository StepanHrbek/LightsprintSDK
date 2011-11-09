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
#include <GL/glew.h> // setupForRender()

namespace rr_gl
{

////////////////////////////////////////////////////////////////////////
//
// constructors

Camera::Camera()
{
	light = NULL;

	// view
	pos = rr::RRVec3(0);
	yawPitchRollRad = rr::RRVec3(0);
	updateView();

	// projection
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	fieldOfViewVerticalDeg = 90;
	anear = 0.1f;
	afar = 100;
	orthogonal = false;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
	updateProjection();
}

Camera::Camera(const rr::RRVec3& _pos, const rr::RRVec3& _yawPitchRoll, float _aspect, float _fieldOfViewVerticalDeg, float _anear, float _afar)
{
	light = NULL;

	// view
	pos = _pos;
	yawPitchRollRad = _yawPitchRoll;
	updateView();

	// projection
	aspect = _aspect;
	fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
	anear = (_anear<_afar) ? _anear : 0.1f;
	afar = (_anear<_afar) ? _afar : 100;
	orthogonal = false;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
	updateProjection();
}

Camera::Camera(rr::RRLight& _light)
{
	light = &_light;

	// view
	pos = _light.position;
	yawPitchRollRad = rr::RRVec3(0);
	if (_light.type!=rr::RRLight::POINT)
	{
		RR_ASSERT(_light.type!=rr::RRLight::DIRECTIONAL || fabs(_light.direction.length2()-1)<0.01f); // direction must be normalized (only for directional light)
		setDirection(_light.direction);
	}
	else
		updateView();

	// projection
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	fieldOfViewVerticalDeg = (_light.type==rr::RRLight::SPOT) ? RR_CLAMPED(RR_RAD2DEG(_light.outerAngleRad)*2,0.0000001f,179.9f) : 90;
	anear = (_light.type==rr::RRLight::DIRECTIONAL) ? 10.f : .1f;
	afar = (_light.type==rr::RRLight::DIRECTIONAL) ? 200.f : 100.f;
	orthogonal = _light.type==rr::RRLight::DIRECTIONAL;
	orthoSize = 100;
	screenCenter = rr::RRVec2(0);
	updateProjection();
}

////////////////////////////////////////////////////////////////////////
//
// view get/setters

void Camera::setPosition(const rr::RRVec3& _pos)
{
	if (pos!=_pos)
	{
		pos = _pos;
		updateView();
	}
}

void Camera::setYawPitchRollRad(const rr::RRVec3& _yawPitchRollRad)
{
	if (yawPitchRollRad!=_yawPitchRollRad)
	{
		yawPitchRollRad = _yawPitchRollRad;
		updateView();
	}
}

void Camera::setDirection(const rr::RRVec3& _dir)
{
	rr::RRVec3 dir = _dir.normalized();
	yawPitchRollRad[1] = asin(dir.y);
	float d = _dir.x*_dir.x+_dir.z*_dir.z;
	if (d>1e-7f)
	{
		// simpler "if (d)" would make yawPitchRollRad[0] jump randomly in situations when _dir points _nearly_ up.
		// _dir often points _nearly_ up because it can be represented by angles that are clamped by RR_PI, which is not exact PI, i.e. not straight up.
		// 1e-7f epsilon does not feel like proper solution, perhaps it would be better to avoid calling us with _dir extremely close to up, but it's difficult to explain such requirement to callers.
		float sin_angle = _dir.x/sqrt(d);
		yawPitchRollRad[0] = asin(RR_CLAMPED(sin_angle,-1,1));
		if (_dir.z<0) yawPitchRollRad[0] = (rr::RRReal)(RR_PI-yawPitchRollRad[0]);
		yawPitchRollRad[0] += RR_PI;
	}
	else
	{
		// We are looking straight up or down. Keep old angle, don't reset it.
	}
	yawPitchRollRad[2] = 0;
	updateView();
}

bool Camera::manipulateViewBy(const rr::RRMatrix3x4& transformation, bool rollChangeAllowed)
{
	float oldRoll = yawPitchRollRad[2];
	rr::RRMatrix3x4 matrix(viewMatrix,false);
	matrix.setTranslation(pos);
	matrix = transformation * matrix;
	rr::RRVec3 newRot = matrix.getYawPitchRoll();
	if (abs(abs(yawPitchRollRad.x-newRot.x)-RR_PI)<RR_PI/2) // rot.x change is closer to -180 or 180 than to 0 or 360. this happens when rot.y overflows 90 or -90
		return false;
	pos = matrix.getTranslation();
	yawPitchRollRad = rr::RRVec3(newRot.x,newRot.y,rollChangeAllowed?newRot.z:oldRoll); // prevent unwanted roll distortion (yawpitch changes would accumulate rounding errors in roll)
	updateView();
	return true;
}

static float s_viewAngles[6][3] = // 6x yawPitchRollRad
{
	{0,-RR_PI/2,0}, // TOP
	{0,RR_PI/2,0}, // BOTTOM
	{0,0,0}, // FRONT
	{RR_PI,0,0}, // BACK
	{-RR_PI/2,0,0}, // LEFT
	{RR_PI/2,0,0}, // RIGHT
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
	yawPitchRollRad[0] = s_viewAngles[view][0];
	yawPitchRollRad[1] = s_viewAngles[view][1];
	yawPitchRollRad[2] = s_viewAngles[view][2];
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
	updateView();
	updateProjection();
}

Camera::View Camera::getView() const
{
	if (orthogonal)
		for (View view=TOP;view<=RIGHT;view=(View)(view+1))
			if (yawPitchRollRad[0]==s_viewAngles[view][0] && yawPitchRollRad[1]==s_viewAngles[view][1] && yawPitchRollRad[2]==s_viewAngles[view][2])
				return view;
	return OTHER;
}


////////////////////////////////////////////////////////////////////////
//
// projection get/setters

void Camera::setOrthogonal(bool _orthogonal)
{
	if (orthogonal!=_orthogonal)
	{
		orthogonal = _orthogonal;
		updateProjection();
	}
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
		updateProjection();
	}
}

void Camera::setFieldOfViewVerticalDeg(float _fieldOfViewVerticalDeg)
{
	if (_finite(_fieldOfViewVerticalDeg) && fieldOfViewVerticalDeg!=_fieldOfViewVerticalDeg)
	{
		fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
		updateProjection();
	}
}

void Camera::setNear(float _near)
{
	if (_finite(_near) && _near!=anear)
	{
		anear = _near;
		if (afar<=anear) afar = anear+100;
		updateProjection();
	}
}

void Camera::setFar(float _far)
{
	if (_finite(_far) && _far!=afar)
	{
		afar = _far;
		if (anear>=afar) anear = (afar>0)?afar/10:afar-1;
		updateProjection();
	}
}

void Camera::setRange(float _near, float _far)
{
	if (_finite(_near) && _finite(_far) && (_near!=anear || _far!=afar) && _near<_far)
	{
		anear = _near;
		afar = _far;
		updateProjection();
	}
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

void Camera::setOrthoSize(float _orthoSize)
{
	if (_finite(_orthoSize) && orthoSize!=_orthoSize && orthoSize>0)
	{
		orthoSize = _orthoSize;
		updateProjection();
	}
}

void Camera::setScreenCenter(rr::RRVec2 _screenCenter)
{
	if (_screenCenter.finite() && screenCenter!=_screenCenter)
	{
		screenCenter = _screenCenter;
		updateProjection();
	}
}


////////////////////////////////////////////////////////////////////////
//
// blending

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
	yawPitchRollRad[0] = blendModulo(a.yawPitchRollRad[0],b.yawPitchRollRad[0],blend,(float)(2*RR_PI));
	yawPitchRollRad[1] = blendNormal(a.yawPitchRollRad[1],b.yawPitchRollRad[1],blend);
	yawPitchRollRad[2] = blendNormal(a.yawPitchRollRad[2],b.yawPitchRollRad[2],blend);
	aspect = blendNormal(a.aspect,b.aspect,blend);
	fieldOfViewVerticalDeg = blendNormal(a.fieldOfViewVerticalDeg,b.fieldOfViewVerticalDeg,blend);
	anear = blendNormal(a.anear,b.anear,blend);
	afar = blendNormal(a.afar,b.afar,blend);
	orthogonal = a.orthogonal;
	orthoSize = blendNormal(a.orthoSize,b.orthoSize,blend);
	screenCenter = blendNormal(a.screenCenter,b.screenCenter,blend);
	getDirection() = blendNormal(a.getDirection(),b.getDirection(),blend);
	updateView();
	updateProjection();
}

#if defined(_MSC_VER) && _MSC_VER>=1600

//static inline float abs(float a)
//{
//	return fabs(a);
//}

static inline rr::RRVec3 abs(rr::RRVec3 a)
{
	return a.abs();
}

static inline rr::RRVec4 abs(rr::RRVec4 a)
{
	return a.abs();
}

//static inline void ifBoth0SetBoth1(float& a,float& b)
//{
//	if (!a && !b) a=b=1;
//}

static inline void ifBoth0SetBoth1(rr::RRVec3& a,rr::RRVec3& b)
{
	if (!a[0] && !b[0]) a[0]=b[0]=1;
	if (!a[1] && !b[1]) a[1]=b[1]=1;
	if (!a[2] && !b[2]) a[2]=b[2]=1;
}

static inline void ifBoth0SetBoth1(rr::RRVec4& a,rr::RRVec4& b)
{
	if (!a[0] && !b[0]) a[0]=b[0]=1;
	if (!a[1] && !b[1]) a[1]=b[1]=1;
	if (!a[2] && !b[2]) a[2]=b[2]=1;
	if (!a[3] && !b[3]) a[3]=b[3]=1;
}

static inline void minimizeDistanceModulo2PI(const rr::RRVec3& a,rr::RRVec3& b)
{
	b[0] += floor((a[0]-b[0])/(2*RR_PI)+0.5f)*(2*RR_PI);
	b[1] += floor((a[1]-b[1])/(2*RR_PI)+0.5f)*(2*RR_PI);
	b[2] += floor((a[2]-b[2])/(2*RR_PI)+0.5f)*(2*RR_PI);
}

template <class X, class Y>
static Y interpolAkima(unsigned numPoints, const X* x, std::function<Y (int)> y, X xx, bool minimizeRotations = false)
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
	// - understand values as angles in radians, move from 0.1*PI to 1.9*PI like from 0.1*PI to -0.1*PI
	if (minimizeRotations)
	{
		minimizeDistanceModulo2PI(y0,y1);
		minimizeDistanceModulo2PI(y1,y2);
		minimizeDistanceModulo2PI(y2,y3);
		minimizeDistanceModulo2PI(y3,y4);
		minimizeDistanceModulo2PI(y4,y5);
	}

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
	//#define BLEND_FLOAT(name) {name = interpolAkima<float,float>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	//#define BLEND_RRVEC2(name) {name = interpolAkima<float,rr::RRVec2>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	#define BLEND_RRVEC3(name) {name = interpolAkima<float,rr::RRVec3>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time);}
	#define BLEND_RRVEC3_ANGLES(name) {name = interpolAkima<float,rr::RRVec3>(numCameras,times,[&cameras](int i){return cameras[i]->name;},time,true);}
	#define BLEND_3FLOATS(name1,name2,name3) {rr::RRVec3 tmp = interpolAkima<float,rr::RRVec3>(numCameras,times,[&cameras](int i){return rr::RRVec3(cameras[i]->name1,cameras[i]->name2,cameras[i]->name3);},time); name1=tmp.x; name2=tmp.y; name3=tmp.z;}
	#define BLEND_4FLOATS(name1,name2,name3,name4) {rr::RRVec4 tmp = interpolAkima<float,rr::RRVec4>(numCameras,times,[&cameras](int i){return rr::RRVec4(cameras[i]->name1,cameras[i]->name2,cameras[i]->name3,cameras[i]->name4);},time); name1=tmp.x; name2=tmp.y; name3=tmp.z; name4=tmp.w;}

	BLEND_RRVEC3(pos);
	BLEND_RRVEC3_ANGLES(yawPitchRollRad);
	BLEND_3FLOATS(anear,afar,fieldOfViewVerticalDeg);
	BLEND_4FLOATS(screenCenter.x,screenCenter.y,orthoSize,aspect);
	updateView();
	updateProjection();
}

#else

void Camera::blendAkima(unsigned numCameras, const Camera** cameras, float* times, float time)
{
	rr::RRReporter::report(rr::WARN,"blendAkima() not yet implemented for this compiler, use VS 2010.\n");
}

#endif


////////////////////////////////////////////////////////////////////////
//
// other tools

rr::RRVec2 Camera::getPositionInWindow(rr::RRVec3 worldPosition) const
{
	double tmp[4],out[4];
	for (int i=0; i<4; i++) tmp[i] = worldPosition[0] * viewMatrix[0*4+i] + worldPosition[1] * viewMatrix[1*4+i] + worldPosition[2] * viewMatrix[2*4+i] + viewMatrix[3*4+i];
	for (int i=0; i<4; i++) out[i] = tmp[0] * projectionMatrix[0*4+i] + tmp[1] * projectionMatrix[1*4+i] + tmp[2] * projectionMatrix[2*4+i] + tmp[3] * projectionMatrix[3*4+i];
	return rr::RRVec2((rr::RRReal)(out[0]/out[3]),(rr::RRReal)(out[1]/out[3]));
}

rr::RRVec3 Camera::getRayOrigin(rr::RRVec2 posInWindow) const
{
	if (!orthogonal)
		return pos;
	return
		pos
		+ getRight() * (posInWindow[0]+screenCenter[0]) * orthoSize * aspect
		- getUp()    * (posInWindow[1]-screenCenter[1]) * orthoSize
		;
}

rr::RRVec3 Camera::getRayDirection(rr::RRVec2 posInWindow) const
{
	if (orthogonal)
		return getDirection();
	// CameraObjectDistance uses length of our result, don't normalize
	return
		getDirection()
		+ getRight() * ( (posInWindow[0]+screenCenter[0]) * tan(getFieldOfViewHorizontalRad()/2) )
		- getUp()    * ( (posInWindow[1]-screenCenter[1]) * tan(getFieldOfViewVerticalRad()  /2) )
		;
}

bool Camera::operator==(const Camera& a) const
{
	return pos==a.pos
		&& yawPitchRollRad==a.yawPitchRollRad
		//&& aspect==a.aspect ... aspect is usually just byproduct of window size, users don't want "scene was modified" just because window size changed
		&& fieldOfViewVerticalDeg==a.fieldOfViewVerticalDeg
		&& anear==a.anear
		&& afar==a.afar;
}

bool Camera::operator!=(const Camera& a) const
{
	return !(*this==a);
}

void Camera::mirror(float altitude)
{
	pos[1] = 2*altitude-pos[1];
	yawPitchRollRad[1] = -yawPitchRollRad[1];
	screenCenter.y = -screenCenter.y;
	yawPitchRollRad[2] = -yawPitchRollRad[2];
	// it is not completely mirrored, up stays {0,1,0} in update()
	updateView();
	updateProjection();
}

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
		+ makeFinite(yawPitchRollRad[0],0)
		+ makeFinite(yawPitchRollRad[1],0)
		+ makeFinite(yawPitchRollRad[2],0);
	if (numFixes)
	{
		updateView();
		updateProjection();
	}
	return numFixes;
}

////////////////////////////////////////////////////////////////////////
//
// update

void Camera::updateView()
{
	// pos, yawPitchRollRad -> viewMatrix, rrlight
	rr::RRMatrix3x4 view = rr::RRMatrix3x4::rotationByYawPitchRoll(yawPitchRollRad);
	for (unsigned i=0;i<4;i++)
		for (unsigned j=0;j<4;j++)
			viewMatrix[4*i+j] = (i<3) ? ((j<3)?view.m[i][j]:0) : ((j<3)?-pos.dot(view.getColumn(j)):1);
	if (light)
	{
		light->position = getPosition();
		light->direction = getDirection();
	}
}

void Camera::updateProjection()
{
	// orthoSize, aspect, screenCenter, anear, afar, fieldOfViewVerticalDeg -> projectionMatrix
	if (orthogonal)
	{
		projectionMatrix[0] = 1/(orthoSize*aspect);
		projectionMatrix[1] = 0;
		projectionMatrix[2] = 0;
		projectionMatrix[3] = 0;
		projectionMatrix[4] = 0;
		projectionMatrix[5] = 1/orthoSize;
		projectionMatrix[6] = 0;
		projectionMatrix[7] = 0;
		projectionMatrix[8] = 0;
		projectionMatrix[9] = 0;
		projectionMatrix[10] = -1/(afar-anear);
		projectionMatrix[11] = 0;
		projectionMatrix[12] = -screenCenter.x;
		projectionMatrix[13] = -screenCenter.y;
		projectionMatrix[14] = -(anear+afar)/(afar-anear);
		projectionMatrix[15] = 1;
	}
	else
	{
		projectionMatrix[0] = 1/(tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2)*aspect);
		projectionMatrix[1] = 0;
		projectionMatrix[2] = 0;
		projectionMatrix[3] = 0;
		projectionMatrix[4] = 0;
		projectionMatrix[5] = 1/tan(RR_DEG2RAD(fieldOfViewVerticalDeg)/2);
		projectionMatrix[6] = 0;
		projectionMatrix[7] = 0;
		projectionMatrix[8] = screenCenter.x;
		projectionMatrix[9] = screenCenter.y;
		projectionMatrix[10] = -(afar+anear)/(afar-anear);
		projectionMatrix[11] = -1;
		projectionMatrix[12] = 0;
		projectionMatrix[13] = 0;
		projectionMatrix[14] = -2*anear*afar/(afar-anear);
		projectionMatrix[15] = 0;
	}
}



static const Camera* s_renderCamera = NULL;

const Camera* getRenderCamera()
{
	return s_renderCamera;
}

void setupForRender(const Camera& camera)
{
	s_renderCamera = &camera;
	// set matrices in GL pipeline
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(camera.getProjectionMatrix());
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(camera.getViewMatrix());
}

}; // namespace
