// --------------------------------------------------------------------------
// DynamicObject, 3d object without dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "DemoEngine/RendererOf3DS.h"
#include "DemoEngine/RendererWithCache.h"
#include "DynamicObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale)
{
	DynamicObject* d = new DynamicObject();
	if(d->model.Load(filename,scale) && d->getModel().numObjects)
	{
		return d;
	}
	if(!d->getModel().numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
}

const Model_3DS& DynamicObject::getModel()
{
	return model;
}

void DynamicObject::render(UberProgram* uberProgram,UberProgramSetup uberProgramSetup,AreaLight* areaLight,unsigned firstInstance,Texture* lightDirectMap,const Camera& eye,float rot)
{
	// use program
	Program* program = uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap);
	if(!program)
	{
		printf("Failed to compile or link GLSL program for dynamic object.\n");
		return;
	}
	// set matrices
	float m[16];
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
	glRotatef(rot,0,1,0);
	glTranslatef(-getModel().localCenter.x,-getModel().localMinY,-getModel().localCenter.z);
	glGetFloatv(GL_MODELVIEW_MATRIX,m);
	glPopMatrix();
	program->sendUniform("worldMatrix",m,false,4);
	// render
	model.Draw(NULL,NULL,NULL);
}
