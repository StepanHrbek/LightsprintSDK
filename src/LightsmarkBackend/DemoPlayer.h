#ifndef DEMOPLAYER_H
#define DEMOPLAYER_H

#include <vector>
#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"

class DemoPlayer
{
public:
	DemoPlayer(const char* demoCfg, bool supportEditor, bool supportMusic, bool pauseMusic);
	~DemoPlayer();
	void advance(); // advances according to real time
	void advanceBy(float seconds); // advances by given time (e.g. 1/30. for video capture)

	void setPaused(bool paused);
	bool getPaused() const;

	float getDemoLength() const;
	float getDemoPosition() const;
	void  setDemoPosition(float seconds);

	unsigned getPartIndex() const;
	unsigned getNumParts() const;
	float getPartLength(unsigned part = 0xffff) const; // default = current part, otherwise index of part, 0..3
	float getPartPosition() const;
	void  setPartPosition(float seconds);

	float getMusicLength() const;
	float getMusicPosition() const;
	void  setVolume(float volume);

	class Level* getNextPart(bool seekInMusic, bool loop); // adjusts timers, next part is started
	class Level* getPart(unsigned index); // no timer adjustments made

	class DynamicObjects* getDynamicObjects();

	unsigned getNumProjectors();
	rr::RRBuffer* getProjector(unsigned projectorIndex);

	void setBigscreen(bool big);
	void getBoost(rr::RRVec4& frameBrightness,rr::RRReal& frameGamma) const;

private:
	// timing
	bool paused;
	bool pauseMusic; // true: music is synchronized/paused with demo, false: music plays all time
	float partStart; // 0..demo duration in seconds, time when current part started
	float demoPosition; // 0..demo duration in seconds.
	rr::RRTime referenceTime;
	float referencePosition;

	// loading_screen
	rr::RRBuffer* loadingMap;

	// music
	rr::RRBuffer* music;

	// sky
	rr::RRBuffer* skyMap;

	// objects
	DynamicObjects* dynamicObjects;

	// scenes
	unsigned nextSceneIndex;
	std::vector<class Level*> scenes;

	// projectors
	std::vector<rr::RRBuffer*> projectors;

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
