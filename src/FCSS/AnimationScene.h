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
	float scale;
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
	Frames::iterator getFrameByIndex(unsigned index);

	// returns time from animation start in seconds to given frame
	float getFrameTime(unsigned index) const;

	// returns total length of animation in seconds
	float getTotalTime() const;

	// returns frame for given time (0..length), NULL for times outside range
	const AnimationFrame* getFrameByTime(float absSeconds);

	unsigned LevelSetup::getFrameIndexByTime(float absSeconds, float* transitionDone, float* transitionTotal);

	const de::Texture* getOverlay();
private:
	char overlayFilename[300];
	de::Texture* overlayMap;
};

#endif
