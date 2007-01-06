// --------------------------------------------------------------------------
// DynamicObject, 3d object without global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/UberProgramSetup.h"
#include "DemoEngine/Renderer.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

class DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale);
	const Model_3DS& getModel();
	void render(UberProgram* uberProgram,UberProgramSetup uberProgramSetup,AreaLight* areaLight,unsigned firstInstance,Texture* lightDirectMap,const Camera& eye,float rot);

	float worldFoot[3];
private:
	DynamicObject();
	Model_3DS model;
	UberProgramSetup material;
};

#endif
