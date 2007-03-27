#include "DemoPlayer.h"
#include "Music.h"
#include "DynamicObject.h"
#include "DynamicObjects.h"
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
	dynamicObjects = new DynamicObjects();
	float diffuse,specular;
	unsigned specularMap,normalMap;
	unsigned specularCubeSize;
	float scale;
	while(7==fscanf(f,"object = %f,%f,%d,%d,%d,%f,%s\n",
		&diffuse,&specular,&specularMap,&normalMap,&specularCubeSize,&scale,buf))
	{
		de::UberProgramSetup material;
		material.MATERIAL_DIFFUSE = diffuse?1:0;
		material.MATERIAL_DIFFUSE_CONST = 0;
		material.MATERIAL_DIFFUSE_VCOLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = diffuse?1:0;
		material.MATERIAL_SPECULAR = specular?1:0;
		material.MATERIAL_SPECULAR_MAP = specularMap?1:0;
		material.MATERIAL_NORMAL_MAP = normalMap?1:0;
		rr::RRReporter::report(rr::RRReporter::INFO,"Loading %s...\n",buf);
		DynamicObject* object = DynamicObject::create(buf,scale,material,specularCubeSize);
		dynamicObjects->addObject(object);
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
	delete dynamicObjects;
}

DynamicObjects* DemoPlayer::getDynamicObjects()
{
	return dynamicObjects;
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

float DemoPlayer::getPartLength()
{
	return 0;
}

float DemoPlayer::getDemoLength()
{
	return 0;
}

float DemoPlayer::getMusicLength()
{
	return music->getLength();
}
