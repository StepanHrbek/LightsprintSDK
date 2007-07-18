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
	void render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,rr_gl::AreaLight* areaLight,unsigned firstInstance,rr_gl::Texture* lightDirectMap,rr_gl::Texture* lightIndirectEnvSpecular,const rr_gl::Camera& eye,float rot);

	float worldFoot[3];
private:
	DynamicObject();
	Model_3DS model;
};

#endif
