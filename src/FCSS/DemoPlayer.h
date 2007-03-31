#ifndef DEMOPLAYER_H
#define DEMOPLAYER_H

#include <vector>
#include "Lightsprint/RRIllumination.h"

class DemoPlayer
{
public:
	DemoPlayer(const char* demoCfg, bool supportEditor);
	~DemoPlayer();
	void advance(float seconds);

	void setPaused(bool paused);
	bool getPaused();

	float getDemoLength();
	float getDemoPosition();

	unsigned getPartIndex();
	unsigned getNumParts();
	float getPartLength(unsigned part = 0xffff); // default = current part, otherwise index of part, 0..3
	float getPartPosition();
	void  setPartPosition(float seconds);

	float getMusicLength();
	float getMusicPosition();

	class DynamicObjects* getDynamicObjects();

	class Level* getNextPart();

private:
	bool paused;
	float demoTime; // 0..demo duration in seconds
	float partStart; // 0..demo duration in seconds, time when current part started

	// music
	class Music* music;

	// sky
	rr::RRIlluminationEnvironmentMap* skyMap;

	// objects
	DynamicObjects* dynamicObjects;

	// scenes
	unsigned nextSceneIndex;
	std::vector<class Level*> scenes;
};

#endif
