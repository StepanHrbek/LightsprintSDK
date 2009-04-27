// --------------------------------------------------------------------------
// DynamicObject, 3d object without dynamic global illumination.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "GL/glew.h"
#include "DynamicObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale)
{
	DynamicObject* d = new DynamicObject();
	if (d->model.Load(filename,scale) && d->model.numObjects)
	{
		return d;
	}
	if (!d->model.numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
}

void DynamicObject::render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,rr_gl::RealtimeLight* light,unsigned firstInstance,rr_gl::Texture* lightIndirectEnvSpecular,const rr_gl::Camera& eye,float rot)
{
	// use program
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,light,firstInstance,NULL,1);
	if (!program)
	{
		LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program for dynamic object.\n"));
		return;
	}
	// set specular environment map
	if (uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
	{
		if (!lightIndirectEnvSpecular)
		{
			LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Rendering dynamic object with NULL cubemap.\n"));
			return;
		}
		GLint activeTexture;
		glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
		glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
		lightIndirectEnvSpecular->bindTexture();
		glActiveTexture(activeTexture);
	}
	// set matrices
	if (uberProgramSetup.OBJECT_SPACE)
	{
		float m[16];
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
		glRotatef(rot,0,1,0);
		glTranslatef(-model.localCenter.x,-model.localMinY,-model.localCenter.z);
		glGetFloatv(GL_MODELVIEW_MATRIX,m);
		glPopMatrix();
		program->sendUniform("worldMatrix",m,false,4);
	}
	// render
	model.Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);
}
