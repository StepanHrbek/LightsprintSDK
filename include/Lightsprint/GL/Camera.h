//----------------------------------------------------------------------------
//! \file Camera.h
//! \brief LightsprintGL | frustum with adjustable pos, dir, aspect, fov, near, far
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

#include "DemoEngine.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Camera

//! Frustum with publicly visible parameters, suitable for cameras and spotlights.
//
//! With all angles reset to 0, camera direction is (0,0,1), Z+,
//! and up vector is (0,1,0), Y+.
class RR_GL_API Camera : public rr::RRUniformlyAllocated
{
public:
	// inputs, to be modified by user

	//! Position of camera (imaginary frustum apex).
	rr::RRVec3 pos;
	//! Rotation around Y axis, radians. For characters standing in Y axis, it controls their look to the left/right. Ignored if !updateDirFromAngles.
	float    angle;
	//! Rotation around Z axis, radians. For characters looking into Z+ axis, it controls leaning. Used for up vector even if !updateDirFromAngles.
	float    leanAngle;
	//! Rotation around X axis, radians, controls looking up/down. Ignored if !updateDirFromAngles.
	float    angleX;
private:
	//! Camera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Camera's vertical field of view in degrees. Must be positive and less than 180.
	float    fieldOfViewVerticalDeg;
	//! Camera's near plane distance in world units. Must be positive.
	float    anear;
	//! Camera's far plane distance in world units. Must be greater than near.
	float    afar;
public:
	//! Whether camera is orthogonal, set for directional lights.
	union
	{
		bool     orthogonal;
		float    dummy1; // AnimationFrame needs everything float sized
	};
	//! Only if orthogonal: World space distance between closest points projected to top and bottom of screen.
	float    orthoSize;
	//! True(default): you set angle+leanAngle+angleX, update() computes direction from angles. False: you set direction, update() doesn't touch it.
	union
	{
		bool     updateDirFromAngles;
		float    dummy2; // AnimationFrame needs everything float sized
	};

	// inputs or outputs

	//! Normalized view direction. Input if !updateDirFromAngles, output if updateDirFromAngles.
	rr::RRVec3 dir;

	// outputs, to be calculated by update() and possibly read by user

	//! Normalized up vector.
	rr::RRVec3 up;
	//! Normalized right vector.
	rr::RRVec3 right;
	//! View matrix in format suitable for OpenGL.
	double   viewMatrix[16];
	//! Inverse view matrix in format suitable for OpenGL.
	double   inverseViewMatrix[16];
	//! Projection matrix in format suitable for OpenGL.
	double   frustumMatrix[16];
	//! Inverse projection matrix in format suitable for OpenGL.
	double   inverseFrustumMatrix[16];
	//! When set, it's position/direction is updated in update().
	rr::RRLight* origin;

	// tools, to be called by user

	//! Default constructor.
	Camera();
	//! Initializes all inputs at once.
	Camera(float posx, float posy, float posz, float angle, float leanAngle, float angleX, float aspect, float fieldOfView, float anear, float afar);
	//! Initializes all inputs from RRLight. If you move light, each update() will copy new position and direction to light.
	Camera(rr::RRLight& light);
	//! Sets camera direction. Doesn't have to be normalized. Alternatively, you can write directly to angles or dir, depending on updateDirFromAngles flag.
	void setDirection(const rr::RRVec3& dir);
	//! Converts position in window (2d) to world space directions (3d) from eye to given point in scene.
	//! Vector is not normalized, length is >=1, so that pos+getDirection() is always in depth=1 plane.
	//! posInWindow 0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	rr::RRVec3 getDirection(rr::RRVec2 posInWindow = rr::RRVec2(0)) const;

	float getAspect()                   const {return aspect;}
	float getFieldOfViewVerticalDeg()   const {return fieldOfViewVerticalDeg;}
	float getFieldOfViewHorizontalDeg() const {return RR_RAD2DEG(getFieldOfViewHorizontalRad());}
	float getFieldOfViewVerticalRad()   const {return RR_DEG2RAD(fieldOfViewVerticalDeg);}
	float getFieldOfViewHorizontalRad() const {return atan(tan(getFieldOfViewVerticalRad()*0.5f)*aspect)*2;}
	float getNear()                     const {return anear;}
	float getFar()                      const {return afar;}
	void  setAspect(float aspect);
	void  setFieldOfViewVerticalDeg(float fieldOfViewVerticalDeg);
	void  setNear(float _near);
	void  setFar(float _far);
	void  setRange(float _near, float _far);
	//! Sets pos and dir randomly, and near-far range based on scene size. Uses raycasting (~1000 rays).
	void  setPosDirRangeRandomly(const rr::RRObject* scene);
	//! Sets near and far to cover scene visible by camera, but not much more.
	//
	//! Uses raycasting (~100 rays), performance hit is acceptable even if called once per frame.
	//! Camera should be up to date before call, use update() if it is not.
	void  setRangeDynamically(const rr::RRObject* scene);
	
	//! == operator, true when inputs are equal.
	bool operator==(const Camera& a) const;
	//! != operator, true when inputs differ.
	bool operator!=(const Camera& a) const;
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
	//
	//! \param observer
	//!  When observer is set, camera is moved to cover area around observer.
	//!  Typically, observer is player and this is directional (sun) light with shadows;
	//!  updated sun travels with player.
	//! \param maxShadowArea
	//!  Only for directional light: Limits area where shadows are computed, default is 1000x1000.
	void update(const Camera* observer=NULL, float maxShadowArea=1000);
	//! Rotates viewMatrix into one of 6 directions of point light. To be called after update().
	void rotateViewMatrix(unsigned instance);
	//! Sends our outputs to OpenGL pipeline, so that following primitives are
	//! transformed as if viewed by this camera.
	void setupForRender() const;
	//! Returns last camera that executed setupForRender().
	static const Camera* getRenderCamera();
};

}; // namespace

#endif //CAMERA_H
