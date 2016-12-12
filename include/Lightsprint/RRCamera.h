//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRCamera.h
//! \brief LightsprintCore | RRCamera, camera with stereo and panorama support
//----------------------------------------------------------------------------

#ifndef RRCAMERA_H
#define RRCAMERA_H

#include "RRLight.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRCamera

//! Camera - generates projection matrices for use in renderer, supports also stereo and panorama.
//
//! With yawPitchRollRad reset to 0, camera view direction is Z+ (0,0,1),
//! up vector is Y+ (0,1,0) and right vector is X- (-1,0,0).
class RR_API RRCamera
{
public:
	//////////////////////////////////////////////////////////////////////////////
	// constructors
	//////////////////////////////////////////////////////////////////////////////

	//! Initializes camera in world center.
	RRCamera();
	//! Initializes all basic parameters at once.
	RRCamera(const RRVec3& pos, const RRVec3& yawPitchRollRad, float aspect, float fieldOfViewVerticalDeg, float anear, float afar);
	//! Initializes camera from RRLight. Changes made to camera will be propagated back to light, so you must not delete light before camera.
	RRCamera(RRLight& light);


	//////////////////////////////////////////////////////////////////////////////
	// view get/setters
	//////////////////////////////////////////////////////////////////////////////

	//! Returns camera position.
	const RRVec3& getPosition() const {return pos;}
	//! Sets camera position.
	void setPosition(const RRVec3& _pos);

	//! Returns camera rotation in radians.
	//
	//! Yaw + Pitch + Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
	//! Rotations are applied in this order: yaw (around Y axis), pitch (around X axis), roll (around Z axis).
	const RRVec3& getYawPitchRollRad() const {return yawPitchRollRad;}
	//! Sets camera rotation in radians.
	//
	//! Yaw + Pitch + Roll = YXZ Euler angles as defined at http://en.wikipedia.org/wiki/Euler_angles
	//! Rotations are applied in this order: yaw (around Y axis), pitch (around X axis), roll (around Z axis).
	void setYawPitchRollRad(const RRVec3& _yawPitchRollRad);

	//! Returns normalized view direction.
	//
	//! May slightly differ from what was sent to setDirection() due to limited precision of floats.
	RRVec3 getDirection() const {return -RRVec3((RRReal)viewMatrix[2],(RRReal)viewMatrix[6],(RRReal)viewMatrix[10]);}
	//! Sets view direction, resets roll to 0.
	//
	//! Direction doesn't have to be normalized.
	//! getDirection() may return slightly different direction due to limited precision of floats.
	void setDirection(const RRVec3& dir);

	//! Returns normalized up vector.
	RRVec3 getUp() const {return RRVec3((RRReal)viewMatrix[1],(RRReal)viewMatrix[5],(RRReal)viewMatrix[9]);}
	//! Returns normalized right vector.
	RRVec3 getRight() const {return RRVec3((RRReal)viewMatrix[0],(RRReal)viewMatrix[4],(RRReal)viewMatrix[8]);}

	//! Applies transformation on top of transformations already set in view matrix. Only rotation and translation components are used, scale is ignored.
	//
	//! \param transformation Transformation to apply.
	//! \param rollChangeAllowed False makes roll constant, it won't be transformed.
	//! \param yawInversionAllowed False makes function return false and do no transformation if it would
	//!  change yaw by more than 90 and less than 270 degrees. This happens for example when pitch overflows 90 or -90 degrees.
	bool manipulateViewBy(const RRMatrix3x4& transformation, bool rollChangeAllowed=true, bool yawInversionAllowed=true);

	//! Predefined camera views.
	enum View
	{
		TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT, EXTENTS, RANDOM, OTHER // note: in case of changing order, fix getView()
	};
	//! Returns whether camera is in one of predefined orthogonal views, or OTHER if it is not.
	//! View is detected from direction or yawPitchRollRad, position and range are ignored.
	View getView() const;
	//! Sets one of predefined orthogonal camera views, or random perspective view if RANDOM is requested.
	//! Setting OTHER has no effect.
	//! If solver is provided, adjusts also position/near/far to fit surrounding geometry.
	//! Setting orthogonal view is fast, RANDOM uses raycasting (~1000 rays) to find interesting view.
	void setView(View view, const class RRSolver* solver, const RRVec3* mini, const RRVec3* maxi);

