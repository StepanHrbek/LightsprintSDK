#include "DemoPlayer.h"
#include "Music.h"
#include "DynamicObject.h"
#include "LevelSequence.h"

DemoPlayer::DemoPlayer(const char* demoCfg)
{
	memset(this,0,sizeof(*this));
	FILE* f = fopen(demoCfg,"rt");
	if(!f)
		return;
	// load music
	char buf[1000];
	if(1!=fscanf(f,"music = %s\n",buf))
		return;
	music = new Music(buf);
	paused = true;
	music->setPaused(paused);
	// load objects
	float scale;
	de::UberProgramSetup material;
	unsigned materialIdx;
	unsigned specularCubeSize;
	while(4==fscanf(f,"object = %d,%d,%f,%s\n",&materialIdx,&specularCubeSize,&scale,buf))
	{
		DynamicObject* object = DynamicObject::create(buf,scale,material,specularCubeSize);
		objects.push_back(object);
	}
	// load scenes
	while(1==fscanf(f,"scene = %s\n",buf))
	{
		extern LevelSequence levelSequence;
		levelSequence.insertLevelBack(buf);
		//Level* level = new Level(buf);
		//scenes.push_back(level);
	}
	fclose(f);
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
