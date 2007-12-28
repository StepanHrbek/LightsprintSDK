// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "DynamicObject.h"
#include "../Import3DS/RendererOf3DS.h"
#include "../Import3DS/Model_3DS.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* _filename,float _scale,rr_gl::UberProgramSetup _material,unsigned _gatherCubeSize,unsigned _specularCubeSize)
{
	DynamicObject* d = new DynamicObject();
	d->model = new Model_3DS;
	//d->model->smoothAll = true; // use for characters from lowpolygon3d.com
	if(d->model->Load(_filename,_scale) && d->model->numObjects)
	{
		d->material = _material;
		d->gatherCubeSize = _gatherCubeSize;
		d->specularCubeSize = _specularCubeSize;
		// create envmaps
		if(d->material.MATERIAL_DIFFUSE)
			d->illumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,1,1,6,rr::BF_RGBF,true,NULL);
		if(d->material.MATERIAL_SPECULAR)
			d->illumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,1,1,6,rr::BF_RGBF,true,NULL);
		d->updatePosition();

		// simple renderer
		d->rendererWithoutCache = new RendererOf3DS(d->model,true,_material.MATERIAL_DIFFUSE_MAP,_material.MATERIAL_EMISSIVE_MAP);
		d->rendererCached = d->rendererWithoutCache->createDisplayList();
		d->amdBugWorkaround = false;
		char* renderer = (char*)glGetString(GL_RENDERER);
		if(renderer)
		{
			// find 4digit number
			unsigned number = 0;
#define IS_DIGIT(c) ((c)>='0' && (c)<='9')
			for(unsigned i=0;renderer[i];i++)
				if(!IS_DIGIT(renderer[i]) && IS_DIGIT(renderer[i+1]) && IS_DIGIT(renderer[i+2]) && IS_DIGIT(renderer[i+3]) && IS_DIGIT(renderer[i+4]) && !IS_DIGIT(renderer[i+5]))
				{
					number = (renderer[i+1]-'0')*1000 + (renderer[i+2]-'0')*100 + (renderer[i+3]-'0')*10 + (renderer[i+4]-'0');
					break;
				}

			// workaround for Catalyst bug, observed on HD2xxx and HD3xxx
			if( (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")) && (number>=2000 && number<=3999) )
				d->amdBugWorkaround = true;
		}

		return d;
	}
	if(!d->model->numObjects) printf("Model %s contains no objects.\n",_filename);
	delete d;
	return NULL;
}

DynamicObject::DynamicObject()
{
	model = NULL;
	rendererWithoutCache = NULL;
	rendererCached = NULL;
	illumination = new rr::RRObjectIllumination(0);
	worldFoot = rr::RRVec3(0);
	rotYZ = rr::RRVec2(0);
	animationTime = 0;
	visible = true;
}

DynamicObject::~DynamicObject()
{
	delete rendererCached;
	delete rendererWithoutCache;
	delete illumination;
	delete model;
}

