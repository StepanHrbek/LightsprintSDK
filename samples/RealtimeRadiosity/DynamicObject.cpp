// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/Renderer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "DynamicObject.h"
#include "../src/LightsprintIO/Import3DS/RendererOf3DS.h"
#include "../src/LightsprintIO/Import3DS/Model_3DS.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* _filename,float _scale,rr_gl::UberProgramSetup _material,unsigned _gatherCubeSize,unsigned _specularCubeSize)
{
	DynamicObject* d = new DynamicObject();
	d->model = new Model_3DS;
	//d->model->smoothAll = true; // use for characters from lowpolygon3d.com
	if (d->model->Load(_filename,_scale) && d->model->numObjects)
	{
		d->material = _material;
		// create envmaps
		if (d->material.MATERIAL_DIFFUSE)
			d->illumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGBA,true,NULL);
		if (d->material.MATERIAL_SPECULAR)
			d->illumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,_specularCubeSize,_specularCubeSize,6,rr::BF_RGBA,true,NULL);
		d->illumination->gatherEnvMapSize = _gatherCubeSize;
		d->updatePosition();

		// simple renderer, one draw call, display list of many arrays
		d->rendererWithoutCache = new RendererOf3DS(d->model,true,_material.MATERIAL_DIFFUSE_MAP,_material.MATERIAL_EMISSIVE_MAP);
		d->rendererWithoutCache->render(); // render once without cache to create textures, avoid doing so in display list
		d->rendererCached = d->rendererWithoutCache->createDisplayList();
		return d;
	}
	if (!d->model->numObjects) rr::RRReporter::report(rr::WARN,"Model %s contains no objects.\n",_filename);
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
	if (model)
	{
		/*
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(worldFoot[0],worldFoot[1],worldFoot[2]);
		glRotatef(rotYZ[1],0,0,1);
		glRotatef(rotYZ[0],0,1,0);
		if (model) glTranslatef(-model->localCenter.x,-model->localMinY,-model->localCenter.z);
		glGetFloatv(GL_MODELVIEW_MATRIX,worldMatrix);
		glPopMatrix();
		*/
		float sz = sin(RR_DEG2RAD(rotYZ[1]));
		float cz = cos(RR_DEG2RAD(rotYZ[1]));
		float sy = sin(RR_DEG2RAD(rotYZ[0]));
		float cy = cos(RR_DEG2RAD(rotYZ[0]));
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
}

void DynamicObject::render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,const rr::RRVector<rr_gl::RealtimeLight*>* lights,unsigned firstInstance,const rr_gl::Camera& eye, const rr::RRVec4* brightness, float gamma, float waterLevel)
{
	if (!uberProgramSetup.LIGHT_DIRECT)
	{
		// direct light is disabled in shader so let's ignore direct lights
		// (withouth this, we would enable SHADOW_SAMPLES and it's invalid with LIGHT_DIRECT=0)
		lights = NULL;
	}
	if (uberProgramSetup.LIGHT_INDIRECT_auto)
	{
		uberProgramSetup.LIGHT_INDIRECT_auto = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
	}
	// mix uberProgramSetup with our material setup
	// but only when indirect illum is on.
	// when indirect illum is off, do nothing, it's probably render of shadow into shadowmap.
	if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
	{
		uberProgramSetup.MATERIAL_DIFFUSE = material.MATERIAL_DIFFUSE;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = material.MATERIAL_DIFFUSE_MAP;
		uberProgramSetup.MATERIAL_SPECULAR = material.MATERIAL_SPECULAR;
		uberProgramSetup.MATERIAL_SPECULAR_CONST = material.MATERIAL_SPECULAR_CONST;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = material.MATERIAL_SPECULAR_MAP;
		uberProgramSetup.MATERIAL_NORMAL_MAP = material.MATERIAL_NORMAL_MAP;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = material.MATERIAL_EMISSIVE_MAP;
	}
	uberProgramSetup.ANIMATION_WAVE = material.ANIMATION_WAVE;
	// temporary simplification, select only 1 light from list
	rr_gl::RealtimeLight* light = NULL;
	if (lights)
		for (unsigned i=0;i<lights->size();i++)
		{
			if (!light || ((*lights)[i]->getParent()->pos-worldFoot).length2()<(light->getParent()->pos-worldFoot).length2())
				light = (*lights)[i];
		}
	if (!light || (!light->getRRLight().castShadows))
	{
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0; // for 3ds draw, not reset by MultiPass
	}
	else
	{
		uberProgramSetup.SHADOW_CASCADE = light->getParent()->orthogonal && light->getNumShadowmaps()>1;
		uberProgramSetup.SHADOW_SAMPLES = light->getNumShadowSamples(0); // for 3ds draw, not reset by MultiPass
		//if (uberProgramSetup.SHADOW_SAMPLES) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for moving objects // don't reduce, looks bad in Lightsmark
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.SHADOW_CASCADE) uberProgramSetup.SHADOW_SAMPLES = 4; // increase shadow quality for cascade (even moving objects)
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.FORCE_2D_POSITION) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for DDI (even cascade)
	}

	uberProgramSetup.LIGHT_DIRECT_COLOR           = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().color!=rr::RRVec3(1);
	uberProgramSetup.LIGHT_DIRECT_MAP             = uberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light && light->getProjectedTexture();
	uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL    = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
	uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL  = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
	uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
	// use program
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,light,firstInstance,brightness,gamma,waterLevel);
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program for dynamic object.\n"));
		return;
	}
	// set matrix
	if (uberProgramSetup.OBJECT_SPACE)
	{
		program->sendUniform("worldMatrix",worldMatrix,false,4);
	}
	// set material
	//  3ds renderer sets nearly everything, we only have to set this:
	if (uberProgramSetup.MATERIAL_SPECULAR_CONST)
	{
		program->sendUniform("materialSpecularConst",.5f,.5f,.5f,1.0f);
	}
	// set envmap
	if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
	{
		// backup active texture
		GLint activeTexture;
		glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);

		uberProgramSetup.useIlluminationEnvMaps(program,illumination,false);

		// activate previously active texture
		//  sometimes it's diffuse, sometimes emissive
		glActiveTexture(activeTexture);
	}
	// set animation
	if (uberProgramSetup.ANIMATION_WAVE)
	{
		program->sendUniform("animationTime",animationTime);
	}

	// simple render, cached inside display list
	rendererCached->render();
}
