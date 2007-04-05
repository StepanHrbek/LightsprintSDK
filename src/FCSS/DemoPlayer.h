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
	bool getPaused() const;

	float getDemoLength() const;
	float getDemoPosition() const;

	unsigned getPartIndex() const;
	unsigned getNumParts() const;
	float getPartLength(unsigned part = 0xffff) const; // default = current part, otherwise index of part, 0..3
	float getPartPosition() const;
	void  setPartPosition(float seconds);

	float getMusicLength() const;
	float getMusicPosition() const;

	class DynamicObjects* getDynamicObjects();

	class Level* getNextPart(bool seekInMusic); // adjusts timers, next part is started
	class Level* getPart(unsigned index); // no timer adjustments made

	void setBigscreen(bool big);
	void getBoost(rr::RRVec4& frameBrightness,rr::RRReal& frameGamma) const;

private:
	bool paused;
	float demoTimeWhenPaused; // 0..demo duration in seconds. matters only when paused, because fmod is inaccurate when paused
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

	// brightness/gamma
	bool bigscreen; // enabled by cmdline param 'bigscreen'
	float bigscreenBrightness; // loaded from .cfg
	float bigscreenGamma; // loaded from .cfg
	struct Flash
	{
		float start; // seconds of demo time
		float duration; // seconds
		float brightness;
		float gamma;
	};
	std::vector<Flash> flashes;
};

#endif