void DynamicObject::updatePosition()
{
	// set object's world matrix
	if(model)
	{
		/*
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
		glRotatef(rotYZ[1],0,0,1);
		glRotatef(rotYZ[0],0,1,0);
		if(model) glTranslatef(-model->localCenter.x,-model->localMinY,-model->localCenter.z);
		glGetFloatv(GL_MODELVIEW_MATRIX,worldMatrix);
		glPopMatrix();
		*/
		float sz = sin(rotYZ[1]/180*3.14159f);
		float cz = cos(rotYZ[1]/180*3.14159f);
		float sy = sin(rotYZ[0]/180*3.14159f);
		float cy = cos(rotYZ[0]/180*3.14159f);
		float mx = -model->localCenter.x;
		float my = -model->localMinY;
		float mz = -model->localCenter.z;
		worldMatrix[0] = cz*cy;
		worldMatrix[1] = sz*cy;
		worldMatrix[2] = -sy;
		worldMatrix[3] = 0;
		worldMatrix[4] = -sz;
		worldMatrix[5] = cz;
		worldMatrix[6] = 0;
		worldMatrix[7] = 0;
		worldMatrix[8] = cz*sy;
		worldMatrix[9] = sz*sy;
		worldMatrix[10] = cy;
		worldMatrix[11] = 0;
		worldMatrix[12] = cz*cy*mx-sz*my+cz*sy*mz+worldFoot[0];
		worldMatrix[13] = sz*cy*mx+cz*my+sz*sy*mz+worldFoot[1];
		worldMatrix[14] = -sy*mx+cy*mz+worldFoot[2];
		worldMatrix[15] = 1;
	}

	// update object's center in world coordinates
	illumination->envMapWorldCenter = rr::RRVec3(
		model->localCenter.x*worldMatrix[0]+model->localCenter.y*worldMatrix[4]+model->localCenter.z*worldMatrix[ 8]+worldMatrix[12],
		model->localCenter.x*worldMatrix[1]+model->localCenter.y*worldMatrix[5]+model->localCenter.z*worldMatrix[ 9]+worldMatrix[13],
		model->localCenter.x*worldMatrix[2]+model->localCenter.y*worldMatrix[6]+model->localCenter.z*worldMatrix[10]+worldMatrix[14]);

	// update other illumination params
	illumination->diffuseEnvMapSize = material.MATERIAL_DIFFUSE?4:0;
	illumination->specularEnvMapSize = material.MATERIAL_SPECULAR?specularCubeSize:0;
	illumination->gatherEnvMapSize = MAX(gatherCubeSize,illumination->specularEnvMapSize);
}

void DynamicObject::render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,const rr::RRVector<rr_gl::RealtimeLight*>* lights,unsigned firstInstance,const rr_gl::Camera& eye, const rr::RRVec4* brightness, float gamma)
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
	uberProgramSetup.ANIMATION_WAVE = material.ANIMATION_WAVE;
	// temporary simplification, select only 1 light from list
	const rr_gl::RealtimeLight* light = NULL;
	if(lights)
		for(unsigned i=0;i<lights->size();i++)
		{
			if(!light || ((*lights)[i]->getParent()->pos-worldFoot).length2()<(light->getParent()->pos-worldFoot).length2()	)
				light = (*lights)[i];
		}
	if(!light || (light->origin && !light->origin->castShadows))
	{
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
	}
	uberProgramSetup.LIGHT_DIRECT_COLOR         = uberProgramSetup.LIGHT_DIRECT && light && light->origin && light->origin->color!=rr::RRVec3(1);
	uberProgramSetup.LIGHT_DIRECT_MAP           = uberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light && light->lightDirectMap;
	uberProgramSetup.LIGHT_DISTANCE_PHYSICAL    = uberProgramSetup.LIGHT_DIRECT && light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::PHYSICAL;
	uberProgramSetup.LIGHT_DISTANCE_POLYNOMIAL  = uberProgramSetup.LIGHT_DIRECT && light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::POLYNOMIAL;
	uberProgramSetup.LIGHT_DISTANCE_EXPONENTIAL = uberProgramSetup.LIGHT_DIRECT && light && light->origin && light->origin->distanceAttenuationType==rr::RRLight::EXPONENTIAL;
	// use program
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,light,firstInstance,brightness,gamma);
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
			glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
			if(illumination->specularEnvMap)
				rr_gl::getTexture(illumination->specularEnvMap)->bindTexture();
			else
				assert(0); // have you called updateIllumination()?
			program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
		}
		if(uberProgramSetup.MATERIAL_DIFFUSE)
		{
			glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
			if(illumination->diffuseEnvMap)
				rr_gl::getTexture(illumination->diffuseEnvMap)->bindTexture();
			else
				assert(0); // have you called updateIllumination()?
		}
		// activate previously active texture
		//  sometimes it's diffuse, sometimes emissive
		glActiveTexture(activeTexture);
	}
	// set animation
	if(uberProgramSetup.ANIMATION_WAVE)
	{
		program->sendUniform("animationTime",animationTime);
	}

	if(amdBugWorkaround)
		// simple render, non cached
		model->Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);
	else
		// simple render, cached inside display list
		rendererCached->render();
}
