#ifndef ANIMATIONSCENE_H
#define ANIMATIONSCENE_H

#include <list>
#include "AnimationFrame.h"
#include "DynamicObject.h"

/////////////////////////////////////////////////////////////////////////////
//
// LevelSetup, all data from .ani file in editable form

class DynamicObjectLoadable : public DynamicObject
{
public:
	DynamicObjectLoadable(FILE* f);
	void save(FILE* f);
};

class LevelSetup
{
public:
	// constant setup
	const char* filename;
	rr::RRDynamicSolver::CalculateParams calculateParams;
	float scale;
	float minFeatureSize;
	bool renderWater;
	float waterLevel;
	typedef std::list<AnimationFrame*> Frames;
	Frames frames;
	std::vector<unsigned> objects; // objects in scene, indices into global dynamic object pool

	LevelSetup(const char* afilename);
	~LevelSetup();

	// load all from .ani
	// afilename is name of scene, e.g. path/koupelna4.3ds
	bool load(const char* afilename);

	// save all to .ani file
	bool save() const;

	// returns if layer number is available (not used and >=2)
	bool isOkForNewLayerNumber(unsigned layerNumber);
	// returns layer number not used by our frames
	unsigned newLayerNumber();

	// returns iterator to n-th frame
	Frames::iterator getFrameIterByIndex(unsigned index);
	// returns n-th frame
	AnimationFrame* getFrameByIndex(unsigned index);

	// returns time from animation start in seconds to given frame
	float getFrameTime(unsigned index) const;

	// returns total length of animation in seconds
	float getTotalTime() const;

	// returns frame for given abs time (0..length), NULL for times outside range
	// warning: time->frame conversion is not reliable
	//          frame to be used by player, not by editor
	const AnimationFrame* getFrameByTime(float absSeconds);

	// returns frame index and time inside frame for given abs time
	// warning: time->frame conversion is not reliable (when frame has 0sec)
	//          frame number to be used by player, not by editor
	// if hintFrameIndex is specified, it is used despite possible rounding errors (transitionDone slightly subzero etc)
	unsigned getFrameIndexByTime(float absSeconds, float* transitionDone, float* transitionTotal);
};

#endif
