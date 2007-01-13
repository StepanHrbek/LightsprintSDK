// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "DemoEngine/Renderer.h"
#include "DynamicObject.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale,de::UberProgramSetup amaterial,unsigned aspecularCubeSize)
{
	DynamicObject* d = new DynamicObject();
	if(d->model.Load(filename,scale) && d->getModel().numObjects)
	{
		d->rendererWithoutCache = de::Renderer::create3DSRenderer(&d->model);
		d->rendererCached = d->rendererWithoutCache->createDisplayList();
		d->material = amaterial;
		d->specularCubeSize = aspecularCubeSize;
		return d;
	}
	if(!d->getModel().numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
	rendererWithoutCache = NULL;
	rendererCached = NULL;
	worldFoot = rr::RRVec3(0);
}

DynamicObject::~DynamicObject()
{
	delete rendererCached;
	delete rendererWithoutCache;
}

const de::Model_3DS& DynamicObject::getModel()
{
	return model;
}

void DynamicObject::render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,de::Texture* lightDirectMap,rr::RRRealtimeRadiosity* solver,const de::Camera& eye,float rot)
{
	// mix uberProgramSetup with our material setup
	// avoid fancy materials when envmaps are off - could be render to shadowmap
	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		uberProgramSetup.MATERIAL_DIFFUSE = material.MATERIAL_DIFFUSE;
		uberProgramSetup.MATERIAL_DIFFUSE_COLOR = material.MATERIAL_DIFFUSE_COLOR;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = material.MATERIAL_DIFFUSE_MAP;
		uberProgramSetup.MATERIAL_SPECULAR = material.MATERIAL_SPECULAR;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = material.MATERIAL_SPECULAR_MAP;
		uberProgramSetup.MATERIAL_NORMAL_MAP = material.MATERIAL_NORMAL_MAP;
	}
	// use program
	de::Program* program = uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap);
	if(!program)
	{
		printf("Failed to compile or link GLSL program for dynamic object.\n");
		return;
	}
	// set matrices
	rr::RRVec3 worldCenter;
	rr::RRVec3 localCenter = rr::RRVec3(getModel().localCenter.x,getModel().localCenter.y,getModel().localCenter.z);
	rr::RRVec3 localFoot = rr::RRVec3(localCenter.x,getModel().localMinY,localCenter.z);
	float m[16];
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
	glRotatef(rot,0,1,0);
	glTranslatef(-localFoot[0],-localFoot[1],-localFoot[2]);
	glGetFloatv(GL_MODELVIEW_MATRIX,m);
	glPopMatrix();
	program->sendUniform("worldMatrix",m,false,4);
	worldCenter = rr::RRVec3(
		localCenter[0]*m[0]+localCenter[1]*m[4]+localCenter[2]*m[ 8]+m[12],
		localCenter[0]*m[1]+localCenter[1]*m[5]+localCenter[2]*m[ 9]+m[13],
		localCenter[0]*m[2]+localCenter[1]*m[6]+localCenter[2]*m[10]+m[14]);
	// set envmap
	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		solver->updateEnvironmentMaps(worldCenter,16,
			uberProgramSetup.MATERIAL_SPECULAR?specularCubeSize:0, uberProgramSetup.MATERIAL_SPECULAR?&specularMap:NULL,
			uberProgramSetup.MATERIAL_DIFFUSE?4:0, uberProgramSetup.MATERIAL_DIFFUSE?&diffuseMap:NULL);
		if(uberProgramSetup.MATERIAL_SPECULAR)
		{
			glActiveTexture(GL_TEXTURE0+de::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
			specularMap.bindTexture();
			program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
		}
		if(uberProgramSetup.MATERIAL_DIFFUSE)
		{
			glActiveTexture(GL_TEXTURE0+de::TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
			diffuseMap.bindTexture();
		}
		glActiveTexture(GL_TEXTURE0+de::TEXTURE_2D_MATERIAL_DIFFUSE);
	}
	// render
	rendererCached->render(); // cached inside display list
	//model.Draw(NULL); // non cached
}
