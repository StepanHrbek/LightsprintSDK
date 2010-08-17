//----------------------------------------------------------------------------
//! \file RealtimeLight.h
//! \brief LightsprintGL | Extends RRLight for realtime rendering with GI
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2010
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef REALTIMELIGHT_H
#define REALTIMELIGHT_H

#include <cmath>
#include "Camera.h"
#include "Texture.h"
#include "Lightsprint/RRVector.h"

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

	//! Specifies how shadows of transparent materials are created, whether quickly, or more precisely.
	enum TransparentMaterialShadows
	{
		FULLY_OPAQUE_SHADOWS, ///< All materials will be fully opaque for light (when creating shadows). This is the fastest technique.
		ALPHA_KEYED_SHADOWS,  ///< All materials will be fully opaque or fully transparent for light. This is evaluated per-pixel.
		RGB_SHADOWS,          ///< not yet implemented
	};

	//! Extends RRLight, adding properties and functions for realtime rendering.
	//
	//! RRLight must exist at least as long as realtime light.
	//! RRLight position/direction is updated each time this->getParent()->update() is called.
	RealtimeLight(rr::RRLight& rrlight);
	virtual ~RealtimeLight();

	//! Returns our RRLight with standard light properties like color.
	//! It's legal to edit RRLight properties at any moment as long as you call updateAfterRRLightChanges() later.
	rr::RRLight& getRRLight() {return rrlight;}
	//! Returns our RRLight with standard light properties like color.
	const rr::RRLight& getRRLight() const {return rrlight;}
	//! Updates RealtimeLight after changes made to RRLight.
	void updateAfterRRLightChanges();
	//! Updates RRLight after changes made to RealtimeLight (after changes made to getParent()->pos/dir).
	void updateAfterRealtimeLightChanges();

	//! Returns parent instance. Instances inherit parent's properties, so by editing parent, you edit all instances.
	Camera* getParent() const;
	//! Sets parent instance, returns old parent.
	//! You are responsible for deleting both parents when they are no longer needed.
	//! Should not be used in new programs.
	Camera* setParent(Camera* parent);

	//! Returns number of shadowmaps.
	virtual unsigned getNumShadowmaps() const;

	//! Provides light with data necessary for CSM calculations in getShadowmapCamera().
	void configureCSM(Camera* observer, rr::RRObject* scene);
	//! Creates and returns requested instance (element of area light).
	//! To be deleted by caller.
	virtual Camera* getShadowmapCamera(unsigned instance, bool jittered = false) const;

	//! Sets shadowmap resolution.
	//! Set higher resolution for hard and sharper shadows,
	//! set lower resolution for area and more blurry shadows.
	void setShadowmapSize(unsigned newSize);
	//! Returns shadowmap size previously set by setShadowmapSize().
	unsigned getShadowmapSize() {return shadowmapSize;}

	//! Returns shadowmap for given light instance (element of area light).
	Texture* getShadowmap(unsigned instance);

	//! Recommends number of shadow samples for given light. Valid numbers 1,2,4,8.
	//! In case of dirlight, this is number of samples close to camera.
	void setNumShadowSamples(unsigned numSamples);
	//! Returns recommended number of shadow samples.
	//! In case of dirlight, this is number of samples close to camera.
	unsigned getNumShadowSamples() const;
	//! Returns recommended number of shadow samples for given light instance.
	unsigned getNumShadowSamples(unsigned instance) const;

	//! Renders only shadows, no illumination. This is useful for simulating indirect shadows.
	//! Works only in presence of indirect illumination, shadows are subtracted from indirect illumination.
	//! When rendering with multiple lights, works only in first light.
	bool shadowOnly;

	//! Shape of light source area.
	//
	//! Area light is simulated by multiple spot lights.
	//! Types of area are implemented in virtual instanceMakeup().
	//! If you are interested in better control over area type,
	//! let us know, we can quickly add new types or example
	//! of custom areaType, using custom instanceMakeup().
	AreaType areaType;
	//! Size factor, light source size scales linearly with areaSize.
	float areaSize;

	//! Specifies how shadows of transparent materials are created.
	TransparentMaterialShadows transparentMaterialShadows;

	//! Helper for GI calculation, used by RRDynamicSolverGL.
	unsigned* smallMapCPU;
	//! Helper for GI calculation, used by RRDynamicSolverGL.
	unsigned numTriangles;

	//! Whether shadowmap needs update.
	//! Set by RRDynamicSolverGL::reportDirectIlluminationChange(), cleared by RRDynamicSolverGL::calculate().
	bool dirtyShadowmap;
	//! Whether GI needs update.
	//! Set by RRDynamicSolverGL::reportDirectIlluminationChange() and by getProjectedTexture()
	//! (see #changesInProjectedTextureAffectGI), cleared by RRDynamicSolverGL::calculate().
	bool dirtyGI;
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

	//! Modifies light to become given instance.
	//
	//! Inputs are not update()d, outputs are update()d.
	//! \param light
	//!  This is in/out parameter.
	//!  On input, it is copy of parent spotlight.
	//!  On output, it is expected to be given instance.
	//!  Instances may differ in any property.
	//!  Typically, they slightly differ in light position and direction,
	//!  which creates affect of area light.
	//!  You can tweak this code to support set of completely different spotlights,
	//!  with arbitrary positions/directions.
	//! \param instance
	//!  Number of instance to be create, 0..numInstances-1.
	//! \param jittered
	//!  Makes instances slightly jittered (in subpixel scale) to improve penumbra quality.
	//!  Jitter is deterministic.
	//!  \n For full effect, it should be enabled only when generating OR using shadowmap, not in both cases.
	//!  It is enabled in UberProgramSetup::useProgram(), so set it false when generating shadowmaps.
	virtual void instanceMakeup(Camera& light, unsigned instance, bool jittered) const;
	rr::RRVec3 csmObserverPos;
	rr::RRVec3 csmSceneSize;
	Camera* parent;
	bool deleteParent;
	rr::RRVector<Texture*> shadowmaps; //! Vector of shadow maps. Size of vector is updated lazily, only when map is requested and actual number of maps doesn't match.
	unsigned shadowmapSize;
	//! Number of samples in soft shadows, defaults to 4, you may change it to 1,2,8.
	unsigned numSoftShadowSamples;
};

typedef rr::RRVector<RealtimeLight*> RealtimeLights;

}; // namespace

#endif
