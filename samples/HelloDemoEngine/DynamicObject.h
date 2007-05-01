// --------------------------------------------------------------------------
// DynamicObject, 3d object without global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "../Import3DS/Model_3DS.h"
#include "Lightsprint/DemoEngine/UberProgramSetup.h"
#include "Lightsprint/DemoEngine/Renderer.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

//! 3ds object packed with position in world.
class DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale);
	const Model_3DS& getModel();
	void render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,de::Texture* lightDirectMap,de::Texture* lightIndirectEnvSpecular,const de::Camera& eye,float rot);

	float worldFoot[3];
private:
	DynamicObject();
	Model_3DS model;
};

#endif
