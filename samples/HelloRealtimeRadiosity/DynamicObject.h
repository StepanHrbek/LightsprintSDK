// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/UberProgramSetup.h"
#include "DemoEngine/Renderer.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

class DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale,UberProgramSetup amaterial,unsigned aspecularCubeSize);
	const Model_3DS& getModel();
	void render(UberProgram* uberProgram,UberProgramSetup uberProgramSetup,AreaLight* areaLight,unsigned firstInstance,Texture* lightDirectMap,rr::RRRealtimeRadiosity* solver,const Camera& eye,float rot);
	~DynamicObject();

	rr::RRVec3 worldFoot;
private:
	DynamicObject();
	Model_3DS model;
	UberProgramSetup material;
	unsigned specularCubeSize;
	rr::RRIlluminationEnvironmentMapInOpenGL specularMap;
	rr::RRIlluminationEnvironmentMapInOpenGL diffuseMap;
	Renderer* rendererWithoutCache;
	Renderer* rendererCached;
};

#endif
