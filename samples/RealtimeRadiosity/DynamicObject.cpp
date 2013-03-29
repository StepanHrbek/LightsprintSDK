// --------------------------------------------------------------------------
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "DynamicObject.h"
#include "RendererOf3DS.h"
#include "../src/LightsprintIO/Import3DS/Model_3DS.h"

//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

DynamicObject* DynamicObject::create(const char* _filename,float _scale,rr_gl::UberProgramSetup _material,unsigned _reflectionCubeSize)
{
	DynamicObject* d = new DynamicObject();
	d->model = new Model_3DS;
	//d->model->smoothAll = true; // use for characters from lowpolygon3d.com
	if (d->model->Load(_filename,NULL,_scale) && d->model->numObjects)
	{
		d->material = _material;
		// create envmaps
		if (d->material.MATERIAL_DIFFUSE || d->material.MATERIAL_SPECULAR)
			d->illumination->getLayer(LAYER_ENVIRONMENT) = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,_reflectionCubeSize,_reflectionCubeSize,6,rr::BF_RGBA,true,NULL);
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
	illumination = new rr::RRObjectIllumination;
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
		worldMatrix = rr::RRMatrix3x4::translation(worldFoot)
			* rr::RRMatrix3x4::rotationByYawPitchRoll(rr::RRVec3(rotYZ[0],rotYZ[1],0)*(RR_PI/180))
			* rr::RRMatrix3x4::translation(rr::RRVec3(-model->localCenter.x,-model->localMinY,-model->localCenter.z));

	// update object's center in world coordinates
	illumination->envMapWorldCenter = worldMatrix.getTransformedPosition(model->localCenter);
}

void DynamicObject::render(rr_gl::UberProgram* uberProgram,rr_gl::UberProgramSetup uberProgramSetup,const rr::RRCamera& camera,const rr::RRVector<rr_gl::RealtimeLight*>* lights,unsigned firstInstance,const rr::RRCamera& eye, const rr::RRVec4* brightness, float gamma)
{
	if (!uberProgramSetup.LIGHT_DIRECT)
	{
		// direct light is disabled in shader so let's ignore direct lights
		// (withouth this, we would enable SHADOW_SAMPLES and it's invalid with LIGHT_DIRECT=0)
		lights = NULL;
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
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = material.MATERIAL_EMISSIVE_MAP;
		uberProgramSetup.MATERIAL_BUMP_MAP = material.MATERIAL_BUMP_MAP;
		uberProgramSetup.MATERIAL_BUMP_TYPE_HEIGHT = material.MATERIAL_BUMP_TYPE_HEIGHT;
	}
	uberProgramSetup.ANIMATION_WAVE = material.ANIMATION_WAVE;
	// temporary simplification, select only 1 light from list
	rr_gl::RealtimeLight* light = NULL;
	if (lights)
		for (unsigned i=0;i<lights->size();i++)
		{
			if (!light || ((*lights)[i]->getCamera()->getPosition()-worldFoot).length2()<(light->getCamera()->getPosition()-worldFoot).length2())
				light = (*lights)[i];
		}
	if (!light || (!light->getRRLight().castShadows))
	{
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0; // for 3ds draw, not reset by MultiPass
	}
	else
	{
		uberProgramSetup.SHADOW_CASCADE = light->getCamera()->isOrthogonal() && light->getNumShadowmaps()>1;
		uberProgramSetup.SHADOW_SAMPLES = light->getNumShadowSamples(); // for 3ds draw, not reset by MultiPass
		if (uberProgramSetup.SHADOW_SAMPLES && uberProgramSetup.FORCE_2D_POSITION) uberProgramSetup.SHADOW_SAMPLES = 1; // reduce shadow quality for DDI (even cascade)
	}

	uberProgramSetup.LIGHT_DIRECT_COLOR           = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().color!=rr::RRVec3(1);
	uberProgramSetup.LIGHT_DIRECT_MAP             = uberProgramSetup.LIGHT_DIRECT_MAP && uberProgramSetup.SHADOW_MAPS && light && light->getProjectedTexture();
	uberProgramSetup.LIGHT_DIRECT_ATT_PHYSICAL    = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::PHYSICAL;
	uberProgramSetup.LIGHT_DIRECT_ATT_POLYNOMIAL  = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::POLYNOMIAL;
	uberProgramSetup.LIGHT_DIRECT_ATT_EXPONENTIAL = uberProgramSetup.LIGHT_DIRECT && light && light->getRRLight().distanceAttenuationType==rr::RRLight::EXPONENTIAL;
	// use program
	rr_gl::Program* program = uberProgramSetup.useProgram(uberProgram,&camera,light,firstInstance,brightness,gamma,NULL);
	if (!program)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"Failed to compile or link GLSL program for dynamic object.\n"));
		return;
	}
	// use material
	uberProgramSetup.useMaterial(program,model->Materials,animationTime);
	// set matrix
	if (uberProgramSetup.OBJECT_SPACE)
	{
		rr::RRObject object;
		object.setWorldMatrix(&worldMatrix);
		uberProgramSetup.useWorldMatrix(program,&object);
	}
	// set envmap
	uberProgramSetup.useIlluminationEnvMap(program,illumination->getLayer(LAYER_ENVIRONMENT));
	// set animation
	if (uberProgramSetup.ANIMATION_WAVE)
	{
		program->sendUniform("animationTime",animationTime);
	}

	if (uberProgramSetup.MATERIAL_DIFFUSE_MAP)
		program->sendTexture("materialDiffuseMap",NULL); // activate unit, render() will bind textures

	// simple render, cached inside display list
	rendererCached->render();
}
