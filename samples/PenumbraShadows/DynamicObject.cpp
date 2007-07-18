// --------------------------------------------------------------------------
// DynamicObject, 3d object without dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "GL/glew.h"
#include "DynamicObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale)
{
	DynamicObject* d = new DynamicObject();
	if(d->model.Load(filename,scale) && d->model.numObjects)
	{
		return d;
	}
	if(!d->model.numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
}

void DynamicObject::render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,de::Texture* lightDirectMap,de::Texture* lightIndirectEnvSpecular,const de::Camera& eye,float rot)
{
	// use program
	de::Program* program = uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap,NULL,1);
	if(!program)
	{
		printf("Failed to compile or link GLSL program for dynamic object.\n");
		return;
	}
	// set specular environment map
	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		GLint activeTexture;
		glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
		glActiveTexture(GL_TEXTURE0+de::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
		lightIndirectEnvSpecular->bindTexture();
		program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
		glActiveTexture(activeTexture);
	}
	// set matrices
	float m[16];
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
	glRotatef(rot,0,1,0);
	glTranslatef(-model.localCenter.x,-model.localMinY,-model.localCenter.z);
	glGetFloatv(GL_MODELVIEW_MATRIX,m);
	glPopMatrix();
	program->sendUniform("worldMatrix",m,false,4);
	// render
	model.Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);
}
