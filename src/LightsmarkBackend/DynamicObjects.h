#ifndef DYNAMICOBJECTS_H
#define DYNAMICOBJECTS_H

#include "Lightsprint/RRScene.h"
#include "AnimationFrame.h"

/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects

class DynamicObjects : public rr::RRObjects
{
public:
	~DynamicObjects();

	bool addObject(const char* filename, float scale);

	// actual positions in scene
	rr::RRVec3 getPos(unsigned objIndex) const;
	void setPos(unsigned objIndex, rr::RRVec3 worldFoot);
//	rr::RRVec2 getRot(unsigned objIndex) const;
//	void setRot(unsigned objIndex, rr::RRVec2 rot);

	// copy positions from animation frame to actual scene
	// returns true when object in scene changed position or rotation
	bool copyAnimationFrameToScene(const class LevelSetup* setup, const AnimationFrame& frame, bool lightsChanged);

	// copy positions from actual scene to animation frame
	void copySceneToAnimationFrame_ignoreThumbnail(AnimationFrame& frame, const LevelSetup* setup);

	// automaticky rotuje objekty, pouzij kdyz je benchmark pauznuty
//	void advanceRot(float seconds);

private:
	std::vector<rr::RRScene*> scenesToBeDeleted;
};

#endif
