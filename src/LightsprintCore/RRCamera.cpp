// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// RRCamera, camera with stereo and panorama support.
// --------------------------------------------------------------------------

#include <cmath>
#include <set> // generateRandomCamera
#include <cfloat> // _finite in generateRandomCamera
#include "Lightsprint/RRCamera.h"
#include "Lightsprint/RRObject.h"
	#include "Lightsprint/RRSolver.h"

#ifdef RR_HAS_LAMBDAS
	#include <functional> // blendAkima
	// VS2010 crashes when compiling this in release win32, unless we set "C++ Exceptions" to "Yes with SEH exceptions"
#else
	#pragma message ( "Skipping Akima interpolation, requires C++11 compiler." )
	// not recognized by msvc 2005, 2008:
	//#warning Skipping Akima interpolation, requires C++11 compiler.
#endif

namespace rr
{

////////////////////////////////////////////////////////////////////////
//
// constructors

RRCamera::RRCamera()
{
	light = nullptr;

	// view
	pos = RRVec3(0);
	yawPitchRollRad = RRVec3(0);
	updateView(false,false);

	// projection
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	fieldOfViewVerticalDeg = 90;
	anear = 0.1f;
	afar = 100;
	orthogonal = false;
	orthoSize = 100;
	screenCenter = RRVec2(0);
	updateProjection();

	// stereo
	stereoMode = SM_MONO;
	stereoSwap = false;
	eyeSeparation = 0.08f;
	displayDistance = 0.5f;

	// panorama
	panoramaMode = PM_OFF;
	panoramaCoverage = PC_FULL;
	panoramaScale = 1;
	panoramaFisheyeFovDeg = 180;

	// dof
	apertureDiameter = 0.04f;
	dofNear = 1;
	dofFar = 1;
}

RRCamera::RRCamera(const RRVec3& _pos, const RRVec3& _yawPitchRoll, float _aspect, float _fieldOfViewVerticalDeg, float _anear, float _afar)
{
	light = nullptr;

	// view
	pos = _pos;
	yawPitchRollRad = _yawPitchRoll;
	updateView(false,false);

	// projection
	orthogonal = false;
	aspect = _aspect;
	fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
	anear = (_anear<_afar) ? _anear : 0.1f;
	afar = (_anear<_afar) ? _afar : 100;
	orthoSize = 100;
	screenCenter = RRVec2(0);
	updateProjection();

	// stereo
	stereoMode = SM_MONO;
	stereoSwap = false;
	eyeSeparation = 0.08f;
	displayDistance = 0.5f;

	// panorama
	panoramaMode = PM_OFF;
	panoramaCoverage = PC_FULL;
	panoramaScale = 1;
	panoramaFisheyeFovDeg = 180;

	// dof
	apertureDiameter = 0.04f;
	dofNear = 1;
	dofFar = 1;
}

RRCamera::RRCamera(RRLight& _light)
{
	light = &_light;

	// view
	pos = _light.position;
	yawPitchRollRad = RRVec3(0);
	if (_light.type!=RRLight::POINT)
	{
		RR_ASSERT(_light.type!=RRLight::DIRECTIONAL || fabs(_light.direction.length2()-1)<0.01f); // direction must be normalized (only for directional light)
		setDirection(_light.direction);
	}
	else
		updateView(false,false);

	// projection
	aspect = 1; // ctor must set it directly, setAspect() may fail if old aspect is NaN
	fieldOfViewVerticalDeg = (_light.type==RRLight::SPOT) ? RR_CLAMPED(RR_RAD2DEG(_light.outerAngleRad)*2,0.0000001f,179.9f) : 90;
	anear = (_light.type==RRLight::DIRECTIONAL) ? 10.f : .1f;
	afar = (_light.type==RRLight::DIRECTIONAL) ? 200.f : 100.f;
	orthogonal = _light.type==RRLight::DIRECTIONAL;
	orthoSize = 100;
	screenCenter = RRVec2(0);
	updateProjection();

	// stereo
	stereoMode = SM_MONO;
	stereoSwap = false;
	eyeSeparation = 0.08f;
	displayDistance = 0.5f;

	// panorama
	panoramaMode = PM_OFF;
	panoramaCoverage = PC_FULL;
	panoramaScale = 1;
	panoramaFisheyeFovDeg = 180;

	// dof
	apertureDiameter = 0.04f;
	dofNear = 1;
	dofFar = 1;
}

////////////////////////////////////////////////////////////////////////
//
// view get/setters

void RRCamera::setPosition(const RRVec3& _pos)
{
	if (pos!=_pos)
	{
		pos = _pos;
		updateView(true,false);
	}
}

void RRCamera::setYawPitchRollRad(const RRVec3& _yawPitchRollRad)
{
	if (yawPitchRollRad!=_yawPitchRollRad)
	{
		yawPitchRollRad = _yawPitchRollRad;
		updateView(false,true);
	}
}

void RRCamera::setDirection(const RRVec3& _dir)
{
	RRVec3 dir = _dir.normalized();
	yawPitchRollRad[1] = asin(dir.y);
	float d = _dir.x*_dir.x+_dir.z*_dir.z;
	if (d>1e-7f)
	{
		// simpler "if (d)" would make yawPitchRollRad[0] jump randomly in situations when _dir points _nearly_ up.
		// _dir often points _nearly_ up because it can be represented by angles that are clamped by RR_PI, which is not exact PI, i.e. not straight up.
		// 1e-7f epsilon does not feel like proper solution, perhaps it would be better to avoid calling us with _dir extremely close to up, but it's difficult to explain such requirement to callers.
		float sin_angle = _dir.x/sqrt(d);
		yawPitchRollRad[0] = asin(RR_CLAMPED(sin_angle,-1,1));
		if (_dir.z<0) yawPitchRollRad[0] = (RRReal)(RR_PI-yawPitchRollRad[0]);
		yawPitchRollRad[0] += RR_PI;
	}
	else
	{
		// We are looking straight up or down. Keep old angle, don't reset it.
	}
	yawPitchRollRad[2] = 0;
	updateView(false,true);
}

bool RRCamera::manipulateViewBy(const RRMatrix3x4& transformation, bool rollChangeAllowed, bool yawInversionAllowed)
{
	float oldRoll = yawPitchRollRad[2];
	RRMatrix3x4 matrix(viewMatrix,false);
	matrix.setTranslation(pos);
	matrix = transformation * matrix;
	RRVec3 newRot = matrix.getYawPitchRoll();
	if (!yawInversionAllowed && abs(abs(yawPitchRollRad.x-newRot.x)-RR_PI)<RR_PI/2) // rot.x change is closer to -180 or 180 than to 0 or 360. this happens when rot.y overflows 90 or -90
		return false;
	pos = matrix.getTranslation();
	yawPitchRollRad = RRVec3(newRot.x,newRot.y,rollChangeAllowed?newRot.z:oldRoll); // prevent unwanted roll distortion (yawpitch changes would accumulate rounding errors in roll)
	updateView(true,true);
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

// Generates position and direction suitable for automatically placed camera.
static void generateRandomCamera(const RRSolver* _solver, RRVec3& _pos, RRVec3& _dir, RRReal& _maxdist)
{
	if (!_solver)
	{
	empty:
		_pos = RRVec3(0);
		_dir = RRVec3(1,0,0);
		_maxdist = 10;
		return;
	}
	bool aborting = false;
	RRObjects objects = _solver->getObjects();
	RRObject* superObject = objects.createMultiObject(RRCollider::IT_LINEAR,aborting,-1,-1,false,0,nullptr);
	if (!superObject)
		goto empty;
	const RRMesh* superMesh = superObject->getCollider()->getMesh();
	unsigned numVertices = superMesh->getNumVertices();
	RRVec3 mini,maxi,center;
	_solver->getAABB(&mini,&maxi,&center);
	_maxdist = (maxi-mini).length();
	if (!_maxdist) _maxdist = 10;
	unsigned bestNumFaces = 0;
	RRRay ray;
	const RRCollider* collider = _solver->getCollider();
	std::set<unsigned> hitTriangles;
	for (unsigned i=0;i<20;i++)
	{
		// generate random pos+dir
		RRVec3 pos;
		superMesh->getVertex(rand()%numVertices,pos);
		for (unsigned j=0;j<3;j++)
			if (!_finite(pos[j]))
				pos[j] = mini[j] + (maxi[j]-mini[j])*(rand()/float(RAND_MAX));
		RRVec3 dir = (center-pos).normalizedSafe();
		pos += dir*_maxdist* ((rand()-RAND_MAX/2)/(RAND_MAX*1.f)); // -0.5 .. 0.5

		// measure quality (=number of unique triangles hit by 100 rays)
		hitTriangles.clear();
		for (unsigned j=0;j<100;j++)
		{
			ray.rayOrigin = pos;
			ray.rayDir = ( dir + RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX))-RRVec3(0.5f) ).normalized();
			ray.rayLengthMax = _maxdist;
			ray.rayFlags = RRRay::FILL_TRIANGLE|RRRay::FILL_SIDE;
			ray.hitObject = _solver->getMultiObject();
			if (collider->intersect(ray))
			{
				const RRMaterial* material = ray.hitObject->getTriangleMaterial(ray.hitTriangle,nullptr,nullptr);
				if ((material && material->sideBits[ray.hitFrontSide?0:1].renderFrom) || (!material && ray.hitFrontSide))
					hitTriangles.insert(ray.hitTriangle);
			}
		}
		if (hitTriangles.size()>=bestNumFaces)
		{
			bestNumFaces = (unsigned)hitTriangles.size();
			_pos = pos;
			_dir = dir;
		}
	}
	delete superObject;
}

