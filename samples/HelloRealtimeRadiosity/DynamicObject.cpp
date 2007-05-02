// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include "Lightsprint/DemoEngine/Renderer.h"
#include "Lightsprint/RRGPUOpenGL.h"
#include "DynamicObject.h"
#include "../Import3DS/RendererOf3DS.h"
#include "../Import3DS/Model_3DS.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* filename,float scale,de::UberProgramSetup amaterial,unsigned aspecularCubeSize)
{
	DynamicObject* d = new DynamicObject();
	d->model = new Model_3DS;
	//d->model->smoothAll = true; // use for characters from lowpolygon3d.com
	if(d->model->Load(filename,scale) && d->model->numObjects)
	{
		d->rendererWithoutCache = new RendererOf3DS(d->model,true,amaterial.MATERIAL_DIFFUSE_MAP,amaterial.MATERIAL_EMISSIVE_MAP);
		d->rendererCached = d->rendererWithoutCache->createDisplayList();
		d->material = amaterial;
		d->specularCubeSize = aspecularCubeSize;
		return d;
	}
	if(!d->model->numObjects) printf("Model %s contains no objects.\n",filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
	model = NULL;
	rendererWithoutCache = NULL;
	rendererCached = NULL;
	specularCubeSize = 0;
	diffuseMap = NULL;
	specularMap = NULL;
	worldFoot = rr::RRVec3(0);
	rotYZ = rr::RRVec2(0);
	visible = true;
	updatePosition();
}

DynamicObject::~DynamicObject()
{
	delete model;
	delete specularMap;
	delete diffuseMap;
	delete rendererCached;
	delete rendererWithoutCache;
}

void DynamicObject::updatePosition()
{
	// set matrices
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
	glRotatef(rotYZ[1],0,0,1);
	glRotatef(rotYZ[0],0,1,0);
	if(model)
		glTranslatef(-model->localCenter.x,-model->localMinY,-model->localCenter.z);
	glGetFloatv(GL_MODELVIEW_MATRIX,worldMatrix);
	glPopMatrix();
}

void DynamicObject::updateIllumination(rr::RRDynamicSolver* solver)
{
	// create envmaps if they don't exist yet
	if(material.MATERIAL_DIFFUSE && !diffuseMap)
		diffuseMap = rr_gl::RRDynamicSolverGL::createIlluminationEnvironmentMap();
	if(material.MATERIAL_SPECULAR && !specularMap)
		specularMap = rr_gl::RRDynamicSolverGL::createIlluminationEnvironmentMap();
	// compute object's center in world coordinates
	rr::RRVec3 worldCenter = rr::RRVec3(
		model->localCenter.x*worldMatrix[0]+model->localCenter.y*worldMatrix[4]+model->localCenter.z*worldMatrix[ 8]+worldMatrix[12],
		model->localCenter.x*worldMatrix[1]+model->localCenter.y*worldMatrix[5]+model->localCenter.z*worldMatrix[ 9]+worldMatrix[13],
		model->localCenter.x*worldMatrix[2]+model->localCenter.y*worldMatrix[6]+model->localCenter.z*worldMatrix[10]+worldMatrix[14]);
	// update envmaps
	solver->updateEnvironmentMaps(worldCenter,MAX(16,specularCubeSize),
		material.MATERIAL_SPECULAR?specularCubeSize:0, material.MATERIAL_SPECULAR?specularMap:NULL,
		material.MATERIAL_DIFFUSE?4:0, material.MATERIAL_DIFFUSE?diffuseMap:NULL);
}

void DynamicObject::render(de::UberProgram* uberProgram,de::UberProgramSetup uberProgramSetup,de::AreaLight* areaLight,unsigned firstInstance,const de::Texture* lightDirectMap,const de::Camera& eye, const float brightness[4], float gamma)
{
	// mix uberProgramSetup with our material setup
	// but only when indirect illum is on.
	// when indirect illum is off, do nothing, it's probably render of shadow into shadowmap.
	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		uberProgramSetup.MATERIAL_DIFFUSE = material.MATERIAL_DIFFUSE;
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = material.MATERIAL_DIFFUSE_VCOLOR;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = material.MATERIAL_DIFFUSE_MAP;
		uberProgramSetup.MATERIAL_SPECULAR = material.MATERIAL_SPECULAR;
		uberProgramSetup.MATERIAL_SPECULAR_CONST = material.MATERIAL_SPECULAR_CONST;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = material.MATERIAL_SPECULAR_MAP;
		uberProgramSetup.MATERIAL_NORMAL_MAP = material.MATERIAL_NORMAL_MAP;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = material.MATERIAL_EMISSIVE_MAP;
	}
	// use program
	de::Program* program = uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap,brightness,gamma);
	if(!program)
	{
		printf("Failed to compile or link GLSL program for dynamic object.\n");
		return;
	}
	// set matrix
	program->sendUniform("worldMatrix",worldMatrix,false,4);
	// set envmap
	if(uberProgramSetup.LIGHT_INDIRECT_ENV)
	{
		GLint activeTexture;
		glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
		if(uberProgramSetup.MATERIAL_SPECULAR)
		{
			glActiveTexture(GL_TEXTURE0+de::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
			if(specularMap)
				specularMap->bindTexture();
			else
				assert(0); // have you called updateIllumination()?
			program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
		}
		if(uberProgramSetup.MATERIAL_DIFFUSE)
		{
			glActiveTexture(GL_TEXTURE0+de::TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
			if(diffuseMap)
				diffuseMap->bindTexture();
			else
				assert(0); // have you called updateIllumination()?
		}
		// activate previously active texture
		//  sometimes it's diffuse, sometimes emissive
		glActiveTexture(activeTexture);
	}
	// render
	rendererCached->render(); // cached inside display list
	//model->Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,NULL,NULL); // non cached
}
