#include <assert.h>
#include <time.h>
#include "RRVisionApp.h"

namespace rrVision
{

#define TIME    clock_t            
#define GETTIME clock()
#define PER_SEC CLOCKS_PER_SEC

RRAppFramework::RRAppFramework()
{
	surfaces = NULL;
	scene = NULL;
	resultChannelIndex = 0;
	dirtyMaterials = true;
	dirtyGeometry = true;
	dirtyLights = true;
}

RRAppFramework::~RRAppFramework()
{
	delete scene;
	delete surfaces;
}

void RRAppFramework::setObjects(Objects& aobjects)
{
	objects = aobjects;
	dirtyGeometry = true;
}

void RRAppFramework::setResultChannel(unsigned channelIndex)
{
	resultChannelIndex = channelIndex;
}

void RRAppFramework::adjustScene()
{
	scene->setScaler(RRScaler::createRgbScaler());
}

void RRAppFramework::reportAction(const char* action) const
{
}

void RRAppFramework::reportMaterialChange()
{
	dirtyMaterials = true;
}

void RRAppFramework::reportGeometryChange()
{
	dirtyGeometry = true;
}

void RRAppFramework::reportLightChange()
{
	dirtyLights = true;
}

static bool endByTime(void *context)
{
	return GETTIME>*(TIME*)context;
}

void RRAppFramework::calculate()
{
	bool dirtyFactors = false;
	bool dirtyEnergies = false;
	if(dirtyMaterials)
	{
		dirtyMaterials = false;
		dirtyFactors = true;
		detectMaterials();
	}
	if(dirtyGeometry)
	{
		dirtyGeometry = false;
		dirtyLights = true;
		dirtyFactors = true;
		delete scene;
		scene = new RRScene;
		for(Objects::iterator i=objects.begin();i!=objects.end();i++)
			scene->objectCreate((*i)->getObjectImporter());
		//!!! slepit multiobject
	}
	if(dirtyLights)
	{
		dirtyLights = false;
		dirtyEnergies = true;
		detectDirectIllumination();
	}
	if(dirtyFactors)
	{
		dirtyFactors = false;
		dirtyEnergies = false;
		scene->illuminationReset(true);
	}
	if(dirtyEnergies)
	{
		dirtyEnergies = false;
		scene->illuminationReset(false);
	}

	TIME now = GETTIME;
	static const float calcstep = 0.1f;
	TIME end = (TIME)(now+calcstep*PER_SEC);
	//if((now-lastInteractionTime)/(float)PER_SEC<TIME_TO_START_IMPROVING) return;
	scene->illuminationImprove(endByTime,(void*)&end);
}

} // namespace