void RRCamera::setView(RRCamera::View _view, const RRSolver* _solver, const RRVec3* _mini, const RRVec3* _maxi)
{
	// find out extents
	RRVec3 mini(0),maxi(0);
	if (_solver)
	{
		_solver->getAABB(&mini,&maxi,nullptr);
	}
	else
	if (_mini && _maxi && *_mini!=*_maxi)
	{
		mini = *_mini;
		maxi = *_maxi;
	}
	bool knownExtents = (maxi-mini).mini()>=0 && (maxi-mini).sum()>0;

	// configure camera
	switch (_view)
	{
		case RANDOM:
			orthogonal = false;
			if (_solver)
			{
				// generate new values
				RRVec3 newPos, newDir;
				float maxDistance;
				generateRandomCamera(_solver,newPos,newDir,maxDistance);
				// set them
				pos = newPos;
				setDirection(newDir);
				setRange(maxDistance/500,maxDistance);
			}
			break;
		case EXTENTS:
			if (knownExtents)
			{
				pos = (maxi+mini)/2;
				if (orthogonal)
				{
					orthoSize = (maxi-mini).maxi()/2;
					pos -= getRight() * (getScreenCenter().x * orthoSize) + getUp() * (getScreenCenter().y * orthoSize);
					afar = (maxi-mini).length();
					anear = -afar;
				}
				else
				{
					// 0.2 is compensation for the worst corner case, scene made of single axis aligned cube
					float dist = (maxi-mini).length() * ( 0.2f + 1 / (RRVec2(tan(getFieldOfViewHorizontalRad()/2),tan(getFieldOfViewVerticalRad()/2)).mini()*2) );
					pos -= (
						getDirection() +
						getRight() * (getScreenCenter().x * tan(getFieldOfViewHorizontalRad()/2)) +
						getUp() * (getScreenCenter().y * tan(getFieldOfViewVerticalRad()/2))
						) * dist;
					afar = dist + (maxi-mini).length();
					anear = afar/1000;
				}
				updateView(true,true);
				updateProjection();
			}
			break;
		case TOP:
		case BOTTOM:
		case FRONT:
		case BACK:
		case LEFT:
		case RIGHT:
			yawPitchRollRad[0] = s_viewAngles[_view][0];
			yawPitchRollRad[1] = s_viewAngles[_view][1];
			yawPitchRollRad[2] = s_viewAngles[_view][2];
			if (knownExtents)
			{
				orthogonal = true;
				switch (_view)
				{
					case TOP:
						orthoSize = RR_MAX(maxi[0]-mini[0],maxi[2]-mini[2])/2;
						//pos = RRVec3((mini[0]+maxi[0])/2,maxi[1]+orthoSize,(mini[2]+maxi[2])/2);
						break;
					case BOTTOM:
						orthoSize = RR_MAX(maxi[0]-mini[0],maxi[2]-mini[2])/2;
						//pos = RRVec3((mini[0]+maxi[0])/2,mini[1]-orthoSize,(mini[2]+maxi[2])/2);
						break;
					case LEFT:
						orthoSize = RR_MAX(maxi[1]-mini[1],maxi[2]-mini[2])/2;
						//pos = RRVec3(mini[0]-orthoSize,(mini[1]+maxi[1])/2,(mini[2]+maxi[2])/2);
						break;
					case RIGHT:
						orthoSize = RR_MAX(maxi[1]-mini[1],maxi[2]-mini[2])/2;
						//pos = RRVec3(maxi[0]+orthoSize,(mini[1]+maxi[1])/2,(mini[2]+maxi[2])/2);
						break;
					case FRONT:
						orthoSize = RR_MAX(maxi[0]-mini[0],maxi[1]-mini[1])/2;
						//pos = RRVec3((mini[0]+maxi[0])/2,(mini[1]+maxi[1])/2,maxi[2]+orthoSize);
						break;
					case BACK:
						orthoSize = RR_MAX(maxi[0]-mini[0],maxi[1]-mini[1])/2;
						//pos = RRVec3((mini[0]+maxi[0])/2,(mini[1]+maxi[1])/2,mini[2]-orthoSize);
						break;
					default:
						// should not get here, just prevents warning
						break;
				}
				// move pos to center, so that camera rotation keeps object in viewport
				// user application is advised to stop setting range dynamically
				pos = (maxi+mini)/2;
				anear = -(mini-maxi).length()/2;
				afar = -1.5f*anear;
			}
			updateView(true,true);
			updateProjection();
			break;
		default:
			// should not get here, just prevents warning
			break;
	}
}