	//! Sets all parameters used to construct view matrix at once. It is slightly faster than setting all parameters one by one.
	void setView(const RRVec3& pos, const RRVec3& yawPitchRollRad);

	//! Returns 4x4 view matrix.
	const double* getViewMatrix() const {return viewMatrix;}


	//////////////////////////////////////////////////////////////////////////////
	// projection get/setters
	//////////////////////////////////////////////////////////////////////////////

	//! Returns true if camera is orthogonal rather, false if perspective.
	bool isOrthogonal() const {return orthogonal;}
	//! Makes camera orthogonal or perspective.
	void setOrthogonal(bool orthogonal);

	//! Returns aspect ratio, which is usually viewport width/height.
	float getAspect()                   const {return aspect;}
	//! Sets aspect ratio, with configurable effect on FOV.
	//! \param aspect
	//!  Your new aspect ratio, usually set to viewport width/height.
	//! \param effectOnFOV
	//!  0 to preserve vertical FOV, 1 to preserve horizontal FOV, values between 0 and 1 for blend of both options.
	//!  Default 0 is absolutely numerically stable, other values add tiny error to FOV each time setAspect is called.
	void  setAspect(float aspect, float effectOnFOV = 0);

	//! Returns vertical FOV in degrees, from 0 to 180.
	float getFieldOfViewVerticalDeg()   const {return fieldOfViewVerticalDeg;}
	//! Returns horizontal FOV in degrees, from 0 to 180.
	float getFieldOfViewHorizontalDeg() const {return RR_RAD2DEG(getFieldOfViewHorizontalRad());}
	//! Returns vertical FOV in radians, from 0 to PI.
	float getFieldOfViewVerticalRad()   const {return RR_DEG2RAD(fieldOfViewVerticalDeg);}
	//! Returns horizontal FOV in radians, from 0 to PI.
	float getFieldOfViewHorizontalRad() const {return atan(tan(getFieldOfViewVerticalRad()*0.5f)*aspect)*2;}
	//! Sets vertical FOV in degrees, from 0 to 180.
	void  setFieldOfViewVerticalDeg(float fieldOfViewVerticalDeg);
	//! Sets horizontal FOV in degrees, from 0 to 180. Converts to vertical FOV internally (numerically unstable).
	void  setFieldOfViewHorizontalDeg(float fieldOfViewHorizontalDeg);

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
	//! Uses raycasting, performance hit is acceptable even if called once per frame.
	//! In panorama modes, it shoots in all directions.
	//! \param solver
	//!  Camera is tested against geometry in given solver. Range is not adjusted if it is nullptr.
	//! \param shadowRays
	//!  True to set range using all shadow casters rather than just visible objects.
	//! \param numRays
	//!  How many rays it shoots to measure distances. Higher = more accurate, but slower.
	//!  0 for defaults (around 100).
	void  setRangeDynamically(const class RRSolver* solver, bool shadowRays, unsigned numRays = 0);

	//! Only if orthogonal: Returns world space distance between top and bottom of viewport.
	float getOrthoSize() const {return orthoSize;}
	//! Only if orthogonal: Sets world space distance between top and bottom of viewport.
	void setOrthoSize(float orthoSize);

	//! View direction is projected into given screen position, default 0,0 for screen center, -1,-1 for top left corner, 1,1 for bottom right corner.
	RRVec2 getScreenCenter() const {return screenCenter;}
	void setScreenCenter(RRVec2 screenCenter);

	//! Sets all parameters used to construct projection matrix at once. It is slightly faster than setting all parameters one by one.
	void setProjection(bool orthogonal, float aspect, float fieldOfViewVerticalDeg, float anear, float afar, float orthoSize, RRVec2 screenCenter);

	//! Returns 4x4 projection matrix.
	const double* getProjectionMatrix() const {return projectionMatrix;}


	//////////////////////////////////////////////////////////////////////////////
	// stereo
	//////////////////////////////////////////////////////////////////////////////

	enum StereoMode
	{
		SM_MONO             =0, ///< common non-stereo mode
		SM_INTERLACED       =1, ///< interlaced, with top scanline visible by right eye, for passive displays \image html stereo_interlaced.png
		SM_SIDE_BY_SIDE     =2, ///< left half is left eye \image html stereo_sidebyside.jpg
		SM_TOP_DOWN         =3, ///< top half is left eye \image html stereo_topdown.jpg
		SM_OCULUS_RIFT      =4, ///< for Oculus Rift compatible devices \image html stereo_oculus.jpg

