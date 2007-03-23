#ifndef AUTOPILOT_H
#define AUTOPILOT_H

#include "AnimationScene.h"

/////////////////////////////////////////////////////////////////////////////
//
// Autopilot, navigates camera, light, characters according to LevelSetup

class Autopilot
{
public:
	enum 
	{
		TIME_TO_AUTOPILOT    = 15, // seconds, time of inactivity before autopilot starts
		TIME_OF_FLIGHT       = 5,  // seconds, length of flight between two frames
		TIME_OF_STAY_STILL   = 4,  // seconds, length of flight between two frames
		TIME_TO_CHANGE_LEVEL = 60,
	};
	Autopilot(LevelSetup* levelSetup);

	void reportInteraction();

	// generates new positions for eye, camera, characters
	const AnimationFrame* autopilot(float seconds, bool* lightsChanged);

	bool isTimeToChangeLevel();

	LevelSetup* setup;
private:
	bool enabled;
	unsigned pilotedFrames; // number of frames already played
	float secondsSinceLastInteraction;
	float secondsSinceFrameA;
	unsigned frameA; // 0..MAX_FRAMES-1
	unsigned frameB; // 0..MAX_FRAMES-1
	char frameVisitedTimes[1000]; 
	
	// picks next frame that was less often selected
	unsigned getNextFrame();
};

#endif