RRCamera::View RRCamera::getView() const
{
	if (orthogonal)
		for (View view=TOP;view<=RIGHT;view=(View)(view+1))
			if (yawPitchRollRad[0]==s_viewAngles[view][0] && yawPitchRollRad[1]==s_viewAngles[view][1] && yawPitchRollRad[2]==s_viewAngles[view][2])
				return view;
	return OTHER;
}

void RRCamera::setView(const RRVec3& _pos, const RRVec3& _yawPitchRollRad)
{
	pos = _pos;
	yawPitchRollRad = _yawPitchRollRad;
	updateView(true,true);
}


////////////////////////////////////////////////////////////////////////
//
// projection get/setters

void RRCamera::setOrthogonal(bool _orthogonal)
{
	if (orthogonal!=_orthogonal)
	{
		orthogonal = _orthogonal;
		updateProjection();
	}
}

void RRCamera::setAspect(float _aspect, float _effectOnFOV)
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

void RRCamera::setFieldOfViewVerticalDeg(float _fieldOfViewVerticalDeg)
{
	if (_finite(_fieldOfViewVerticalDeg) && fieldOfViewVerticalDeg!=_fieldOfViewVerticalDeg)
	{
		fieldOfViewVerticalDeg = RR_CLAMPED(_fieldOfViewVerticalDeg,0.0000001f,179.9f);
		updateProjection();
	}
}

void RRCamera::setNear(float _near)
{
	if (_finite(_near) && _near!=anear)
	{
		anear = _near;
		if (afar<=anear) afar = anear+100;
		updateProjection();
	}
}

void RRCamera::setFar(float _far)
{
	if (_finite(_far) && _far!=afar)
	{
		afar = _far;
		if (anear>=afar) anear = (afar>0)?afar/10:afar-1;
		updateProjection();
	}
}

void RRCamera::setRange(float _near, float _far)
{
	if (_finite(_near) && _finite(_far) && (_near!=anear || _far!=afar) && _near<_far)
	{
		anear = _near;
		afar = _far;
		updateProjection();
	}
}

void RRCamera::setOrthoSize(float _orthoSize)
{
	if (_finite(_orthoSize) && orthoSize!=_orthoSize && _orthoSize>0)
	{
		orthoSize = _orthoSize;
		updateProjection();
	}
}

void RRCamera::setScreenCenter(RRVec2 _screenCenter)
{
	if (_screenCenter.finite() && screenCenter!=_screenCenter)
	{
		screenCenter = _screenCenter;
		updateProjection();
	}
}

void RRCamera::setRangeDynamically(const RRSolver* solver, bool shadowRays, unsigned numRays)
{
	if (!solver)
		return;

	// get scene bbox, taking large dynamic planes into account (large static planes are not allowed, they break collider)
	RRVec3 sceneMin(0), sceneMax(0);
	const RRObject* planeObject = solver->getAABB(&sceneMin,&sceneMax,nullptr);

	// calculate far as a distance to bbox
	RRReal maxDist = 0;
	for (unsigned j=0;j<8;j++)
	{
		RRVec3 bbPoint((j&1)?sceneMax.x:sceneMin.x,(j&2)?sceneMax.y:sceneMin.y,(j&4)?sceneMax.z:sceneMin.z);
		maxDist = RR_MAX(maxDist,(bbPoint-pos).length());
	}
	if (sceneMin==sceneMax)
		maxDist = 100; // empty scene (possibly with planes), set arbitrary far

	// increase far in presence of plane
	if (planeObject)
	{
		RRVec3 mini,maxi;
		planeObject->getCollider()->getMesh()->getAABB(&mini,&maxi,nullptr);
		RRVec3 size(maxi-mini);
		rr::RRVec4 plane = size.x ? ( size.y ? rr::RRVec4(0,0,1,-mini.z) : rr::RRVec4(0,1,0,-mini.y) ) : rr::RRVec4(1,0,0,-mini.x);
		planeObject->getWorldMatrixRef().transformPlane(plane);
		RRReal planeDistance = plane.planePointDistance(pos);
		RRReal sceneSize = (sceneMax-sceneMin).length();
		maxDist = RR_MAX(maxDist,100*planeDistance);
		maxDist = RR_MAX(maxDist,10*sceneSize);
	}

	// calculate distance to visible geometry (raycasting)
	RRVec2 distanceMinMax(1e10f,0);
	if (panoramaMode!=PM_OFF)
		solver->getCollider()->getDistancesFromPoint(getPosition(),solver->getMultiObject(),shadowRays,distanceMinMax,numRays);
	else
		solver->getCollider()->getDistancesFromCamera(*this,solver->getMultiObject(),shadowRays,distanceMinMax,numRays);

	// set range
	if (distanceMinMax[1]>=distanceMinMax[0])
	{
		// some rays intersected scene, use scene size and distance
		// but don't increase far when raycasting hits more distant part of plane, it would make far depend on view dir, horizon would jump up/down as camera looks up/down
		//float newFar = RR_MAX(maxDist,2*distanceMinMax[1]);
		float newFar = maxDist; // instead, keep far calculated from scene bbox and plane
		float min = distanceMinMax[0]/2;
		float relativeSceneProximity = RR_CLAMPED(newFar/min,10,100)*10; // 100..1000, 100=looking from distance, 1000=closeup
		float newNear = RR_CLAMPED(min,0.0001f,newFar/relativeSceneProximity);
		setRange(newNear,newFar);
	}
	else
	{
		// rays did not intersect scene, increase far if necessary, otherwise keep range unchanged
		if (maxDist>getFar())
			setFar(maxDist);
	}
}

