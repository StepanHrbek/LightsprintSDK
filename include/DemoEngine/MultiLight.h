// --------------------------------------------------------------------------
// DemoEngine
// AreaLight, provides multiple generated instances of light for area light simulation.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef MULTILIGHT_H
#define MULTILIGHT_H

#include <cassert>
#include "MultiInstance.h"
#include "Camera.h"
#include "Texture.h"

/////////////////////////////////////////////////////////////////////////////
//
// MultiLightWithShadowmaps

class MultiLightWithShadowmaps : public MultiInstanceWithParentAndInstances<Camera>
{
public:
	MultiLightWithShadowmaps(unsigned anumInstances, unsigned shadowmapSize)
	{
		numInstancesMax = anumInstances;
		setNumInstances(anumInstances);
		shadowMaps = new Texture*[numInstances];
		for(unsigned i=0;i<numInstancesMax;i++)
			shadowMaps[i] = Texture::createShadowmap(shadowmapSize,shadowmapSize);
	}
	~MultiLightWithShadowmaps()
	{
		for(unsigned i=0;i<numInstancesMax;i++)
			delete shadowMaps[i];
		delete[] shadowMaps;
	}
	Texture* getShadowMap(unsigned instance)
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps[instance];
	}
protected:
	Texture** shadowMaps;
	unsigned numInstancesMax;
};


/////////////////////////////////////////////////////////////////////////////
//
// AreaLight

class AreaLight : public MultiLightWithShadowmaps
{
public:
	AreaLight(unsigned anumInstances, unsigned shadowmapSize)
		: MultiLightWithShadowmaps(anumInstances,shadowmapSize)
	{
		areaType = 0;
		areaSize = 0.15f;
	}
	unsigned areaType; // 0=linear, 1=square grid, 2=circle
	float areaSize; // size of area light
protected:
	virtual void instanceMakeup(Camera& light, unsigned instance)
	{
		if(numInstances<=1) return;
		switch(areaType)
		{
			case 0: // linear
				light.angle += 2*areaSize*(instance/(numInstances-1.f)-0.5f);
				light.height += -0.4f*instance/numInstances;
				break;
			case 1: // rectangular
				{int q=(int)sqrtf((float)(numInstances-1))+1;
				light.angle += areaSize*(instance/q/(q-1.f)-0.5f);
				light.height += (instance%q/(q-1.f)-0.5f);}
				break;
			case 2: // circular
				light.angle += sin(instance*2*3.14159f/numInstances)*0.5f*areaSize;
				light.height += cos(instance*2*3.14159f/numInstances)*0.5f;
				break;
		}
		light.update(0.3f);
	}
};

#endif
