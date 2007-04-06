// --------------------------------------------------------------------------
// DemoEngine
// AreaLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstring> // NULL
#include "Lightsprint/DemoEngine/AreaLight.h"

namespace de
{

	AreaLight::AreaLight(Camera* aparent, unsigned anumInstances, unsigned shadowmapSize)
	{
		parent = aparent;
		numInstancesMax = anumInstances;
		numInstances = anumInstances;
		shadowMaps = new Texture*[numInstances];
		for(unsigned i=0;i<numInstancesMax;i++)
			shadowMaps[i] = Texture::createShadowmap(shadowmapSize,shadowmapSize);
		areaType = 0;
		areaSize = 0.15f;
	}

	AreaLight::~AreaLight()
	{
		for(unsigned i=0;i<numInstancesMax;i++)
			delete shadowMaps[i];
		delete[] shadowMaps;
	}

	const Camera* AreaLight::getParent() const
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

	Camera* AreaLight::getInstance(unsigned instance)
	{
		if(!parent || instance>numInstances)
			return NULL;
		Camera* c = new Camera(*parent);
		if(!c)
			return NULL;
		instanceMakeup(*c,instance);
		return c;
	}

	void AreaLight::setShadowmapSize(unsigned newSize)
	{
		for(unsigned i=0;i<numInstances;i++)
		{
			shadowMaps[i]->reset(newSize,newSize,Texture::TF_NONE,NULL,false);
		}
	}

	Texture* AreaLight::getShadowMap(unsigned instance)
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps[instance];
	}

	void AreaLight::instanceMakeup(Camera& light, unsigned instance)
	{
		if(numInstances<=1) return;
		switch(areaType)
		{
			case 0: // linear
				light.angle += 2*areaSize*(instance/(numInstances-1.f)-0.5f);
				light.angleX += -0.4f*areaSize*instance/numInstances;
				break;
			case 1: // rectangular
				{int q=(int)sqrtf((float)(numInstances-1))+1;
				light.angle += areaSize*(instance/q/(q-1.f)-0.5f);
				light.angleX += areaSize*(instance%q/(q-1.f)-0.5f);}
				break;
			case 2: // circular
				light.angle += sin(instance*2*3.14159f/numInstances)*0.5f*areaSize;
				light.angleX += cos(instance*2*3.14159f/numInstances)*0.5f*areaSize;
				break;
		}
		light.update(0.3f);
	}

}; // namespace
