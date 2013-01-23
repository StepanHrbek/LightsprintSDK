// --------------------------------------------------------------------------
// DynamicObject, 3d object without dynamic global illumination.
// Copyright (C) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "GL/glew.h"
#include "DynamicObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale)
{
	DynamicObject* d = new DynamicObject();
	if (d->model.Load(filename,NULL,scale) && d->model.numObjects)
	{
		return d;
	}
	if (!d->model.numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

void DynamicObject::render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,const rr::RRCamera& camera,rr_gl::RealtimeLight* light,unsigned firstInstance,rr::RRBuffer* lightIndirectEnvSpecular,const rr::RRCamera& eye,float rot)
{
	// use program
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,&camera,light,firstInstance,NULL,1,NULL);
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program for dynamic object.\n"));
		return;
	}
	// use material
	uberProgramSetup.useMaterial(program,model.Materials);
	// use environment map
	uberProgramSetup.useIlluminationEnvMap(program,lightIndirectEnvSpecular);
	// set matrices
	if (uberProgramSetup.OBJECT_SPACE)
	{
		rr::RRObject object;
		object.setWorldMatrix(&( 
			rr::RRMatrix3x4::translation(worldFoot)
			* rr::RRMatrix3x4::rotationByYawPitchRoll(rr::RRVec3(rot*RR_PI/180,0,0))
			* rr::RRMatrix3x4::translation(rr::RRVec3(-model.localCenter.x,-model.localMinY,-model.localCenter.z)) ));
		uberProgramSetup.useWorldMatrix(program,&object);
	}
	// render
	if (uberProgramSetup.MATERIAL_DIFFUSE_MAP)
		program->sendTexture("materialDiffuseMap",NULL); // activate unit, Draw will bind textures
	model.Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);
}
