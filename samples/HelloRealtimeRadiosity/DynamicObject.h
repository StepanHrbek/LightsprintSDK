// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "Lightsprint/RRRealtimeRadiosity.h"
#include "Lightsprint/DemoEngine/Model_3DS.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h"
#include "Lightsprint/DemoEngine/Renderer.h"
#include "Lightsprint/RRGPUOpenGL.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

//! 3ds object packed with material settings, environment maps and position in world.
class DynamicObject
{
public:
	//! Creates dynamic object from .3ds file.
	static DynamicObject* create(const char* filename,float scale,de::UberProgramSetup amaterial,unsigned aspecularCubeSize);

	//! Returns reference to our .3ds model.
	const de::Model_3DS& getModel();

	//! Updates object's position according to worldFoot position and rot rotation.
	//! To be called each time object moves/rotates.
	void updatePosition();

	//! Updates object's illumination.
	//! Expects that position was already updated or it hasn't changed.
	//! No need to call it when object is not visible and won't be rendered.
	void updateIllumination(rr::RRRealtimeRadiosity* solver);

	//! Renders object.
	//! Expects that illumination was already updated or it hasn't changed.
	//! No need to call it when object is not visible.
	void render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,de::Texture* lightDirectMap,const de::Camera& eye);

	~DynamicObject();

	// object's interface for movement, freely changeable from outside
	rr::RRVec3 worldFoot;
	rr::RRVec2 rotYZ; // y is rotated first, then z
	bool visible;

	// updated by updateIllumination, public only for save & load
	rr::RRIlluminationEnvironmentMap* specularMap;
	rr::RRIlluminationEnvironmentMap* diffuseMap;

protected:
	DynamicObject();
	de::Model_3DS model;
	de::UberProgramSetup material;
	unsigned specularCubeSize;
	de::Renderer* rendererWithoutCache;
	de::Renderer* rendererCached;

	// updated by updatePosition
	float worldMatrix[16];
};

#endif
