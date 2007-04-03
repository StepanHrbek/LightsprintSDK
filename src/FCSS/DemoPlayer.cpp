#include "DemoPlayer.h"
#include "Music.h"
#include "DynamicObject.h"
#include "DynamicObjects.h"
//#include "LevelSequence.h"
#include "Level.h"
#include "Lightsprint/RRGPUOpenGL.h"

DemoPlayer::DemoPlayer(const char* demoCfg, bool supportEditor)
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

	// load sky
	buf[0] = 0;
	fscanf(f,"sky = %s\n",buf);
	const char* cubeSideNames[6] = {"ft","bk","dn","up","rt","lf"};
	if(buf[0])
	{
		skyMap = rr_gl::RRRealtimeRadiosityGL::loadIlluminationEnvironmentMap(buf,cubeSideNames);
		if(!skyMap)
			rr::RRReporter::report(rr::RRReporter::WARN,"Failed to load skybox %s.\n",buf);
	}

	// load objects
	dynamicObjects = new DynamicObjects();
	float diffuse,specular;
	unsigned specularMap,normalMap,emissiveMap;
	unsigned specularCubeSize;
	float scale;
	while(8==fscanf(f,"object = %f,%f,%d,%d,%d,%d,%f,%s\n",
		&diffuse,&specular,&specularMap,&normalMap,&emissiveMap,&specularCubeSize,&scale,buf))
	{
		de::UberProgramSetup material;
		material.MATERIAL_DIFFUSE = diffuse?1:0;
		material.MATERIAL_DIFFUSE_CONST = 0;
		material.MATERIAL_DIFFUSE_VCOLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = diffuse?1:0;
		material.MATERIAL_SPECULAR = specular?1:0;
		material.MATERIAL_SPECULAR_MAP = specularMap?1:0;
		material.MATERIAL_NORMAL_MAP = normalMap?1:0;
		material.MATERIAL_EMISSIVE_MAP = emissiveMap?1:0;
		rr::RRReporter::report(rr::RRReporter::INFO,"Loading %s...\n",buf);
		DynamicObject* object = DynamicObject::create(buf,scale,material,specularCubeSize);
		dynamicObjects->addObject(object);
	}

	// load scenes
	while(1==fscanf(f,"scene = %s\n",buf))
	{
		//extern LevelSequence levelSequence;
		//levelSequence.insertLevelBack(buf);
		Level* level = new Level(new LevelSetup(buf),skyMap,supportEditor);
		scenes.push_back(level);
	}
	nextSceneIndex = 0;

	fclose(f);
}

DemoPlayer::~DemoPlayer()
{
	for(unsigned i=0;i<scenes.size();i++)
		delete scenes[i];
	delete music;
	delete dynamicObjects;
}

DynamicObjects* DemoPlayer::getDynamicObjects()
{
	return dynamicObjects;
}

Level* DemoPlayer::getPart(unsigned index)
{
	if(index<scenes.size())
		return scenes[index];
	else
		return NULL;
}

Level* DemoPlayer::getNextPart()
{
	if(nextSceneIndex<scenes.size())
	{
		partStart += getPartLength();
		nextSceneIndex++;
		setPartPosition(0);
		return scenes[nextSceneIndex-1];
	}
	else
		return NULL;
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

float DemoPlayer::getDemoLength()
{
	float total = 0;
	for(unsigned i=0;i<scenes.size();i++)
		total += getPartLength(i);
	return total;
}

unsigned DemoPlayer::getPartIndex()
{
	return nextSceneIndex-1;
}

unsigned DemoPlayer::getNumParts()
{
	return scenes.size();
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

float DemoPlayer::getPartLength(unsigned part)
{
	if(part==0xffff)
		part = nextSceneIndex-1;
	if(part>=scenes.size())
		return 0;
	return scenes[part]->pilot.setup->getTotalTime();
}

float DemoPlayer::getMusicPosition()
{
	return music->getPosition();
}

float DemoPlayer::getMusicLength()
{
	return music->getLength();
}
