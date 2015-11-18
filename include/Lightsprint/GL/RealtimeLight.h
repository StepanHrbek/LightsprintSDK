//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RealtimeLight.h
//! \brief LightsprintGL | extends RRLight for realtime rendering with GI
//----------------------------------------------------------------------------

#ifndef REALTIMELIGHT_H
#define REALTIMELIGHT_H

#include <cmath>
#include "Texture.h"
#include "Lightsprint/RRObject.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RealtimeLight

//! Runtime extension of RRLight, with structures needed for realtime GI rendering (shadowmaps etc).
class RR_GL_API RealtimeLight
{
public:
	//! Shapes of light source area, relevant only for SPOT.
	enum AreaType
	{
		LINE,      ///< n instances in line, spot/dir-light with penumbra shadows. Approx 1m long line, it simulates light coming from long narrow lightsource.
		CIRCLE,    ///< n instances in circle, spot/dir-light with penumbra shadows. Circle, it simulates light coming from circle (border).
		RECTANGLE, ///< n instances in rectangle, spot/dir-light with penumbra shadows. Approx 1m*1m square grid, it simulates light coming from whole square. It needs more instances to prevent shadow banding.
	};

	//! Specifies how shadows of transparent materials are created (in realtime renderer), whether quickly, or more accurately.
	enum ShadowTransparency
	{
		//! The fastest, fully opaque shadows, light is completely blocked, material properties are ignored.
		FULLY_OPAQUE_SHADOWS,
		//! Fast alpha keyed shadows, light goes through or is stopped according to material, this is evaluated per-pixel.
		ALPHA_KEYED_SHADOWS,
		//! Realistic colored shadows from semi-translucent materials (if specularTransmittance is red, only red light goes through), this is evaluated per-pixel.
		RGB_SHADOWS,
		//! The most realistic shadows, with normal maps and Fresnel term applied to increase occlusion depending on index of refraction. It goes half way towards caustics, as light intensity is modulated, but light direction is not. For refractionIndex 1, result is identical to RGB_SHADOWS.
		FRESNEL_SHADOWS,
	};

	//! Extends RRLight, adding properties and functions for realtime rendering.
	//
	//! RRLight must exist at least as long as realtime light.
	//! RRLight position/direction is updated each time this->getCamera()->update() is called.
	RealtimeLight(rr::RRLight& rrlight);
	virtual ~RealtimeLight();

	//! Returns our RRLight with standard light properties like color.
	//! It's legal to edit RRLight properties at any moment as long as you call updateAfterRRLightChanges() later.
	rr::RRLight& getRRLight() {return rrlight;}
	//! Returns our RRLight with standard light properties like color.
	const rr::RRLight& getRRLight() const {return rrlight;}
	//! Propagates changes from RRLight (position,direction etc) to RealtimeLight and its camera.
	//! Note that changes in the opposite direction (from camera to RRLight) are propagated automatically.
	void updateAfterRRLightChanges();

	//! Returns camera you can use to control light's position, direction etc. Changes made to this camera are automatically propagated to original RRLight.
	rr::RRCamera* getCamera() const;
	//! Assigns new camera to this light, returns previously assigned camera.
	//
	//! You are responsible for deleting both cameras when they are no longer needed.
	//! Should not be used in new programs.
	rr::RRCamera* setCamera(rr::RRCamera* camera);

	//! Returns number of shadowmaps (depth maps or color maps).
	virtual unsigned getNumShadowmaps(bool color = false) const;

	//! Provides light with data necessary for CSM calculations in getShadowmapCamera().
	void configureCSM(const rr::RRCamera* observer, const class rr::RRSolver* solver);
	//! Returns position of observer from previous configureCSM() call.
	rr::RRVec3 getObserverPos() const;
	//! Copies camera for n-th shadowmap into out and returns it.
	virtual rr::RRCamera& getShadowmapCamera(unsigned instance, rr::RRCamera& out) const;

	//! Sets shadowmap resolution.
	//
	//! This is shortcut for setting rrlight.rtShadowmapSize and dirtyShadowmap.
	//! Set higher resolution for hard and sharper shadows,
	//! set lower resolution for area and more blurry shadows.
	void setShadowmapSize(unsigned newSize);

	//! Returns shadowmap (depth or color) for given light instance (element of area light).
	Texture* getShadowmap(unsigned instance, bool color = false);

	//! Recommends number of shadow samples for given light. Valid numbers 1,2,4,8.
	//! In case of dirlight, this is number of samples close to camera.
	void setNumShadowSamples(unsigned numSamples);
	//! Returns recommended number of shadow samples.
	//! In case of dirlight, this is number of samples close to camera.
	unsigned getNumShadowSamples() const;
	//! Returns recommended number of shadow samples for given light instance.
	unsigned getNumShadowSamples(unsigned instance) const;

