//----------------------------------------------------------------------------
//! \file AreaLight.h
//! \brief LightsprintGL | provides multiple spotlights for area light simulation
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef AREALIGHT_H
#define AREALIGHT_H

#include <cassert>
#include <cmath>
#include "Camera.h"
#include "Texture.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// AreaLight

//! Area light with shadowmaps for realtime area light soft shadows.
class RR_GL_API AreaLight
{
public:
	//! Creates area light.
	//! \param parent
	//!  Parent instance. This light will always copy settings from parent instance.
	//!  If you later change parent, new instances will change too.
	//! \param numInstancesMax
	//!  Max number of instances supported by later calls to setNumInstance.
	//!  Instances are area light elements, more instances make smoother soft shadow.
	//!  Area light is initially set to the same (max) number of instance.
	//! \param shadowmapSize
	//!  Resolution of shadowmaps will be shadowmapSize * shadowmapSize texels.
	//!  Set higher resolution for hard and sharper shadows,
	//!  set lower resolution for area and more blurry shadows.
	AreaLight(Camera* parent, unsigned numInstancesMax, unsigned shadowmapSize);
	virtual ~AreaLight();

	//! Returns parent instance
	const Camera* getParent() const;

	//! Sets number of Class instances.
	virtual void setNumInstances(unsigned instances);
	//! Returns number of Class instances available.
	virtual unsigned getNumInstances() const;

	//! Creates and returns requested instance (element of area light).
	//! To be deleted by caller.
	virtual Camera* getInstance(unsigned instance) const;

	//! Sets shadowmap resolution.
	//! Set higher resolution for hard and sharper shadows,
	//! set lower resolution for area and more blurry shadows.
	void setShadowmapSize(unsigned newSize);

	//! Returns shadowmap for given light instance (element of area light).
	Texture* getShadowMap(unsigned instance) const;

	//! Shape of light source area, 0=line (default), 1=square, 2=circle
	//
	//! Area light is simulated by multiple spot lights.
	//! Depending on areaType, they are set in
	//! - 0 = approx 1m long line, it simulates light coming from long narrow lightsource.
	//! - 1 = approx 1m*1m square grid, it simulates light coming from whole square. It needs more instances to prevent shadow banding.
	//! - 2 = circle, it simulates light coming from circle (border).
	//!
	//! Types of area are implemented in virtual instanceMakeup().
	//! If you are interested in better control over area type,
	//! let us know, we can quickly add example
	//! of custom areaType, using custom instanceMakeup().
	unsigned areaType;
	//! Size factor, light source size scales linearly with areaSize.
	float areaSize;
protected:
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
	virtual void instanceMakeup(Camera& light, unsigned instance) const;
	Camera* parent;
	unsigned numInstances;
	Texture** shadowMaps;
	unsigned numInstancesMax;
};

}; // namespace

#endif
