#ifndef MULTILIGHT_H
#define MULTILIGHT_H

#include "MultiInstance.h"
#include "Camera.h"
#include "TextureShadowMap.h"


/////////////////////////////////////////////////////////////////////////////
//
// MultiLightWithShadowmaps

class MultiLightWithShadowmaps : public MultiInstanceWithParentAndInstances<Camera>
{
public:
	MultiLightWithShadowmaps(unsigned anumInstances)
	{
		setNumInstances(anumInstances);
		shadowMaps = new TextureShadowMap[numInstances];
	}
	~MultiLightWithShadowmaps()
	{
		delete[] shadowMaps;
	}
	TextureShadowMap* getShadowMap(unsigned instance)
	{
		if(instance>=numInstances)
		{
			assert(0);
			return NULL;
		}
		return shadowMaps+instance;
	}
protected:
	TextureShadowMap* shadowMaps;
};


/////////////////////////////////////////////////////////////////////////////
//
// AreaLight

class AreaLight : public MultiLightWithShadowmaps
{
public:
	AreaLight(unsigned anumInstances)
		: MultiLightWithShadowmaps(anumInstances)
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
				light.angle += 2*areaSize*(instance/(numInstances-1.)-0.5);
				light.height += -0.4*instance/numInstances;
				break;
			case 1: // rectangular
				{int q=(int)sqrtf(numInstances-1)+1;
				light.angle += areaSize*(instance/q/(q-1.)-0.5);
				light.height += (instance%q/(q-1.)-0.5);}
				break;
			case 2: // circular
				light.angle += sin(instance*2*3.14159/numInstances)*0.5*areaSize;
				light.height += cos(instance*2*3.14159/numInstances)*0.5;
				break;
		}
		light.update(0.3f);
	}
};

#endif
