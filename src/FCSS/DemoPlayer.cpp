#include "DemoPlayer.h"
#include "Music.h"

DemoPlayer::DemoPlayer(const char* musicFilename)
{
	demoTime = 0;
	partStart = 0;
	music = new Music(musicFilename);
	paused = true;
	music->setPaused(paused);
}

DemoPlayer::~DemoPlayer()
{
	delete music;
}

void DemoPlayer::advance(float seconds)
{
	if(!paused)
	{
		demoTime += seconds;
	}
	music->poll();
}

void DemoPlayer::setPaused(bool apaused)
{
	paused = apaused;
	music->setPaused(paused);
}

bool DemoPlayer::getPaused()
{
	return paused;
}

float DemoPlayer::getDemoPosition()
{
	return demoTime;
}

float DemoPlayer::getPartPosition()
{
	return demoTime-partStart;
}

void DemoPlayer::setPartPosition(float seconds)
{
	demoTime = seconds+partStart;
	music->setPosition(demoTime);
}
