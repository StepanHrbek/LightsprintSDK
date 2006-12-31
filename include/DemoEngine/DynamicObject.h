// --------------------------------------------------------------------------
// DemoEngine
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "Model_3DS.h"
#include "UberProgramSetup.h"
#include "RendererOf3DS.h"
#include "RendererWithCache.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

class DE_API DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale)
	{
		DynamicObject* d = new DynamicObject();
		if(d->model.Load(filename,scale) && d->getModel().numObjects)
		{
			d->rendererWithoutCache = new RendererOf3DS(&d->model);
			d->rendererCached = new RendererWithCache(d->rendererWithoutCache);
			return d;
		}
		if(!d->getModel().numObjects) printf("Model %s contains no objects.",filename);
		delete d;
		return NULL;
	}
	const Model_3DS& getModel()
	{
		return model;
	}
	rr::RRIlluminationEnvironmentMap* getSpecularMap()
	{
		return &specularMap;
	}
	rr::RRIlluminationEnvironmentMap* getDiffuseMap()
	{
		return &diffuseMap;
	}
	void render(Program* program,UberProgramSetup uberProgramSetup,rr::RRRealtimeRadiosity* solver,const Camera& eye,float rot)
	{
		// set matrices
		rr::RRVec3 worldCenter;
		rr::RRVec3 localCenter = getModel().localCenter;
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
				uberProgramSetup.MATERIAL_SPECULAR?16:0, uberProgramSetup.MATERIAL_SPECULAR?getSpecularMap():NULL,
				uberProgramSetup.MATERIAL_DIFFUSE?4:0, uberProgramSetup.MATERIAL_DIFFUSE?getDiffuseMap():NULL);
			if(uberProgramSetup.MATERIAL_SPECULAR)
			{
				glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
				getSpecularMap()->bindTexture();
				program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
			}
			if(uberProgramSetup.MATERIAL_DIFFUSE)
			{
				glActiveTexture(GL_TEXTURE0+TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
				getDiffuseMap()->bindTexture();
			}
			glActiveTexture(GL_TEXTURE0+TEXTURE_2D_MATERIAL_DIFFUSE);
		}
		// render
		rendererCached->render(); // cached inside display list
		//model.Draw(NULL); // non cached
	}
	~DynamicObject()
	{
		delete rendererCached;
		delete rendererWithoutCache;
	}
	rr::RRVec3 worldFoot;
private:
	DynamicObject()
	{
		rendererWithoutCache = NULL;
		rendererCached = NULL;
		worldFoot = rr::RRVec3(0);
	}
	Model_3DS model;
	rr::RRIlluminationEnvironmentMapInOpenGL specularMap;
	rr::RRIlluminationEnvironmentMapInOpenGL diffuseMap;
	Renderer* rendererWithoutCache;
	Renderer* rendererCached;
};

#endif
