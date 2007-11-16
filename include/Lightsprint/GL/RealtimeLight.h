//----------------------------------------------------------------------------
//! \file RealtimeLight.h
//! \brief LightsprintGL | Extends RRLight for realtime rendering with GI
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef REALTIMELIGHT_H
#define REALTIMELIGHT_H

#include <cassert>
#include <cmath>
#include "Camera.h"
#include "Texture.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// RealtimeLight

//! Runtime extension of RRLight, with structures needed for realtime GI rendering (shadowmaps etc).
class RR_GL_API RealtimeLight
{
public:
	//! Shapes of light source area.
	enum AreaType
	{
		POINT, // point light, must have exactly 6 instances for cube shadow map generation
		LINE, // n instances in line, spot/dir-light with penumbra shadows. Approx 1m long line, it simulates light coming from long narrow lightsource.
		CIRCLE, // n instances in circle, spot/dir-light with penumbra shadows. Circle, it simulates light coming from circle (border).
		RECTANGLE, // n instances in rectangle, spot/dir-light with penumbra shadows. Approx 1m*1m square grid, it simulates light coming from whole square. It needs more instances to prevent shadow banding.
	};

	//! Creates realtime light out of standard light.
	RealtimeLight(const rr::RRLight& origin);
	//! Old interface, creates realtime spotlight out of camera.
	RealtimeLight(rr_gl::Camera* camera, unsigned numInstances, unsigned resolution);
	virtual ~RealtimeLight();

	//! Returns parent instance. Instances inherit parent's properties, so by editing parent, you edit all instances.
	Camera* getParent() const;

	//! Sets number of Class instances.
	virtual void setNumInstances(unsigned instances);
	//! Returns number of Class instances available.
	virtual unsigned getNumInstances() const;

	//! Creates and returns requested instance (element of area light).
	//! To be deleted by caller.
	virtual Camera* getInstance(unsigned instance, bool jittered = false) const;

	//! Sets shadowmap resolution.
	//! Set higher resolution for hard and sharper shadows,
	//! set lower resolution for area and more blurry shadows.
	void setShadowmapSize(unsigned newSize);

	//! Returns shadowmap for given light instance (element of area light).
	Texture* getShadowMap(unsigned instance) const;

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

	//! Helper for GI calculation, used by RRDynamicSolverGL.
	unsigned* smallMapCPU;
	//! Helper for GI calculation, used by RRDynamicSolverGL.
	unsigned numTriangles;

	//! Marks lights that need update of shadows and GI.
	bool dirty;

	//! Original RRLight used at our creation, contains additional parameters like color. May be NULL.
	const rr::RRLight* origin;

	//! Texture projected by light. May be set from outside.
	const Texture* lightDirectMap;

protected:
	bool deleteParent;
	//! Modifies light to become given instance.
	//
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
	Camera* parent;
	unsigned numInstances;
	Texture** shadowMaps;
	unsigned shadowMapSize;
};

}; // namespace

#endif
