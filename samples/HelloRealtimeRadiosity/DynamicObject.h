// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/UberProgramSetup.h"
#include "DemoEngine/Renderer.h"
#include "RRGPUOpenGL.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

//! 3ds object packed with material settings, environment maps and position in world.
class DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale,de::UberProgramSetup amaterial,unsigned aspecularCubeSize);
	const de::Model_3DS& getModel();
	void render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,de::Texture* lightDirectMap,rr::RRRealtimeRadiosity* solver,const de::Camera& eye,float rot);
	~DynamicObject();

	rr::RRVec3 worldFoot;
private:
	DynamicObject();
	de::Model_3DS model;
	de::UberProgramSetup material;
	unsigned specularCubeSize;
	rr::RRIlluminationEnvironmentMap* specularMap;
	rr::RRIlluminationEnvironmentMap* diffuseMap;
	de::Renderer* rendererWithoutCache;
	de::Renderer* rendererCached;
};

#endif