		//! quad buffered stereo, GL_BACK_RIGHT+GL_BACK_LEFT
		//
		//! note that GPU vendors put various artificial restrictions on quad buffered stereo; as of writing this
		//! - Quadro and FirePro support quad buffered stereo
		//! - Radeon supports quad buffered stereo in fullscreen only, on HD6000 and newer
		//! - GeForce does NOT support quad buffered stereo
		//! - Intel does NOT support quad buffered stereo
		SM_QUAD_BUFFERED    =5,
		SM_OPENVR           =6, ///< for OpenVR compatible devices (HTC Vive) \image html stereo_oculus.jpg
	};

	//! One of camera stereo modes, or SM_MONO for common non-stereo render.
	StereoMode stereoMode;
	//! Swaps left and right eye.
	bool stereoSwap;
	//! For stereo effect: Distance (in meters) between left and right eye.
	float eyeSeparation;
	//! For stereo effect: Distance (in meters) between eye and objects rendered in display plane (so that closer objects appear in front of display, farther objects behind display).
	float displayDistance;
	//! Creates left and right camera.
	void getStereoCameras(RRCamera& left, RRCamera& right) const;


	//////////////////////////////////////////////////////////////////////////////
	// panorama
	//////////////////////////////////////////////////////////////////////////////

	enum PanoramaMode
	{
		PM_OFF              =0, ///< common non-panorama mode
		PM_EQUIRECTANGULAR  =1, ///< 360 degree render in equirectangular projection \image html pano-equirect.jpg
		PM_LITTLE_PLANET    =2, ///< 360 degree render in stereographic (little planet) projection \image html pano-littleplanet.jpg
		PM_FISHEYE          =3, ///< variable FOV fisheye render; suitable for dome projection with spherical mirror in center \image html pano-fisheye.jpg
	};

	enum PanoramaCoverage
	{
		PC_FULL_STRETCH     =0, ///< panorama covers whole output area \image html cover-stretch.jpg
		PC_FULL             =1, ///< panorama covers as large output area as possible while preserving aspect ratio \image html cover-full.jpg
		PC_TRUNCATE_BOTTOM  =2, ///< panorama covers full output width, going from top down, with aspect preserved. bottom might be cropped or empty, depending on output size \image html cover-trunc-bottom.jpg
		PC_TRUNCATE_TOP     =3, ///< panorama covers full output width, going from bottom up, with aspect preserved. upper part might be cropped or empty, depending on output size \image html cover-trunc-top.jpg
		PC_TRUNCATE_TOP_BOT =4, ///< panorama covers full output width, with top and bottom part equally truncated \image html cover-trunc-top-bot.jpg
	};

	//! One of camera panorama modes, or PM_OFF for common non-panorama render.
	PanoramaMode panoramaMode;
	//! One of panorama coverage modes.
	PanoramaCoverage panoramaCoverage;
	//! Panorama scale, 1 for normal size, 2 for bigger etc. Note that you can shift panorama with RRCamera::setScreenCenter().
	float panoramaScale;
	//! Fisheye field of view, 360 for full sphere, 180 for hemisphere.
	float panoramaFisheyeFovDeg;


	//////////////////////////////////////////////////////////////////////////////
	// depth of field
	//////////////////////////////////////////////////////////////////////////////

	//! For depth of field effect: Diameter of opening inside the camera lens, in meters. Wider = DOF effect is more apparent.
	float apertureDiameter;
	//! For depth of field effect: Blur pixels closer than this distance, in meters.
	float dofNear;
	//! For depth of field effect: Blur pixels farther than this distance, in meters.
	float dofFar;


	//////////////////////////////////////////////////////////////////////////////
	// blending
	//////////////////////////////////////////////////////////////////////////////

	//! Fills this with linear interpolation of two samples; sample0 if blend=0, sample1 if blend=1.
	void blendLinear(const RRCamera& sample0, const RRCamera& sample1, float blend);
	//! Fills this with Akima interpolation of samples, non uniformly scattered on time scale.
	//
	//! \param numSamples
	//!  Number of elements in samples and times arrays.
	//!  Interpolations uses at most 3 preceding and 3 following samples, providing more does not increase quality.
	//! \param samples
	//!  Pointers to numSamples samples.
	//! \param times
	//!  Array of times assigned to samples.
	//! \param time
	//!  Time assigned to sample generated by this function.
	void blendAkima(unsigned numSamples, const RRCamera** samples, RRReal* times, RRReal time);


