// --------------------------------------------------------------------------
// RealtimeLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <cstring> // NULL
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/RRDebug.h"
#include <GL/glew.h>

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// RealtimeLight

	RealtimeLight::RealtimeLight(const rr::RRLight& _rrlight)
	{
		deleteParent = true;
		origin = &_rrlight;
		//smallMapGPU = Texture::create(NULL,w,h,false,Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		smallMapCPU = NULL;
		numTriangles = 0;
		dirtyShadowmap = true;
		dirtyGI = true;

		parent = new Camera(_rrlight);
		shadowMapSize = (_rrlight.type==rr::RRLight::DIRECTIONAL)?2048:1024;
		areaType = (_rrlight.type==rr::RRLight::POINT)?POINT:LINE;
		areaSize = 0.2f;
		transparentMaterialShadows = ALPHA_KEYED_SHADOWS;
		lightDirectMap = NULL;
		numInstances = 0;
		shadowMaps = NULL;
		setNumInstances(_rrlight.castShadows?((_rrlight.type==rr::RRLight::POINT)?6:((_rrlight.type==rr::RRLight::DIRECTIONAL)?2:1)):0);
	}

	RealtimeLight::RealtimeLight(rr_gl::Camera* _camera, unsigned _numInstances, unsigned _resolution)
	{
		deleteParent = false;
		origin = NULL;
		//smallMapGPU = Texture::create(NULL,w,h,false,Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		smallMapCPU = NULL;
		numTriangles = 0;
		dirtyShadowmap = true;
		dirtyGI = true;

		parent = _camera;
		shadowMapSize = _resolution;
		areaType = LINE;
		areaSize = 0.2f;
		lightDirectMap = NULL;
		numInstances = 0;
		shadowMaps = NULL;
		setNumInstances(_numInstances);
	}

	RealtimeLight::~RealtimeLight()
	{
		delete[] smallMapCPU;
		if(deleteParent) delete getParent();
		setNumInstances(0);
	}

	Camera* RealtimeLight::getParent() const
	{
		return parent;
	}

	void RealtimeLight::setNumInstances(unsigned instances)
	{
		if(instances!=numInstances)
		{
			for(unsigned i=0;i<numInstances;i++)
				delete shadowMaps[i];
			SAFE_DELETE_ARRAY(shadowMaps);
			numInstances = instances;
			if(numInstances)
			{
				shadowMaps = new Texture*[numInstances];
				for(unsigned i=0;i<numInstances;i++)
					shadowMaps[i] = Texture::createShadowmap(shadowMapSize,shadowMapSize);
			}
		}
	}

	unsigned RealtimeLight::getNumInstances() const
	{
		return numInstances;
	}

	Camera* RealtimeLight::getInstance(unsigned instance, bool jittered) const
	{
		if(!parent || instance>=numInstances)
			return NULL;
		Camera* c = new Camera(*parent);
		if(!c)
			return NULL;
		instanceMakeup(*c,instance,jittered);
		return c;
	}

	void RealtimeLight::setShadowmapSize(unsigned newSize)
	{
		shadowMapSize = newSize;
		for(unsigned i=0;i<numInstances;i++)
		{
			shadowMaps[i]->getBuffer()->reset(rr::BT_2D_TEXTURE,newSize,newSize,1,rr::BF_DEPTH,false,NULL);
			shadowMaps[i]->reset(false,false);
		}
	}

	Texture* RealtimeLight::getShadowMap(unsigned instance) const
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps[instance];
	}

	void RealtimeLight::instanceMakeup(Camera& light, unsigned instance, bool jittered) const
	{
		if(instance>=numInstances) 
		{
			rr::RRReporter::report(rr::WARN,"RealtimeLight: instance %d requested, but light has only %d instances.\n",instance,numInstances);
			return;
		}
		if(numInstances==1)
		{
			light.update();
			return; // only 1 instance -> use unmodified parent
		}
		if(origin && origin->type==rr::RRLight::DIRECTIONAL)
		{
			// setup second map in cascade
			RR_ASSERT(numInstances==2); // dir must have 1 or 2 instances
			if(instance==1)
			{
				light.orthoSize *= 0.2f; // cascade goes in 5x size steps
			}
			light.update();
			return;
		}

		//light.update(0.3f);

		switch(areaType)
		{
			case LINE:
				// edit inputs, update outputs
				light.pos[0] += light.right[0]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[1] += light.right[1]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[2] += light.right[2]*(areaSize*(instance/(numInstances-1.f)*2-1));
				break;
			case RECTANGLE:
				// edit inputs, update outputs
				{int q=(int)sqrtf((float)(numInstances-1))+1;
				light.pos[0] += light.right[0]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[0]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[1] += light.right[1]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[1]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[2] += light.right[2]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[2]*areaSize*(instance%q/(q-1.f)-0.5f);
				break;}
			case CIRCLE:
				// edit inputs, update outputs
				light.pos[0] += light.right[0]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[0]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[1] += light.right[1]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[1]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[2] += light.right[2]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[2]*areaSize*cos(instance*2*3.14159f/numInstances);
				break;
			case POINT:
				// edit output (view matrix) to avoid rounding errors, inputs stay unmodified
				RR_ASSERT(instance<6);
				light.update();
				light.rotateViewMatrix(instance%6);
				return;
		}
		if(jittered)
		{
			static signed char jitterSample[10][2] = {{0,0},{3,-2},{-2,3},{1,2},{-2,-1},{3,4},{-4,-3},{2,-1},{-1,1},{-3,0}};
			light.angle += light.fieldOfView*light.aspect/360*2*3.14159f/shadowMapSize*jitterSample[instance%10][0]*0.22f;
			light.angleX += light.fieldOfView/360*2*3.14159f/shadowMapSize*jitterSample[instance%10][1]*0.22f;
		}
		light.update();
	}

}; // namespace
