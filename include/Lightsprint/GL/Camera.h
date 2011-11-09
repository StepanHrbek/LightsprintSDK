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
	//////////////////////////////////////////////////////////////////////////////
	// constructors
	//////////////////////////////////////////////////////////////////////////////

	//! Initializes camera in world center.
	Camera();
	//! Initializes everything at once.
	Camera(const rr::RRVec3& pos, const rr::RRVec3& yawPitchRollRad, float aspect, float fieldOfView, float anear, float afar);
	//! Initializes camera from RRLight. Changes made to camera will be propagated back to light, so you must not delete light before camera.
	Camera(rr::RRLight& light);


	//////////////////////////////////////////////////////////////////////////////
	// view get/setters
	//////////////////////////////////////////////////////////////////////////////

	//! Returns camera position.
	const rr::RRVec3& getPosition() const {return pos;}
	//! Sets camera position.
	void setPosition(const rr::RRVec3& _pos);

	//! Returns camera rotation in radians.
	//
	//! Yaw + Pitch + Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
	//! Rotations are applied in this order: yaw (around Y axis), pitch (around X axis), roll (around Z axis).
	const rr::RRVec3& getYawPitchRollRad() const {return yawPitchRollRad;}
	//! Sets camera rotation in radians.
	//
	//! Yaw + Pitch + Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
	//! Rotations are applied in this order: yaw (around Y axis), pitch (around X axis), roll (around Z axis).
	void setYawPitchRollRad(const rr::RRVec3& _yawPitchRollRad);

	//! Returns normalized view direction.
	//
	//! May slightly differ from what was sent to setDirection() due to limited precision of floats.
	rr::RRVec3 getDirection() const {return -rr::RRVec3((rr::RRReal)viewMatrix[2],(rr::RRReal)viewMatrix[6],(rr::RRReal)viewMatrix[10]);}
	//! Sets view direction, resets roll to 0.
	//
	//! Direction doesn't have to be normalized.
	//! getDirection() may return slightly different direction due to limited precision of floats.
	void setDirection(const rr::RRVec3& dir);

	//! Returns normalized up vector.
	rr::RRVec3 getUp() const {return rr::RRVec3((rr::RRReal)viewMatrix[1],(rr::RRReal)viewMatrix[5],(rr::RRReal)viewMatrix[9]);}
	//! Returns normalized right vector.
	rr::RRVec3 getRight() const {return rr::RRVec3((rr::RRReal)viewMatrix[0],(rr::RRReal)viewMatrix[4],(rr::RRReal)viewMatrix[8]);}

	//! Applies transformation on top of transformations already set in view matrix. Only rotation and translation components are used, scale is ignored.
	//! If transformation would make pitch overflow 90 or -90, function does nothing and returns false.
	bool manipulateViewBy(const rr::RRMatrix3x4& transformation, bool rollChangeAllowed=true);

	//! Predefined camera views.
	enum View
	{
		TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT, RANDOM, OTHER // note: in case of changing order, fix getView()
	};
	//! Returns whether camera is in one of predefined orthogonal views, or OTHER if it is not.
	//! View is detected from direction or yawPitchRollRad, position and range are ignored.
	View getView() const;
	//! Sets one of predefined orthogonal camera views, or random perspective view if RANDOM is requested.
	//! Setting OTHER has no effect.
	//! If scene is provided, adjusts also position/near/far.
	//! Setting orthogonal view is fast, RANDOM uses raycasting (~1000 rays).
	void setView(View view, const rr::RRObject* scene);

	//! Returns 4x4 view matrix.
	const double* getViewMatrix() const {return viewMatrix;}


	//////////////////////////////////////////////////////////////////////////////
	// projection get/setters
	//////////////////////////////////////////////////////////////////////////////

	//! Returns true if camera is orthogonal rather, false if perspective.
	bool isOrthogonal() const {return orthogonal;}
	//! Makes camera orthogonal or perspective.
	void setOrthogonal(bool orthogonal);

	//! Returns aspect ratio.
	float getAspect()                   const {return aspect;}
	//! Sets aspect ratio, with configurable effect on FOV.
	//! \param aspect
	//!  Your new aspect ratio, usually set to viewport width/height.
	//! \param effectOnFOV
	//!  0 to preserve vertical FOV, 1 to preserve horizontal FOV, values between 0 and 1 for blend of both options.
	//!  Default 0 is absolutely numerically stable, other values add tiny error to FOV each time setAspect is called.
	void  setAspect(float aspect, float effectOnFOV = 0);

	//! Returns vertical FOV in degrees.
	float getFieldOfViewVerticalDeg()   const {return fieldOfViewVerticalDeg;}
	//! Returns horizontal FOV in degrees.
	float getFieldOfViewHorizontalDeg() const {return RR_RAD2DEG(getFieldOfViewHorizontalRad());}
	//! Returns vertical FOV in radians.
	float getFieldOfViewVerticalRad()   const {return RR_DEG2RAD(fieldOfViewVerticalDeg);}
	//! Returns horizontal FOV in radians.
	float getFieldOfViewHorizontalRad() const {return atan(tan(getFieldOfViewVerticalRad()*0.5f)*aspect)*2;}
	//! Sets vertical FOV in degrees.
	void  setFieldOfViewVerticalDeg(float fieldOfViewVerticalDeg);

	//! Returns near clipping plane distance from camera.
	float getNear()                     const {return anear;}
	//! Returns far clipping plane distance from camera.
	float getFar()                      const {return afar;}
	//! Sets near clipping plane distance from camera.
	void  setNear(float _near);
	//! Sets far clipping plane distance from camera.
	void  setFar(float _far);
	//! Sets distance of both near and far clipping planes from camera.
	void  setRange(float _near, float _far);
	//! Sets distance of both near and far clipping planes from camera automatically.
	//
	//! Uses raycasting (~100 rays), performance hit is acceptable even if called once per frame.
	//! \param scene
	//!  Multiobject with all objects in scene.
	//! \param water
	//!  Whether to test distance and prevent near/far clipping also for virtual water plane.
	//! \param waterLevel
	//!  y coordinate of water plane.
	void  setRangeDynamically(const rr::RRObject* scene, bool water=false, float waterLevel=-1e10f);

	//! Only if orthogonal: Returns world space distance between top and bottom of viewport.
	float getOrthoSize() const {return orthoSize;}
	//! Only if orthogonal: Sets world space distance between top and bottom of viewport.
	void setOrthoSize(float orthoSize);

	//! View direction is projected into given screen position, default 0,0 for screen center, -1,-1 for top left corner, 1,1 for bottom right corner.
	rr::RRVec2 getScreenCenter() {return screenCenter;}
	void setScreenCenter(rr::RRVec2 screenCenter);

	//! Returns 4x4 projection matrix.
	const double* getProjectionMatrix() const {return projectionMatrix;}


	//////////////////////////////////////////////////////////////////////////////
	// blending
	//////////////////////////////////////////////////////////////////////////////

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


	//////////////////////////////////////////////////////////////////////////////
	// other tools
	//////////////////////////////////////////////////////////////////////////////

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

	//! == operator, true when inputs without aspect are equal.
	bool operator==(const Camera& a) const;
	//! != operator, true when inputs differ.
	bool operator!=(const Camera& a) const;

	//! Mirrors camera for water reflection rendering. Second call takes changes back.
	//! \param altitude Altitude of mirroring plane.
	void mirror(float altitude);

	//! Fixes NaN and INF values found in camera inputs (pos, dir etc).
	//
	//! It is important before serializing camera into text stream that can't handle infinite numbers.
	//! It does not change outputs (up, right, matrices etc), use update() to update outputs from inputs.
	//! \return Number of changes made.
	unsigned fixInvalidValues();

