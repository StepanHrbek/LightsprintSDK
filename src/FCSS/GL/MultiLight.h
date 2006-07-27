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
	MultiLightWithShadowmaps()
	{
		shadowMaps = new TextureShadowMap[MAX_INSTANCES];
	}
	~MultiLightWithShadowmaps()
	{
		delete[] shadowMaps;
	}
	TextureShadowMap* getShadowMap(unsigned instance)
	{
		if(instance>=MAX_INSTANCES)
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
	AreaLight()
	{
		areaType = 0;
	}
	unsigned areaType; // 0=linear, 1=square grid, 2=circle
protected:
	virtual void instanceMakeup(Camera& light, unsigned instance)
	{
		if(numInstances<=1) return;
		switch(areaType)
		{
			case 0: // linear
				light.angle += 2*AREA_SIZE*(instance/(numInstances-1.)-0.5);
				light.height += -0.4*instance/numInstances;
				break;
			case 1: // rectangular
				{int q=(int)sqrtf(numInstances-1)+1;
				light.angle += AREA_SIZE*(instance/q/(q-1.)-0.5);
				light.height += (instance%q/(q-1.)-0.5);}
				break;
			case 2: // circular
				light.angle += sin(instance*2*3.14159/numInstances)*0.5*AREA_SIZE;
				light.height += cos(instance*2*3.14159/numInstances)*0.5;
				break;
		}
		light.update(0.3f);
	}
};

#endif
