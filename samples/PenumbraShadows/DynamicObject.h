// --------------------------------------------------------------------------
// DynamicObject, 3d object without global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "../src/LightsprintIO/Import3DS/Model_3DS.h"
#include "Lightsprint/GL/UberProgramSetup.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

//! This sample class shows completely customized handling of dynamic objects
//! - loading from disk (3ds, source code included)
//! - rendering (source code included)
//! - moving (source code included)
//! 
//! Compare: Samples like RealtimeLights, Lightmaps are _much shorter_,
//! they get the same things done using built-in loaders/renderer.
//! (They don't have any dynamic object class, dynamic object is just any RRObject
//!  sent to solver->setDynamicObjects(), and rendering is automatic and faster).
class DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale);
	void render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,rr_gl::RealtimeLight* light,unsigned firstInstance,rr::RRBuffer* lightIndirectEnvSpecular,const rr_gl::Camera& eye,float rot);

	float worldFoot[3];
private:
	Model_3DS model;
	rr::RRObjectIllumination illumination;
};

#endif
