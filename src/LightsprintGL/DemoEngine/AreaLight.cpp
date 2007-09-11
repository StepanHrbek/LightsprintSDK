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

	AreaLight::AreaLight(Camera* aparent, unsigned anumInstances, unsigned shadowmapSize)
	{
		parent = aparent;
		numInstancesMax = anumInstances;
		numInstances = anumInstances;
		shadowMaps = new Texture*[numInstances];
		for(unsigned i=0;i<numInstancesMax;i++)
			shadowMaps[i] = Texture::createShadowmap(shadowmapSize,shadowmapSize);
		areaType = 0;
		areaSize = 0.2f;
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

	Camera* AreaLight::getInstance(unsigned instance) const
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

	Texture* AreaLight::getShadowMap(unsigned instance) const
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps[instance];
	}

	void AreaLight::instanceMakeup(Camera& light, unsigned instance) const
	{
		if(instance>=numInstances) 
		{
			rr::RRReporter::report(rr::WARN,"AreaLight: instance %d requested, but light has only %d instances.\n",instance,numInstances);
			return;
		}

		light.update(0.3f);

		switch(areaType)
		{
			case 0: // linear
				light.pos[0] += light.right[0]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[1] += light.right[1]*(areaSize*(instance/(numInstances-1.f)*2-1));
				light.pos[2] += light.right[2]*(areaSize*(instance/(numInstances-1.f)*2-1));
				break;
			case 1: // rectangular
				{int q=(int)sqrtf((float)(numInstances-1))+1;
				light.pos[0] += light.right[0]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[0]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[1] += light.right[1]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[1]*areaSize*(instance%q/(q-1.f)-0.5f);
				light.pos[2] += light.right[2]*areaSize*(instance/q/(q-1.f)-0.5f) + light.up[2]*areaSize*(instance%q/(q-1.f)-0.5f);
				break;}
			case 2: // circular
				light.pos[0] += light.right[0]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[0]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[1] += light.right[1]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[1]*areaSize*cos(instance*2*3.14159f/numInstances);
				light.pos[2] += light.right[2]*areaSize*sin(instance*2*3.14159f/numInstances) + light.up[2]*areaSize*cos(instance*2*3.14159f/numInstances);
				break;
		}
		light.update(0.3f);
	}

}; // namespace