	//! Sets near and far to cover scene visible by light, but not much more.
	//
	//! Uses raycasting, performance hit for one light is acceptable even if called once per frame;
	//! but it could be too much when called for many lights in every frame.
	//!
	//! Instead of directly calling setRangeDynamically(), you can set #dirtyRange, solver will call
	//! setRangeDynamically() automatically before updating shadowmaps. This way you avoid calling
	//! setRangeDynamically() twice if different parts of code request update.
	//! \param collider
	//!  Collider to be used for distance testing.
	//! \param object
	//!  Object to be used for material testing, may be nullptr.
	//! \param numRays
	//!  How many rays it shoots to measure distances. Higher = more accurate, but slower.
	//!  0 for defaults (around 300 for pointlight, 100 for spotlight).
	void setRangeDynamically(const rr::RRCollider* collider, const rr::RRObject* object, unsigned numRays = 0);

	//! Renders only shadows, no illumination. This is useful for simulating indirect shadows.
	//! Works only in presence of indirect illumination, shadows are subtracted from indirect illumination.
	//! When rendering with multiple lights, works only in first light.
	bool shadowOnly;

	//! Shape of light source area.
	//
	//! Area light is simulated by multiple spot lights.
	//! Types of area are implemented in virtual getShadowmapCamera().
	AreaType areaType;
	//! Size factor, light source size scales linearly with areaSize.
	float areaSize;

	//! Requested shadow transparency mode.
	//! It defaults to the highest quality mode, you can change it freely to reduce quality and increase speed.
	//! Renderer is still allowed to use lower(simpler) transparency modes for materials that need it, but not higher.
	//! If you set unnecessarily high mode, there's no performance penalty, renderer detects it and uses lower mode (see shadowTransparencyActual).
	ShadowTransparency shadowTransparencyRequested;
	//! Actual shadow transparency mode. For reading only, set by RRSolverGL::updateShadowmaps().
	ShadowTransparency shadowTransparencyActual;

	//! Helper for GI calculation, used by RRSolverGL.
	unsigned* smallMapCPU;
	//! Helper for GI calculation, used by RRSolverGL.
	unsigned numTriangles;

	//! Whether shadowmap needs update.
	//
	//! Set by RRSolverGL::reportDirectIlluminationChange() or manually, cleared by RRSolverGL::calculate()
	//! (unless light or shadow casting is disabled).
	bool dirtyShadowmap;
	//! Whether GI needs update.
	//
	//! Set by RRSolverGL::reportDirectIlluminationChange() and by getProjectedTexture()
	//! (see #changesInProjectedTextureAffectGI), cleared by RRSolverGL::calculate().
	bool dirtyGI;
	//! Whether shadowmap near/far range needs update.
	//
	//! Set by RRSolverGL::reportDirectIlluminationChange(), cleared by RRSolverGL::calculate()
	//! or setRangeDynamically().
	//! Range should be wide enough to not clip visible geometry, but not wider,
	//! becuase it increases shadow bias and makes distant triangles z-fight.
	//! You can set range manually or just set dirtyRange and we will recalculate range automatically.
	bool dirtyRange;
	//! Eye position when direct lighting was detected.
	//! Only for directional light.
	rr::RRVec3 positionOfLastDDI;

	//! Returns texture projected by spotlight.
	const Texture* getProjectedTexture();
	//! True = each time projected texture content changes, GI is updated.
	//! For higher performance, clear it if you know that changes have small effect on GI.
	//! Even if cleared, you can still set dirtyGI manually on big changes.
	bool changesInProjectedTextureAffectGI;

	//! Number of instances approximating area light. Used only for SPOT, 1 = standard spot, more = area light.
	unsigned numInstancesInArea;

protected:
	//! RRLight used at our creation, contains standard light properties like color.
	rr::RRLight& rrlight;
	rr::RRVec3 csmObserverPos;
	rr::RRVec3 csmObserverDir;
	float csmObserverNear;
	rr::RRVec3 csmSceneSize;
	rr::RRCamera* camera;
	bool deleteCamera;
	rr::RRVector<Texture*> shadowmaps[2]; //! Vectors of depth and color shadow maps. Sizes of vectors are updated lazily, only when map is requested and actual number of maps doesn't match.
	//! Number of samples in soft shadows, defaults to 4, you may change it to 1,2,8.
	unsigned numSoftShadowSamples;
};

typedef rr::RRVector<RealtimeLight*> RealtimeLights;

}; // namespace

#endif
