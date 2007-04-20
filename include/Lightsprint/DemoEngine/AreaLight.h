// --------------------------------------------------------------------------
// DemoEngine
// AreaLight, provides multiple generated instances of spotlight for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef AREALIGHT_H
#define AREALIGHT_H

#include <cassert>
#include <cmath>
#include "Camera.h"
#include "Texture.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// AreaLight

//! Area light with shadowmaps for realtime area light soft shadows.
class DE_API AreaLight
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
	virtual void instanceMakeup(Camera& light, unsigned instance) const;
	Camera* parent;
	unsigned numInstances;
	Texture** shadowMaps;
	unsigned numInstancesMax;
};

}; // namespace

#endif