void RRCamera::setProjection(bool _orthogonal, float _aspect, float _fieldOfViewVerticalDeg, float _anear, float _afar, float _orthoSize, RRVec2 _screenCenter)
{
	orthogonal = _orthogonal;
	aspect = _aspect;
	fieldOfViewVerticalDeg = _fieldOfViewVerticalDeg;
	anear = _anear;
	afar = _afar;
	orthoSize = _orthoSize;
	screenCenter = _screenCenter;
	updateProjection();
}


////////////////////////////////////////////////////////////////////////
//
// stereo

void RRCamera::getStereoCameras(RRCamera& leftEye, RRCamera& rightEye) const
{
	leftEye = *this;
	rightEye = *this;
	float localEyeSeparation = stereoSwap ? -eyeSeparation : eyeSeparation;
	leftEye.pos -= getRight()*(localEyeSeparation/2);
	rightEye.pos += getRight()*(localEyeSeparation/2);
	leftEye.screenCenter.x += localEyeSeparation/(2*tan(getFieldOfViewVerticalRad()*0.5f)*aspect*displayDistance);
	rightEye.screenCenter.x -= localEyeSeparation/(2*tan(getFieldOfViewVerticalRad()*0.5f)*aspect*displayDistance);
	leftEye.stereoMode = SM_MONO;
	rightEye.stereoMode = SM_MONO;
	leftEye.updateView(false,false);
	rightEye.updateView(false,false);
	leftEye.updateProjection();
	rightEye.updateProjection();
}


////////////////////////////////////////////////////////////////////////
//
// blending

// returns a*(1-alpha) + b*alpha;
template<class C>
C blendNormal(C a,C b,RRReal alpha)
{
	// don't interpolate between identical values,
	//  results would be slightly different for different alphas (in win64, not in win32)
	//  some optimizations are enabled only when data don't change between frames (see shadowcast.cpp bool lightChanged)
	return (a==b) ? a : (a*(1-alpha) + b*alpha);
}

// returns a*(1-alpha) + b*alpha; (a and b are points on 360degree circle)
// using shortest path between a and b
RRReal blendModulo(RRReal a,RRReal b,RRReal alpha,RRReal modulo)
{
	a = fmodf(a,modulo);
	b = fmodf(b,modulo);
	if (a<b-(modulo/2)) a += modulo; else
	if (a>b+(modulo/2)) a -= modulo;
	return blendNormal(a,b,alpha);
}

// expects normalized quaternions, returns normalized quaternion
RRVec4 quaternion_slerp(RRVec4 q0, RRVec4 q1, float t)
{
	float dp = q0.dot(q1);
	dp = RR_MIN(dp,1);
	if (dp<0) // switch from long path to short path
	{
		q0 = -q0;
		dp = -dp;
	}
	RRVec4 n = q1-q0*dp;
	float nm = n.length();
	if (nm>0)
	{
		float theta = acos(dp)*t;
		return q0*cos(theta) + (n/nm)*sin(theta);
	}
	return (q0+(q1-q0)*t).normalized();
}

void RRMatrix3x4::blendLinear(const RRMatrix3x4& sample0, const RRMatrix3x4& sample1, RRReal blend)
{
	// blend using quaternion slerp
	RRVec4 q0 = sample0.getQuaternion();
	RRVec4 q1 = sample1.getQuaternion();
	*this = translation(blendNormal(sample0.getTranslation(),sample1.getTranslation(),blend))
		* rotationByQuaternion(quaternion_slerp(q0,q1,blend))
		* scale(blendNormal(sample0.getScale(),sample1.getScale(),blend));
}

void RRLight::blendLinear(const RRLight& sample0, const RRLight& sample1, RRReal blend)
{
	type = sample0.type;
	position = blendNormal(sample0.position,sample1.position,blend);
	direction = blendNormal(sample0.direction,sample1.direction,blend);
	outerAngleRad = blendNormal(sample0.outerAngleRad,sample1.outerAngleRad,blend);
	radius = blendNormal(sample0.radius,sample1.radius,blend);
	color = blendNormal(sample0.color,sample1.color,blend);
	distanceAttenuationType = sample0.distanceAttenuationType;
	polynom = blendNormal(sample0.polynom,sample1.polynom,blend);
	fallOffExponent = blendNormal(sample0.fallOffExponent,sample1.fallOffExponent,blend);
	spotExponent = blendNormal(sample0.spotExponent,sample1.spotExponent,blend);
	fallOffAngleRad = blendNormal(sample0.fallOffAngleRad,sample1.fallOffAngleRad,blend);
	enabled = sample0.enabled;
	castShadows = sample0.castShadows;
	directLambertScaled = sample0.directLambertScaled;
	//projectedTexture = sample0.projectedTexture ? a.projectedTexture->createReference() : nullptr;
	rtNumShadowmaps = sample0.rtNumShadowmaps;
	rtShadowmapSize = sample0.rtShadowmapSize;
	rtShadowmapBias = blendNormal(sample0.rtShadowmapBias,sample1.rtShadowmapBias,blend);
	name = sample0.name;
	customData = sample0.customData;
}

// linear interpolation
void RRCamera::blendLinear(const RRCamera& sample0, const RRCamera& sample1, float blend)
{
	pos = blendNormal(sample0.pos,sample1.pos,blend);
	yawPitchRollRad[0] = blendModulo(sample0.yawPitchRollRad[0],sample1.yawPitchRollRad[0],blend,(float)(2*RR_PI));
	yawPitchRollRad[1] = blendNormal(sample0.yawPitchRollRad[1],sample1.yawPitchRollRad[1],blend);
	yawPitchRollRad[2] = blendNormal(sample0.yawPitchRollRad[2],sample1.yawPitchRollRad[2],blend);
	aspect = blendNormal(sample0.aspect,sample1.aspect,blend);
	fieldOfViewVerticalDeg = blendNormal(sample0.fieldOfViewVerticalDeg,sample1.fieldOfViewVerticalDeg,blend);
	anear = blendNormal(sample0.anear,sample1.anear,blend);
	afar = blendNormal(sample0.afar,sample1.afar,blend);
	orthogonal = sample0.orthogonal;
	orthoSize = blendNormal(sample0.orthoSize,sample1.orthoSize,blend);
	screenCenter = blendNormal(sample0.screenCenter,sample1.screenCenter,blend);

	// stereo
	eyeSeparation = blendNormal(sample0.eyeSeparation,sample1.eyeSeparation,blend);
	displayDistance = blendNormal(sample0.displayDistance,sample1.displayDistance,blend);

	// panorama
	panoramaScale = blendNormal(sample0.panoramaScale,sample1.panoramaScale,blend);
	panoramaFisheyeFovDeg = blendNormal(sample0.panoramaFisheyeFovDeg,sample1.panoramaFisheyeFovDeg,blend);

	// dof
	apertureDiameter = blendNormal(sample0.apertureDiameter,sample1.apertureDiameter,blend);
	dofNear = blendNormal(sample0.dofNear,sample1.dofNear,blend);
	dofFar = blendNormal(sample0.dofFar,sample1.dofFar,blend);

	updateView(true,true);
	updateProjection();
}