private:
	void updateView();
	void updateProjection();

	//////////////////////////////////////////////////////////////////////////////
	// view data
	//////////////////////////////////////////////////////////////////////////////

	//! Camera world position.
	rr::RRVec3 pos;
	//! Camera yaw, pitch and roll in radians.
	rr::RRVec3 yawPitchRollRad;
	//! Camera view matrix.
	double   viewMatrix[16];

	//////////////////////////////////////////////////////////////////////////////
	// projection data
	//////////////////////////////////////////////////////////////////////////////

	//! Is camera orthogonal or perspective?
	bool     orthogonal;
	//! Camera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Only if perspective: Camera's vertical field of view in degrees. Must be positive and less than 180.
	float    fieldOfViewVerticalDeg;
	//! Camera's near plane distance in world units. Must be positive.
	float    anear;
	//! Camera's far plane distance in world units. Must be greater than near.
	float    afar;
	//! Only if orthogonal: World space distance between top and bottom of viewport.
	float    orthoSize;
	//! Maps view direction into screen position, default 0,0 maps view direction to screen center.
	rr::RRVec2 screenCenter;
	//! Camera projection matrix.
	double   projectionMatrix[16];

	//////////////////////////////////////////////////////////////////////////////
	// other data
	//////////////////////////////////////////////////////////////////////////////

	//! When we are constructed from light, we synchronize light's position/direction with us.
	rr::RRLight* light;
};

//! Copies matrices from camera to OpenGL pipeline, so that following primitives are transformed as if viewed by this camera.
//
//! Changes glMatrixMode to GL_MODELVIEW.
//! Note that if you modify camera inputs, changes are propagated to OpenGL only after update() and setupForRender().
RR_GL_API void setupForRender(const Camera& camera);

//! Returns last camera that executed setupForRender().
RR_GL_API const Camera* getRenderCamera();


}; // namespace

#endif //CAMERA_H