	//////////////////////////////////////////////////////////////////////////////
	// other tools
	//////////////////////////////////////////////////////////////////////////////

	//! Converts world space position to clip space
	RRVec4 getPositionInClipSpace(RRVec3 worldPosition) const;
	//! Converts world space position to normalized device coordinates (left bottom near viewport corner in -1,-1,-1, right top far viewport corner in 1,1,1)
	RRVec3 getPositionInViewport(RRVec3 worldPosition) const;

	//! Converts position in viewport (2d) to world space ray (3d origin, 3d direction), suitable for shooting rays from camera.
	//
	//! When shooting ray from camera to screen pixel, set \code
	//! camera.getRay(positionInViewport,ray.rayOrigin,ray.rayDirection);
	//! ray.rayLengthMin = camera.getNear();
	//! ray.rayLengthMax = camera.getFar();
	//! \endcode
	//! \param positionInViewport
	//!  Position in viewport is part of normalized device coordinate, i.e. 0,0 represents center of viewport,
	//!  -1,-1 left bottom viewport corner, 1,1 right top viewport corner.
	//! \param rayOrigin
	//!  Returned ray origin.
	//! \param rayDirection
	//!  Returned ray direction.
	//!  It is not normalized, length is >=1, so that ray lengths from near to far make rays reach exactly from near to far plane.
	//! \param randomized
	//!  Randomly shifts ray to create DOF effect (when many rays are averaged).
	//! \return
	//!  False when given position does not contain image; some panorama modes don't cover whole viewport.
	bool getRay(RRVec2 positionInViewport, RRVec3& rayOrigin, RRVec3& rayDirection, bool randomized=false) const;

	//! Assignment operator.
	const RRCamera& operator=(const RRCamera& camera);
	//! == operator, true when inputs without aspect are equal. Transformations like setPosition(getPosition()) preserve identity, while setDirection(getDirection()) not, due to limited float precision.
	bool operator==(const RRCamera& a) const;
	//! != operator, true when inputs differ.
	bool operator!=(const RRCamera& a) const;

	//! Mirrors camera around general plane. Second call mirrors camera back, sometimes with small rounding errors.
	void mirror(const rr::RRVec4& plane);

	//! Fixes NaN/INF/IND values found in camera (pos, dir etc).
	//
	//! It is important before serializing camera into text stream that can't handle infinite numbers.
	//! \return Number of changes made.
	unsigned fixInvalidValues();

private:
	void updateView(bool updateLightPos, bool updateLightDir);
	void updateProjection();

	//////////////////////////////////////////////////////////////////////////////
	// view data
	//////////////////////////////////////////////////////////////////////////////

	//! RRCamera world position.
	RRVec3 pos;
	//! RRCamera yaw, pitch and roll in radians.
	RRVec3 yawPitchRollRad;
	//! RRCamera view matrix.
	double   viewMatrix[16];

	//////////////////////////////////////////////////////////////////////////////
	// projection data
	//////////////////////////////////////////////////////////////////////////////

	//! Is camera orthogonal or perspective?
	bool     orthogonal;
	//! RRCamera's aspect, horizontal field of view / vertical field of view.
	float    aspect;
	//! Only if perspective: RRCamera's vertical field of view in degrees. Must be positive and less than 180.
	float    fieldOfViewVerticalDeg;
	//! RRCamera's near plane distance in world units. Must be positive.
	float    anear;
	//! RRCamera's far plane distance in world units. Must be greater than near.
	float    afar;
	//! Only if orthogonal: World space distance between top and bottom of viewport.
	float    orthoSize;
	//! Maps view direction into screen position, default 0,0 maps view direction to screen center.
	RRVec2 screenCenter;
	//! RRCamera projection matrix.
	double   projectionMatrix[16];

	//////////////////////////////////////////////////////////////////////////////
	// other data
	//////////////////////////////////////////////////////////////////////////////

	//! When we are constructed from light, we synchronize light's position/direction with us.
	RRLight* light;
};

typedef RRVector<RRCamera> RRCameras;

}; // namespace

#endif //CAMERA_H