#ifdef RR_HAS_LAMBDAS

//static inline float abs(float a)
//{
//	return fabs(a);
//}

static inline RRVec3 abs(RRVec3 a)
{
	return a.abs();
}

static inline RRVec4 abs(RRVec4 a)
{
	return a.abs();
}

//static inline void ifBoth0SetBoth1(float& a,float& b)
//{
//	if (!a && !b) a=b=1;
//}

static inline void ifBoth0SetBoth1(RRVec3& a,RRVec3& b)
{
	if (!a[0] && !b[0]) a[0]=b[0]=1;
	if (!a[1] && !b[1]) a[1]=b[1]=1;
	if (!a[2] && !b[2]) a[2]=b[2]=1;
}

static inline void ifBoth0SetBoth1(RRVec4& a,RRVec4& b)
{
	if (!a[0] && !b[0]) a[0]=b[0]=1;
	if (!a[1] && !b[1]) a[1]=b[1]=1;
	if (!a[2] && !b[2]) a[2]=b[2]=1;
	if (!a[3] && !b[3]) a[3]=b[3]=1;
}

static inline void minimizeDistanceModulo2PI(const RRVec3& a,RRVec3& b)
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
	if (numPoints==2)
	{
		Y y0 = y(0);
		Y y1 = y(1);
		if (minimizeRotations)
			minimizeDistanceModulo2PI(y0,y1);
		return y0+(y1-y0)*(xx-x[0])/(x[1]-x[0]);
	}

	// find how many points we have before/after
	unsigned numPointsBeforeXx = 0;
	while (x[numPointsBeforeXx]<xx && numPointsBeforeXx<numPoints) numPointsBeforeXx++;
	unsigned numPointsAfterXx = numPoints-numPointsBeforeXx;

	// for testing only: do linear interpolation instead of Akima
	//return y(numPointsBeforeXx-1)+(y(numPointsBeforeXx)-y(numPointsBeforeXx-1))*(xx-x[numPointsBeforeXx-1])/(x[numPointsBeforeXx]-x[numPointsBeforeXx-1]);

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
	// - extrapolate missing values
	if (numPointsBeforeXx< 1) { x2 = x3*2-x4; y2 = y3*2-y4; }
	if (numPointsBeforeXx< 2) { x1 = x2*2-x3; y1 = y2*2-y3; }
	if (numPointsBeforeXx< 3) { x0 = x1*2-x2; y0 = y1*2-y2; }
	if (numPointsAfterXx < 1) { x3 = x2*2-x1; y3 = y2*2-y1; }
	if (numPointsAfterXx < 2) { x4 = x3*2-x2; y4 = y3*2-y2; }
	if (numPointsAfterXx < 3) { x5 = x4*2-x3; y5 = y4*2-y3; }
	// - ensure that x2!=x3
	if (x2==x3) return (numPointsBeforeXx>=1) ? y2 : y3;
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
	Y m2 = (y3-y2)/(x3-x2); // x2!=x3
	Y m1 = (x2-x1) ? (y2-y1)/(x2-x1) : m2; // don't divide by zero, use the most close good value instead
	Y m0 = (x1-x0) ? (y1-y0)/(x1-x0) : m1;
	Y m3 = (x4-x3) ? (y4-y3)/(x4-x3) : m2;
	Y m4 = (x5-x4) ? (y5-y4)/(x5-x4) : m3;
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

//#define BLEND_FLOAT(name) {name = interpolAkima<float,float>(numSamples,times,[&samples](int i){return samples[i]->name;},time);}
//#define BLEND_RRVEC2(name) {name = interpolAkima<float,RRVec2>(numSamples,times,[&samples](int i){return samples[i]->name;},time);}
#define BLEND_RRVEC3(name) {name = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return samples[i]->name;},time);}
#define BLEND_RRVEC3_ANGLES(name) {name = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return samples[i]->name;},time,true);}
#define BLEND_3FLOATS(name1,name2,name3) {RRVec3 tmp = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return RRVec3(samples[i]->name1,samples[i]->name2,samples[i]->name3);},time); name1=tmp.x; name2=tmp.y; name3=tmp.z;}
#define BLEND_4FLOATS(name1,name2,name3,name4) {RRVec4 tmp = interpolAkima<float,RRVec4>(numSamples,times,[&samples](int i){return RRVec4(samples[i]->name1,samples[i]->name2,samples[i]->name3,samples[i]->name4);},time); name1=tmp.x; name2=tmp.y; name3=tmp.z; name4=tmp.w;}

void RRMatrix3x4::blendAkima(unsigned numSamples, const RRMatrix3x4** samples, const RRReal* times, RRReal time)
{
	if (!numSamples || !samples|| !times)
	{
		RR_ASSERT(0);
		return;
	}
	if (numSamples==1)
	{
		*this = *samples[0];
		return;
	}

	// interpolate
	RRVec3 tra = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return samples[i]->getTranslation();},time);
	RRVec3 rot = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return samples[i]->getYawPitchRoll();},time);
	RRVec3 sca = interpolAkima<float,RRVec3>(numSamples,times,[&samples](int i){return samples[i]->getScale();},time);
	*this = translation(tra) * rotationByYawPitchRoll(rot) * scale(sca);
}

void RRLight::blendAkima(unsigned numSamples, const RRLight** samples, const RRReal* times, RRReal time)
{
	// handle corner cases
	if (!numSamples || !samples || !times)
	{
		RR_ASSERT(0);
		return;
	}
	if (numSamples==1)
	{
		*this = *samples[0];
		return;
	}

	// init this with the most closely preceding source (or first one if none is preceding)
	unsigned i = 1;
	while (i<numSamples && times[i]<time) i++;
	*this = *samples[i-1];

	// interpolate
	BLEND_RRVEC3(position);
	BLEND_RRVEC3(direction); direction.normalize();
	BLEND_RRVEC3(color);
	BLEND_4FLOATS(polynom.x,polynom.y,polynom.z,outerAngleRad);
	BLEND_4FLOATS(radius,fallOffExponent,spotExponent,fallOffAngleRad);
}

