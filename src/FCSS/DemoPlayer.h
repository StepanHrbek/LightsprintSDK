#ifndef DEMOPLAYER_H
#define DEMOPLAYER_H

#include <vector>

class DemoPlayer
{
public:
	DemoPlayer(const char* demoCfg);
	~DemoPlayer();
	void advance(float seconds);
	void setPaused(bool paused);
	bool getPaused();
	float getDemoPosition();
	float getPartPosition();
	void  setPartPosition(float seconds);
	class DynamicObjects* getDynamicObjects();
private:
	bool paused;
	class Music* music;
	float demoTime; // 0..demo duration in seconds
	float partStart; // 0..demo duration in seconds, time when current part started
	DynamicObjects* dynamicObjects;
	std::vector<class Level*> scenes;
};

#endif
