// --------------------------------------------------------------------------
// DemoEngine
// AreaLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstring> // NULL
#include "Lightsprint/GL/AreaLight.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

	AreaLight::AreaLight(Camera* _parent, unsigned _numInstances, unsigned _shadowMapSize, AreaType _areaType)
	{
		parent = _parent;
		numInstancesMax = _numInstances;
		numInstances = _numInstances;
		shadowMaps = new Texture*[numInstances];
		shadowMapSize = _shadowMapSize;
		for(unsigned i=0;i<numInstancesMax;i++)
			shadowMaps[i] = Texture::createShadowmap(shadowMapSize,shadowMapSize);
		areaType = _areaType;
		areaSize = 0.2f;
	}

	AreaLight::~AreaLight()
	{
		for(unsigned i=0;i<numInstancesMax;i++)
			delete shadowMaps[i];
		delete[] shadowMaps;
	}

	Camera* AreaLight::getParent() const
	{
		return parent;
	}

	void AreaLight::setNumInstances(unsigned instances)
	{
		numInstances = instances;
	}

	unsigned AreaLight::getNumInstances() const
	{
		return numInstances;
	}

	Camera* AreaLight::getInstance(unsigned instance, bool jittered) const
	{
		if(!parent || instance>=numInstances)
			return NULL;
		Camera* c = new Camera(*parent);
		if(!c)
			return NULL;
		instanceMakeup(*c,instance,jittered);
		return c;
	}

	void AreaLight::setShadowmapSize(unsigned newSize)
	{
		shadowMapSize = newSize;
		for(unsigned i=0;i<numInstances;i++)
		{
			shadowMaps[i]->reset(newSize,newSize,Texture::TF_NONE,NULL,false);
		}
	}

	Texture* AreaLight::getShadowMap(unsigned instance) const
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps[instance];
	}

	void AreaLight::instanceMakeup(Camera& light, unsigned instance, bool jittered) const
	{
		if(instance>=numInstances) 
		{
			rr::RRReporter::report(rr::WARN,"AreaLight: instance %d requested, but light has only %d instances.\n",instance,numInstances);
			return;
		}
		if(numInstances==1)
		{
			return; // only 1 instance -> use unmodified parent
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