void RRCamera::blendAkima(unsigned numSamples, const RRCamera** samples, float* times, float time)
{
	// handle corner cases
	if (!numSamples || !samples || !times)
	{
		RR_ASSERT(0);
		return;
	}
	if (numSamples==1)
	{
		*this = *samples[0];
		return;
	}

	// init this with the most closely preceding source (or first one if none is preceding)
	unsigned i = 1;
	while (i<numSamples && times[i]<time) i++;
	*this = *samples[i-1];

	// interpolate
	BLEND_RRVEC3(pos);
	BLEND_RRVEC3_ANGLES(yawPitchRollRad);
	BLEND_3FLOATS(fieldOfViewVerticalDeg,anear,afar);
	BLEND_4FLOATS(screenCenter.x,screenCenter.y,orthoSize,aspect);

	// stereo + panorama
	BLEND_4FLOATS(eyeSeparation,displayDistance,panoramaScale,panoramaFisheyeFovDeg);

	// dof
	BLEND_3FLOATS(apertureDiameter,dofNear,dofFar);

	updateView(true,true);
	updateProjection();
}

#else // !RR_HAS_LAMBDAS

void RRMatrix3x4::blendAkima(unsigned numSamples, const RRMatrix3x4** samples, const RRReal* times, RRReal time)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"blendAkima() not supported by this compiler, requires lambdas.\n"));
}
void RRLight::blendAkima(unsigned numSamples, const RRLight** samples, const RRReal* times, RRReal time)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"blendAkima() not supported by this compiler, requires lambdas.\n"));
}
void RRCamera::blendAkima(unsigned numSamples, const RRCamera** samples, float* times, float time)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"blendAkima() not supported by this compiler, requires lambdas.\n"));
}

#endif // !RR_HAS_LAMBDAS


////////////////////////////////////////////////////////////////////////
//
// other tools

RRVec4 RRCamera::getPositionInClipSpace(RRVec3 worldPosition) const
{
	double tmp[4];
	RRVec4 out;
	for (int i=0; i<4; i++) tmp[i] = worldPosition[0] * viewMatrix[0*4+i] + worldPosition[1] * viewMatrix[1*4+i] + worldPosition[2] * viewMatrix[2*4+i] + viewMatrix[3*4+i];
	for (int i=0; i<4; i++) out[i] = (RRReal)( tmp[0] * projectionMatrix[0*4+i] + tmp[1] * projectionMatrix[1*4+i] + tmp[2] * projectionMatrix[2*4+i] + tmp[3] * projectionMatrix[3*4+i] );
	return out;
}

RRVec3 RRCamera::getPositionInViewport(RRVec3 worldPosition) const
{
	if (panoramaMode==PM_OFF)
	{
		double tmp[4],out[4];
		for (int i=0; i<4; i++) tmp[i] = worldPosition[0] * viewMatrix[0*4+i] + worldPosition[1] * viewMatrix[1*4+i] + worldPosition[2] * viewMatrix[2*4+i] + viewMatrix[3*4+i];
		for (int i=0; i<4; i++) out[i] = tmp[0] * projectionMatrix[0*4+i] + tmp[1] * projectionMatrix[1*4+i] + tmp[2] * projectionMatrix[2*4+i] + tmp[3] * projectionMatrix[3*4+i];
		return RRVec3((RRReal)(out[0]/out[3]),(RRReal)(out[1]/out[3]),(RRReal)(out[2]/out[3]));
	}
	// panoramaMode [#42]
	rr::RRMatrix3x4 view(viewMatrix,!false);
	RRVec3 direction = worldPosition-pos;
	view.transformDirection(direction);
	RRVec2 posInWindow(0);
	if (panoramaMode==PM_EQUIRECTANGULAR)
	{
		direction.normalize();
		RRReal y = asin(direction.y)*2/RR_PI;
		RRReal x = 0.5f - asin( direction.x / sqrt(1-direction.y*direction.y) ) / RR_PI;
		posInWindow = RRVec2((direction.z>0)?x:-x,y);
	}
	if (panoramaMode==PM_LITTLE_PLANET)
	{
		direction /= sqrt(direction.x*direction.x+direction.z*direction.z) + 0.000001f;
		float r = atan(direction.y)/RR_PI/2+0.25f;
		posInWindow = RRVec2(direction.x*2*r,-direction.z*2*r);
	}
	if (panoramaMode==PM_FISHEYE)
	{
		direction /= sqrt(direction.x*direction.x+direction.y*direction.y) + 0.000001f;
		float r = atan(direction.z)/RR_PI/2+0.25f;
		posInWindow = RRVec2(direction.x*2*r,direction.y*2*r);
	}

	// panoramaScale [#43]
	float scale = (panoramaMode==rr::RRCamera::PM_FISHEYE) ? panoramaScale * 360/panoramaFisheyeFovDeg : panoramaScale;
	posInWindow *= scale;

	// panoramaCoverage [#44]
	float x0 = 0;
	float y0 = 0;
	float w = 1;
	float h = 1;
	if (panoramaMode!=PM_EQUIRECTANGULAR)
	switch (panoramaCoverage)
	{
		case PC_FULL_STRETCH:
			break;
		case PC_FULL:
			if (aspect>1)
				posInWindow.x /= aspect;
			else
				posInWindow.y *= aspect;
			break;
		case PC_TRUNCATE_BOTTOM:
			posInWindow.y = posInWindow.y*aspect+(1-aspect);
			break;
		case PC_TRUNCATE_TOP:
			posInWindow.y = posInWindow.y*aspect-(1-aspect);
			break;
		case PC_TRUNCATE_TOP_BOT:
			posInWindow.y = posInWindow.y*aspect;
			break;
	}

	return RRVec3(posInWindow.x,posInWindow.y,1);
}

