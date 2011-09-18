//----------------------------------------------------------------------------
//! \file Camera.h
//! \brief LightsprintGL | frustum with adjustable pos, dir, aspect, fov, near, far
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2011
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
//! With yawPitchRollRad reset to 0, camera view direction is Z+ (0,0,1),
//! up vector is Y+ (0,1,0) and right vector is X- (-1,0,0).
class RR_GL_API Camera : public rr::RRUniformlyAllocated
{
public:
	// inputs, to be modified by user

	//! Position of camera (imaginary frustum apex).
	rr::RRVec3 pos;
	//! Rotation of camera in radians. Rotations are applied in this order: yaw (around Y axis), pitch (around X axis), roll (around Z axis).
	rr::RRVec3 yawPitchRollRad;
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
	//! Is camera orthogonal or perspective?
	union
	{
		bool     orthogonal;
		float    dummy1; // AnimationFrame needs everything float sized
	};
	//! Only if perspective: maps view direction into screen position, default 0,0 maps view direction to screen center.
	rr::RRVec2 screenCenter;
	//! Only if orthogonal: World space distance between top and bottom of viewport.
	float    orthoSize;
	//! True(default): you set yawPitchRollRad, update() computes direction from angles. False: you set direction, update() doesn't touch it.
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
	Camera(const rr::RRVec3& pos, const rr::RRVec3& yawPitchRollRad, float aspect, float fieldOfView, float anear, float afar);
	//! Initializes all inputs from RRLight. If you move light, each update() will copy new position and direction to light.
	Camera(rr::RRLight& light);
	//! Sets camera direction. Doesn't have to be normalized. Alternatively, you can write directly to yawPitchRollRad or dir, depending on updateDirFromAngles flag.
	void setDirection(const rr::RRVec3& dir);
	//! Converts world space position (3d) to position in window (2d).
	//! positionInWindow 0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	rr::RRVec2 getPositionInWindow(rr::RRVec3 worldPosition) const;
	//! Converts position in window (2d) to world space ray origin (3d), suitable for raycasting screen pixels.
	//! positionInWindow 0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	rr::RRVec3 getRayOrigin(rr::RRVec2 positionInWindow) const;
	//! Converts position in window (2d) to world space ray direction (3d) from origin to depth=1.
	//! Vector is not normalized, length is >=1, so that getRayPosition()+getRayDirection() is always in depth=1 plane.
	//! positionInWindow 0,0 represents center of window, -1,-1 top left window corner, 1,1 bottom right window corner.
	rr::RRVec3 getRayDirection(rr::RRVec2 positionInWindow) const;

	float getAspect()                   const {return aspect;}
	float getFieldOfViewVerticalDeg()   const {return fieldOfViewVerticalDeg;}
	float getFieldOfViewHorizontalDeg() const {return RR_RAD2DEG(getFieldOfViewHorizontalRad());}
	float getFieldOfViewVerticalRad()   const {return RR_DEG2RAD(fieldOfViewVerticalDeg);}
	float getFieldOfViewHorizontalRad() const {return atan(tan(getFieldOfViewVerticalRad()*0.5f)*aspect)*2;}
	float getNear()                     const {return anear;}
	float getFar()                      const {return afar;}
	//! Sets aspect ratio, with configurable effect on FOV.
	//! \param aspect
	//!  Your new aspect ratio, usually set to viewport width/height.
	//! \param effectOnFOV
	//!  0 to preserve vertical FOV, 1 to preserve horizontal FOV, values between 0 and 1 for blend of both options.
	//!  Default 0 is absolutely numerically stable, other values add tiny error to FOV each time setAspect is called.
	void  setAspect(float aspect, float effectOnFOV = 0);
	void  setFieldOfViewVerticalDeg(float fieldOfViewVerticalDeg);
	void  setNear(float _near);
	void  setFar(float _far);
	void  setRange(float _near, float _far);
	//! Sets near and far to cover scene visible by camera, but not much more.
	//
	//! Uses raycasting (~100 rays), performance hit is acceptable even if called once per frame.
	//! Camera should be up to date before call, use update() if it is not.
	//! \param scene
	//!  Multiobject with all objects in scene.
	//! \param water
	//!  Whether to test distance and prevent near/far clipping also for virtual water plane.
	//! \param waterLevel
	//!  y coordinate of water plane.
	void  setRangeDynamically(const rr::RRObject* scene, bool water=false, float waterLevel=-1e10f);

	//! == operator, true when inputs without aspect are equal.
	bool operator==(const Camera& a) const;
	//! != operator, true when inputs differ.
	bool operator!=(const Camera& a) const;
	//! Mirrors camera for reflection rendering. Second call takes changes back.
	//! \param altitude Altitude of mirroring plane.
	void mirror(float altitude);

	//! Fills this with linear interpolation of a and b; a if blend=0, b if blend=1.
	void blend(const Camera& a, const Camera& b, float blend);
	//! Fills this with Akima interpolation of cameras, non uniformly scattered on time scale.
	//
	//! \param numCameras
	//!  Number of elements in cameras and times arrays.
	//!  Interpolations uses at most 3 preceding and 3 following cameras, providing more does not increase quality.
	//! \param cameras
	//!  Pointers to numCameras cameras.
	//! \param times
	//!  Array of times assigned to cameras.
	//!  Sequence must be monotonic increasing, result is undefined if it is not.
	//! \param time
	//!  Time assigned to this camera.
	void blendAkima(unsigned numCameras, const Camera** cameras, float* times, float time);

	//! Updates all outputs, recalculates them from inputs.
	void update();
	//! Rotates viewMatrix into one of 6 directions of point light. To be called after update().
	void rotateViewMatrix(unsigned instance);
	//! Sends our outputs to OpenGL pipeline, so that following primitives are
	//! transformed as if viewed by this camera.
	void setupForRender() const;
	//! Returns last camera that executed setupForRender().
	static const Camera* getRenderCamera();

	//! Predefined camera views.
	enum View
	{
		TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT, RANDOM, OTHER // note: in case of changing order, fix getView()
	};
	//! Sets one of predefined orthogonal camera views, or random perspective view if RANDOM is requested.
	//! Setting OTHER has no effect.
	//! If scene is provided, adjusts also position/near/far.
	//! Setting orthogonal view is fast, RANDOM uses raycasting (~1000 rays).
	void setView(View view, const rr::RRObject* scene);
	//! Returns whether camera is in one of predefined orthogonal views, or OTHER if it is not.
	//! View is detected from direction or yawPitchRollRad, position and range are ignored.
	View getView() const;

	//! Fixes NaN and INF values found in camera inputs (pos, dir etc).
	//
	//! It is important before serializing camera into text stream that can't handle infinite numbers.
	//! It does not change outputs (up, right, matrices etc), use update() to update outputs from inputs.
	//! \return Number of changes made.
	unsigned fixInvalidValues();
};

}; // namespace

#endif //CAMERA_H
