#ifndef AUTOPILOT_H
#define AUTOPILOT_H

/////////////////////////////////////////////////////////////////////////////
//
// autopilot

#include "DemoEngine/Camera.h"
#include "RRMath.h"

struct AutopilotFrame
{
	de::Camera eyeLight[2];
	float dynaPosRot[DYNAOBJECTS][4];
	float staySeconds;

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AutopilotFrame* blend(const AutopilotFrame* that, float alpha) const
	{
		assert(that);
		static AutopilotFrame blended;
		float* a = (float*)this;
		float* b = (float*)that;
		float* c = (float*)&blended;
		for(unsigned i=0;i<sizeof(blended)/sizeof(float);i++)
			c[i] = a[i]*(1-alpha) + b[i]*alpha;
		blended.eyeLight[0].update(0);
		blended.eyeLight[1].update(0.3f);
		return &blended;
	}
};

struct LevelSetup
{
	// constant setup
	const char* filename;
	float scale;
	enum {MAX_FRAMES=5};
	AutopilotFrame frames[MAX_FRAMES];

	// create
	static LevelSetup* create(const char* filename)
	{
		LevelSetup* setup = new LevelSetup;
		extern LevelSetup koupelna4;
		*setup = koupelna4;
		setup->filename = filename;
		return setup;
	};
};


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
	Autopilot(const LevelSetup* levelSetup)
	{
		memset(this,0,sizeof(*this));
		setup = levelSetup;
		enabled = true;
		secondsSinceLastInteraction = TIME_TO_AUTOPILOT;
		frameA = getNextFrame();
		frameB = getNextFrame();
	}
	void reportInteraction()
	{
		secondsSinceLastInteraction = 0;
		enabled = false;
	}

	// generates new positions for eye, camera, characters
	const AutopilotFrame* autopilot(float seconds, bool* lightsChanged)
	{
		*lightsChanged = false;
		secondsSinceLastInteraction += seconds;
		if(!enabled)
		{
			if(secondsSinceLastInteraction>TIME_TO_AUTOPILOT)
				enabled = true;
			else
				return NULL;
		}
		secondsSinceFrameA += seconds;
		float secondsOfTransition = secondsSinceFrameA - TIME_OF_STAY_STILL;
		float alpha = 0; // 0..1 where 0 is frameA, 1 is frameB
		if(secondsSinceFrameA!=seconds && secondsOfTransition<0)
		{
			// part with no change
			int i=1;
		}
		else
		{
			// transition from frameA to frameB
			alpha = secondsOfTransition/TIME_OF_FLIGHT;
			if(alpha>1)
			{
				frameA = frameB;
				frameB = getNextFrame();
				alpha = 0;
				secondsSinceFrameA = 0;
			}
			*lightsChanged = true;
		}
		return (alpha<=0) ? &setup->frames[frameA] : setup->frames[frameA].blend(&setup->frames[frameB],alpha);
	}

	bool isTimeToChangeLevel()
	{
		return secondsSinceLastInteraction>TIME_TO_CHANGE_LEVEL+TIME_TO_AUTOPILOT;
	}

	const LevelSetup* setup;
private:
	bool enabled;
	float secondsSinceLastInteraction;
	float secondsSinceFrameA;
	unsigned frameA; // 0..MAX_FRAMES-1
	unsigned frameB; // 0..MAX_FRAMES-1
	char frameVisitedTimes[LevelSetup::MAX_FRAMES]; // how many times was frame visited

	// picks next frame that was less often selected
	unsigned getNextFrame()
	{
//static unsigned q=0;return q++%LevelSetup::MAX_FRAMES;//!!!
		unsigned best = 0;
		for(unsigned i=0;i<100;i++)
		{
			unsigned j = rand()%LevelSetup::MAX_FRAMES;
			if(frameVisitedTimes[j]<frameVisitedTimes[best]) best = j;			
		}
		frameVisitedTimes[best]++;
		return best;
	}
};


#endif