bool RRCamera::getRay(RRVec2 posInWindow, RRVec3& rayOrigin, RRVec3& rayDir, bool randomized) const
{
	// de-stereo-ize posInWindow
	bool left = true;
	RRVec3 pos = this->pos;
	RRVec2 screenCenter = this->screenCenter;
	RRReal aspect = this->aspect;
	if (stereoMode==SM_MONO || stereoMode==SM_INTERLACED || stereoMode==SM_OCULUS_RIFT || stereoMode==SM_QUAD_BUFFERED)
	{
		// done
	}
	else
	{
		// modify posInWindow
		if (stereoMode==SM_SIDE_BY_SIDE)
		{
			aspect *= 0.5f;
			if (posInWindow.x<0)
			{
				left = true;
				posInWindow.x = posInWindow.x*2 + 1;
			}
			else
			{
				left = false;
				posInWindow.x = posInWindow.x*2 - 1;
			}
		}
		else if (stereoMode==SM_TOP_DOWN)
		{
			aspect *= 2;
			if (posInWindow.y<0)
			{
				left = true;
				posInWindow.y = posInWindow.y*2 + 1;
			}
			else
			{
				left = false;
				posInWindow.y = posInWindow.y*2 - 1;
			}
		}
		// emulate getStereoCameras() within our local variables
		if (stereoSwap)
			left = !left;
		if (!left)
		{
			pos += getRight()*(eyeSeparation/2);
			screenCenter.x += eyeSeparation/(2*tan(getFieldOfViewVerticalRad()*0.5f)*aspect*displayDistance);
		}
		else
		{
			pos -= getRight()*(eyeSeparation/2);
			screenCenter.x -= eyeSeparation/(2*tan(getFieldOfViewVerticalRad()*0.5f)*aspect*displayDistance);
		}
	}


	// orthogonal
	if (orthogonal)
	{
		rayOrigin = pos
			+ getRight() * (posInWindow[0]+screenCenter[0]) * orthoSize * aspect
			+ getUp()    * (posInWindow[1]+screenCenter[1]) * orthoSize
			;
		rayDir = getDirection();
		return true;
	}

	// perspective
	rayOrigin = pos;
	if (panoramaMode==PM_OFF)
	{
		rr::RRVec2 localScreenCenter = screenCenter;
		if (randomized && apertureDiameter)
		{
			// randomize ray according to apertureDiameter and default bokeh shape [#48]
		more_samples_needed:
			rr::RRVec3 offsetInBuffer = rr::RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),0); // 0..1
			rr::RRVec2 a(offsetInBuffer.x*2-1,offsetInBuffer.y*2-1);
			if (a.length2()>1) // sample is not inside default bokeh shape (circle)
				goto more_samples_needed;
			rr::RRReal dofDistance = (dofFar+dofNear)/2;
			rr::RRVec2 offsetInMeters = rr::RRVec2(offsetInBuffer.x-0.5f,offsetInBuffer.y-0.5f)*apertureDiameter; // how far do we move camera in right,up directions, -apertureDiameter/2..apertureDiameter/2 (m)
			rayOrigin += getRight()*offsetInMeters.x+getUp()*offsetInMeters.y;
			rr::RRVec2 visibleMetersAtFocusedDistance(tan(getFieldOfViewHorizontalRad()/2)*dofDistance,tan(getFieldOfViewVerticalRad()/2)*dofDistance); // from center to edge (m)
			rr::RRVec2 offsetOnScreen = offsetInMeters/visibleMetersAtFocusedDistance; // how far do we move camera in -1..1 screen space 
			localScreenCenter -= offsetOnScreen;
		}

		rayDir = getDirection()
			+ getRight() * ( (posInWindow[0]+localScreenCenter[0]) * tan(getFieldOfViewHorizontalRad()/2) )
			+ getUp()    * ( (posInWindow[1]+localScreenCenter[1]) * tan(getFieldOfViewVerticalRad()  /2) )
			;
		// CameraObjectDistance uses length of our result, don't normalize
		return true;
	}

	// panoramaCoverage [#44]
	float x0 = 0;
	float y0 = 0;
	float w = 1;
	float h = 1;
	if (panoramaMode!=PM_EQUIRECTANGULAR)
	switch (panoramaCoverage)
	{
		case PC_FULL_STRETCH:
			break;
		case PC_FULL:
			if (aspect>1)
				posInWindow.x *= aspect;
			else
				posInWindow.y /= aspect;
			break;
		case PC_TRUNCATE_BOTTOM:
			posInWindow.y = (posInWindow.y-(1-aspect))/aspect;
			break;
		case PC_TRUNCATE_TOP:
			posInWindow.y = (posInWindow.y+(1-aspect))/aspect;
			break;
		case PC_TRUNCATE_TOP_BOT:
			posInWindow.y = posInWindow.y/aspect;
			break;
	}

	// panoramaScale [#43]
	float scale = (panoramaMode==rr::RRCamera::PM_FISHEYE) ? panoramaScale * 360/panoramaFisheyeFovDeg : panoramaScale;
	posInWindow /= scale;

	// panoramaMode [#42]
	bool result = false;
	if (panoramaMode==PM_EQUIRECTANGULAR)
	{
		RRVec3 direction;
		direction.y = sin(RR_PI/2*posInWindow.y);
		direction.x = sin(RR_PI*(-posInWindow.x+0.5f)) * sqrt(1-direction.y*direction.y);
		direction.z = 1-direction.x*direction.x-direction.y*direction.y;
		direction.z = sqrt(RR_MAX(0,direction.z));
		if (posInWindow.x<0)
			direction.z = -direction.z;
		rayDir = direction;
		result = true;
	}
	if (panoramaMode==PM_LITTLE_PLANET)
	{
		RRVec3 direction;
		direction.x = posInWindow.x*0.5f;
		direction.z = -posInWindow.y*0.5f;
		float r = (posInWindow*0.5f).length()+0.000001f; // +epsilon fixes center pixel on intel
		direction.x /= r; // /r instead of normalize() fixes noise on intel
		direction.z /= r;
		direction.y = tan(RR_PI*2*(r-0.25f)); // r=0 -> y=-inf, r=0.5 -> y=+inf
		rayDir = direction;
		result = r<0.5f;
	}
	if (panoramaMode==PM_FISHEYE)
	{
		RRVec3 direction;
		direction.x = posInWindow.x*0.5f;
		direction.y = posInWindow.y*0.5f;
		float r = (posInWindow*0.5f).length()+0.000001f; // +epsilon fixes center pixel on intel
		direction.x /= r; // /r instead of normalize() fixes noise on intel
		direction.y /= r;
		direction.z = tan(RR_PI*2*(r-0.25f)); // r=0 -> y=-inf, r=0.5 -> y=+inf
		rayDir = direction;
		result = r*360/panoramaFisheyeFovDeg<0.5f;
	}
	rr::RRMatrix3x4 view(viewMatrix,false);
	view.transformDirection(rayDir);
	return result;
}

