#include "Autopilot.h"

/////////////////////////////////////////////////////////////////////////////
//
// Autopilot, navigates camera, light, characters according to LevelSetup

Autopilot::Autopilot(LevelSetup* levelSetup)
{
	memset(this,0,sizeof(*this));
	setup = levelSetup;
	enabled = true;
	secondsSinceLastInteraction = TIME_TO_AUTOPILOT;
	frameA = getNextFrame();
	frameB = getNextFrame();
}

void Autopilot::reportInteraction()
{
	secondsSinceLastInteraction = 0;
	enabled = false;
}

// generates new positions for eye, camera, characters
const AnimationFrame* Autopilot::autopilot(float seconds, bool* lightsChanged)
{
	if(setup->frames.size()==0)
		return NULL;

	pilotedFrames++;
	*lightsChanged = false;
	secondsSinceLastInteraction += seconds;
	if(!enabled 
		&& pilotedFrames>1) // must be enabled at least for one frame in each level
	{
		if(secondsSinceLastInteraction>TIME_TO_AUTOPILOT)
			enabled = true;
		else
			return NULL;
	}
	secondsSinceFrameA += seconds;
	float secondsOfTransition = secondsSinceFrameA - TIME_OF_STAY_STILL;
	float alpha = 0; // 0..1 where 0 is frameA, 1 is frameB
	if(secondsSinceFrameA!=seconds // uz nevim proc
		&& secondsOfTransition<0)
	{
		// part with no change
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
	return (alpha<=0)
		? *setup->getFrameByIndex(frameA)
		: (*setup->getFrameByIndex(frameA))->blend(**setup->getFrameByIndex(frameB),alpha);
}

bool Autopilot::isTimeToChangeLevel()
{
	return 0;//secondsSinceLastInteraction>TIME_TO_CHANGE_LEVEL+TIME_TO_AUTOPILOT;
}

// picks next frame that was less often selected
unsigned Autopilot::getNextFrame()
{
	if(!setup->frames.size())
	{
		return 0;
	}
	//static unsigned q=0;return q++%LevelSetup::MAX_FRAMES;//!!!
	unsigned best = 0;
	for(unsigned i=0;i<100;i++)
	{
		unsigned j = rand()%setup->frames.size();
		if(frameVisitedTimes[j]<frameVisitedTimes[best]) best = j;			
	}
	frameVisitedTimes[best]++;
	return best;
}
