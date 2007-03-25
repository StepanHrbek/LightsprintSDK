#ifndef DYNAMICOBJECTS_H
#define DYNAMICOBJECTS_H

#include "Lightsprint/RRMath.h"
#include "Lightsprint/RRIllumination.h"
#include "AnimationFrame.h"
#include "DynamicObject.h"

/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects

class DynamicObjects
{
public:
	// creates empty pool, with 0 objects
	DynamicObjects();
	~DynamicObjects();
	
	// fills pool with objects
	void addObject(DynamicObject* dynobj);

	// actual positions in scene
	const rr::RRVec3 getPos(unsigned objIndex);
	const rr::RRReal getRot(unsigned objIndex);
	DynamicObject* getObject(unsigned objIndex);
	void setPos(unsigned objIndex, rr::RRVec3 worldFoot);

	// copy positions from animation frame to actual scene
	void copyAnimationFrameToScene(const class LevelSetup* setup, const AnimationFrame& frame, bool lightsChanged);

	// copy positions from actual scene to animation frame
	void copySceneToAnimationFrame_ignoreThumbnail(AnimationFrame& frame, const LevelSetup* setup);

	// nastavi dynamickou scenu do daneho casu od zacatku animace
	// pokud je cas mimo rozsah animace, neudela nic a vrati false
	bool setupSceneDynamicForPartTime(class LevelSetup* setup, float secondsFromStart);

	void updateSceneDynamic(LevelSetup* setup, float advanceSeconds, unsigned onlyDynaObjectNumber=1000);
	void renderSceneDynamic(rr::RRRealtimeRadiosity* solver, de::UberProgram* uberProgram, de::UberProgramSetup uberProgramSetup, de::AreaLight* areaLight, unsigned firstInstance, de::Texture* lightDirectMap) const;

private:
	std::vector<DynamicObject*> dynaobject;
	std::vector<class DynamicObjectAI*> dynaobjectAI;
};

#endif