const RRCamera& RRCamera::operator=(const RRCamera& a)
{
	// view
	pos = a.pos;
	yawPitchRollRad = a.yawPitchRollRad;

	// projection
	aspect = a.aspect;
	fieldOfViewVerticalDeg = a.fieldOfViewVerticalDeg;
	orthogonal = a.orthogonal;
	anear = a.anear;
	afar = a.afar;
	orthoSize = a.orthoSize;
	screenCenter = a.screenCenter;

	// stereo
	stereoMode = a.stereoMode;
	stereoSwap = a.stereoSwap;
	eyeSeparation = a.eyeSeparation;
	displayDistance = a.displayDistance;

	// panorama
	panoramaMode = a.panoramaMode;
	panoramaCoverage = a.panoramaCoverage;
	panoramaScale = a.panoramaScale;
	panoramaFisheyeFovDeg = a.panoramaFisheyeFovDeg;

	// dof
	apertureDiameter = a.apertureDiameter;
	dofNear = a.dofNear;
	dofFar = a.dofFar;

	light = a.light;

	memcpy(viewMatrix,a.viewMatrix,16*sizeof(double));
	memcpy(projectionMatrix,a.projectionMatrix,16*sizeof(double));
	//updateView(false,false);
	//updateProjection();

	return *this;
}

bool RRCamera::operator==(const RRCamera& a) const
{
	return
		// view
		pos==a.pos
		&& yawPitchRollRad==a.yawPitchRollRad

		// projection
		//&& aspect==a.aspect ... aspect is usually just byproduct of window size, users don't want "scene was modified" just because window size changed
		&& fieldOfViewVerticalDeg==a.fieldOfViewVerticalDeg
		&& orthogonal==a.orthogonal
		&& anear==a.anear
		&& afar==a.afar
		&& orthoSize==a.orthoSize
		&& screenCenter==a.screenCenter

		// stereo
		&& stereoMode==a.stereoMode
		&& stereoSwap==a.stereoSwap
		&& eyeSeparation==a.eyeSeparation
		&& displayDistance==a.displayDistance

		// panorama
		&& panoramaMode==a.panoramaMode
		&& panoramaCoverage==a.panoramaCoverage
		&& panoramaScale==a.panoramaScale
		&& panoramaFisheyeFovDeg==a.panoramaFisheyeFovDeg

		// dof
		&& apertureDiameter==a.apertureDiameter
		&& dofNear==a.dofNear
		&& dofFar==a.dofFar;
}

bool RRCamera::operator!=(const RRCamera& a) const
{
	return !(*this==a);
}

void RRCamera::mirror(const rr::RRVec4& plane)
{
	// RRMatrix3x4::mirror flips left-right, but manipulateViewBy reverts it
	//   because such flip can't be represented by pos+yawPitchRollRad
	//   also, it would probably require CW-CCW swap while rendering, which is unwanted complication
	//   workaround is simple and it costs nothing,
	//    1) we flip screenCenter.x now
	//    2) we flip left-right later when applying mirror texture
	manipulateViewBy(RRMatrix3x4::mirror(plane));
	screenCenter.x = -screenCenter.x;
	updateProjection();
}

static unsigned makeFinite(float& f, float def)
{
	if (_finite(f))
		return 0;
	f = def;
	return 1;
}

static unsigned makeFinite(RRVec2& v, const RRVec2& def)
{
	return makeFinite(v[0],def[0])+makeFinite(v[1],def[1]);
}

static unsigned makeFinite(RRVec3& v, const RRVec3& def)
{
	return makeFinite(v[0],def[0])+makeFinite(v[1],def[1])+makeFinite(v[2],def[2]);
}

unsigned RRCamera::fixInvalidValues()
{
	unsigned numFixes =
		+ makeFinite(pos,RRVec3(0))
		+ makeFinite(aspect,1)
		+ makeFinite(fieldOfViewVerticalDeg,90)
		+ makeFinite(anear,0.1f)
		+ makeFinite(afar,100)
		+ makeFinite(screenCenter,RRVec2(0))
		+ makeFinite(orthoSize,100)
		+ makeFinite(orthoSize,100)
		+ makeFinite(yawPitchRollRad[0],0)
		+ makeFinite(yawPitchRollRad[1],0)
		+ makeFinite(yawPitchRollRad[2],0)
		
		// stereo
		+ makeFinite(eyeSeparation,0.08f)
		+ makeFinite(displayDistance,0.5f)

		// panorama
		+ makeFinite(panoramaScale,1)
		+ makeFinite(panoramaFisheyeFovDeg,180)
		
		// dof
		+ makeFinite(apertureDiameter,0.04f)
		+ makeFinite(dofNear,1)
		+ makeFinite(dofFar,1);
	if (numFixes)
	{
		updateView(true,true);
		updateProjection();
	}
	return numFixes;
}

////////////////////////////////////////////////////////////////////////
//
// update

void RRCamera::updateView(bool updateLightPos, bool updateLightDir)
{
	// pos, yawPitchRollRad -> viewMatrix, rrlight
	// parameters are necessary because of following situation:
	//	RRLight did change, we call updateAfterRRLightChanges()
	//	in first step, updateAfterRRLightChanges() calls setPosition()
	//	setPosition() calls updateView()
	//	updateView() must not copy direction, because it would overwrite new value in RRLight by old value from RRCamera
	RRMatrix3x4 view = RRMatrix3x4::rotationByYawPitchRoll(yawPitchRollRad);
	for (unsigned i=0;i<4;i++)
		for (unsigned j=0;j<4;j++)
			viewMatrix[4*i+j] = (i<3) ? ((j<3)?view.m[i][j]:0) : ((j<3)?-pos.dot(view.getColumn(j)):1);
	if (light && updateLightPos)
		light->position = getPosition();
	if (light && updateLightDir)
		light->direction = getDirection();
}

void RRCamera::updateProjection()
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
		projectionMatrix[10] = 1/(anear-afar);
		projectionMatrix[11] = 0;
		projectionMatrix[12] = -screenCenter.x;
		projectionMatrix[13] = -screenCenter.y;
		projectionMatrix[14] = (anear+afar)/(anear-afar);
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
		projectionMatrix[10] = (afar+anear)/(anear-afar);
		projectionMatrix[11] = -1;
		projectionMatrix[12] = 0;
		projectionMatrix[13] = 0;
		projectionMatrix[14] = 2*anear*afar/(anear-afar);
		projectionMatrix[15] = 0;
	}
}

}; // namespace
