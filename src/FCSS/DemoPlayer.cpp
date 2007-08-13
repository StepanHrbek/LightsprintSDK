#include "GL/glew.h"
#include "Level.h" // must be first, so collada is included before demoengine (#define SAFE_DELETE collides)
#include "DemoPlayer.h"
#include "Music.h"
#include "DynamicObject.h"
#include "DynamicObjects.h"
//#include "LevelSequence.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

extern void showImage(const rr_gl::Texture* tex);

DemoPlayer::DemoPlayer(const char* demoCfg, bool supportEditor)
{
	memset(this,0,sizeof(*this));
	bigscreenBrightness = 1;
	bigscreenGamma = 1;
	naturalTime = 1;

	FILE* f = fopen(demoCfg,"rt");
	if(!f)
		return;

	// load loading_screen
	char buf[1000];
	buf[0] = 0;
	fscanf(f,"loading_screen = %s\n",buf);
	if(buf[0])
	{
		loadingMap = rr_gl::Texture::load(buf, NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		showImage(loadingMap);
	}

	// load music
	if(1!=fscanf(f,"music = %s\n",buf))
		return;
	music = new Music(buf);
	paused = true;
	music->setPaused(paused);

	// load sky
	buf[0] = 0;
	fscanf(f,"sky = %s\n",buf);
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	if(buf[0])
	{
		skyMap = rr_gl::RRDynamicSolverGL::loadIlluminationEnvironmentMap(buf,cubeSideNames,true,true);
		if(!skyMap)
			rr::RRReporter::report(rr::WARN,"Failed to load skybox %s.\n",buf);
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
		rr_gl::UberProgramSetup material;
		material.MATERIAL_DIFFUSE = diffuse?1:0;
		material.MATERIAL_DIFFUSE_CONST = (diffuse && diffuse!=1)?1:0;
		material.MATERIAL_DIFFUSE_VCOLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = diffuse?1:0;
		material.MATERIAL_SPECULAR = specular?1:0;
		material.MATERIAL_SPECULAR_CONST = (specular && specular!=1)?1:0;
		material.MATERIAL_SPECULAR_MAP = specularMap?1:0;
		material.MATERIAL_NORMAL_MAP = normalMap?1:0;
		material.MATERIAL_EMISSIVE_MAP = emissiveMap?1:0;
		rr::RRReporter::report(rr::INF1,"Loading %s...\n",buf);
		DynamicObject* object = DynamicObject::create(buf,scale,material,specularCubeSize);
		dynamicObjects->addObject(object);
	}

	// load scenes
	while(1==fscanf(f,"scene = %s\n",buf))
	{
		Level* level = new Level(new LevelSetup(buf),skyMap,supportEditor);
		scenes.push_back(level);
	}
	nextSceneIndex = 0;

	// load projectors
	while(1==fscanf(f,"projector = %s\n",buf))
	{
		rr_gl::Texture* projector = rr_gl::Texture::load(buf, NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		projectors.push_back(projector);
	}
	nextSceneIndex = 0;

	// load boost
	fscanf(f,"bigscreen_brightness = %f\n",&bigscreenBrightness);
	fscanf(f,"bigscreen_gamma = %f\n",&bigscreenGamma);
	Flash flash;
	while(4==fscanf(f,"flash = %f,%f,%f,%f\n",&flash.start,&flash.duration,&flash.brightness,&flash.gamma))
	{
		flashes.push_back(flash);
	}

	fclose(f);
}

DemoPlayer::~DemoPlayer()
{
	delete loadingMap;
	delete skyMap;
	for(unsigned i=0;i<scenes.size();i++)
	{
		LevelSetup* setup = scenes[i]->pilot.setup;
		delete scenes[i];
		delete setup;
	}
	for(unsigned i=0;i<projectors.size();i++)
		delete projectors[i];
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

Level* DemoPlayer::getNextPart(bool seekInMusic, bool loop)
{
	if(loop && scenes.size())
		nextSceneIndex %= scenes.size();
	if(nextSceneIndex<scenes.size())
	{
		partStart += getPartLength();
		nextSceneIndex++;
		if(seekInMusic)
			setPartPosition(0);
		return scenes[nextSceneIndex-1];
	}
	else
		return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// timing

void DemoPlayer::advance(float seconds)
{
	naturalTime = seconds<-1e9;
	if(!paused && !naturalTime)
	{
		demoTimeWhenPaused += seconds;
	}
	if(music)
	{
		music->poll();
	}
}

void DemoPlayer::setPaused(bool apaused)
{
	if(music)
	{
		if(!paused && apaused && naturalTime) demoTimeWhenPaused = music->getPosition();
	}
//	if(paused && !apaused) demoTimeWhenPaused = 0;
	paused = apaused;
	if(music)
	{
		music->setPaused(paused);
	}
}

bool DemoPlayer::getPaused() const
{
	return paused;
}

float DemoPlayer::getDemoPosition() const
{
	if(paused || !naturalTime)
		return demoTimeWhenPaused;
	return getMusicPosition();
}

float DemoPlayer::getDemoLength() const
{
	float total = 0;
	for(unsigned i=0;i<scenes.size();i++)
		total += getPartLength(i);
	return total;
}

unsigned DemoPlayer::getPartIndex() const
{
	return nextSceneIndex-1;
}

unsigned DemoPlayer::getNumParts() const
{
	return scenes.size();
}

float DemoPlayer::getPartPosition() const
{
	return getDemoPosition()-partStart;
}

void DemoPlayer::setPartPosition(float seconds)
{
	if(music)
	{
		music->setPosition(seconds+partStart);
	}
	if(paused || !naturalTime)
		demoTimeWhenPaused = seconds+partStart;
}

float DemoPlayer::getPartLength(unsigned part) const
{
	if(part==0xffff)
		part = nextSceneIndex-1;
	if(part>=scenes.size())
		return 0;
	return scenes[part]->pilot.setup->getTotalTime();
}

float DemoPlayer::getMusicPosition() const
{
	return music->getPosition();
}

float DemoPlayer::getMusicLength() const
{
	return music->getLength();
}

/////////////////////////////////////////////////////////////////////////////
//
// projectors

unsigned DemoPlayer::getNumProjectors()
{
	return projectors.size();
}

const rr_gl::Texture* DemoPlayer::getProjector(unsigned projectorIndex)
{
	if(projectorIndex<projectors.size())
	{
		return projectors[projectorIndex];
	}
	else
	{
		LIMITED_TIMES(3,rr::RRReporter::report(rr::WARN,"\"projector = %d\" used in .ani, but only %d projectors defined in .cfg.\n",projectorIndex,projectors.size()));
		assert(0);
		return NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// brightness/gamma boost

void DemoPlayer::setBigscreen(bool aboost)
{
	bigscreen = aboost;
}

void DemoPlayer::getBoost(rr::RRVec4& frameBrightness,rr::RRReal& frameGamma) const
{
	float boostBrightness = 1;
	float boostGamma = 1;
	if(bigscreen) boostBrightness *= bigscreenBrightness;
	if(bigscreen) boostGamma *= bigscreenGamma;
	float now = getDemoPosition();
	for(unsigned i=0;i<flashes.size();i++)
	{
		if(flashes[i].start<now && now<flashes[i].start+flashes[i].duration)
		{
			float fade = 1-(now-flashes[i].start)/flashes[i].duration; // goes through interval 1..0
			boostBrightness *= (flashes[i].brightness-1)*fade+1; // goes through interval brightness..1
			boostGamma *= (flashes[i].gamma-1)*fade+1; // goes through interval gamma..1
		}
	}
	frameBrightness *= boostBrightness;
	frameGamma *= boostGamma;
}
