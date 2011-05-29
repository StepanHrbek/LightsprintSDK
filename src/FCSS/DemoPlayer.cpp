#include "GL/glew.h"
#include "Level.h"
#include "DemoPlayer.h"
#include "Music.h"
#include "DynamicObjects.h"
//#include "LevelSequence.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/RRDebug.h"

extern void showImage(const rr_gl::Texture* tex);

DemoPlayer::DemoPlayer(const char* demoCfg, bool supportEditor, bool supportMusic, bool _pauseMusic)
{
	rr::RRReportInterval report(rr::INF1,"Loading %s...\n",demoCfg);

	// clear all
	paused = false;
	pauseMusic = false;
	partStart = 0;
	demoPosition = 0;
	referencePosition = 0;
	loadingMap = NULL;
	music = NULL;
	skyMap = NULL;
	dynamicObjects = NULL;
	nextSceneIndex = 0;
	// scenes have constructor
	// projectors have constructor
	bigscreen = false;
	bigscreenBrightness = 1;
	bigscreenGamma = 1;
	// flashes have constructor

	FILE* f = fopen(demoCfg,"rt");
	if (!f)
	{
		rr::RRReporter::report(rr::ERRO,"Wrong path/filename, doesn't exist.\n");
		return;
	}

	// load loading_screen
	char buf[1000];
	buf[0] = 0;
	fscanf(f,"loading_screen = %s\n",buf);
	if (buf[0])
	{
		loadingMap = rr::RRBuffer::load(buf);
		showImage(rr_gl::getTexture(loadingMap,true,false));
	}

	// load music - step1
	char bufmusic[1000];
	bufmusic[0] = 0;
	if (1!=fscanf(f,"music = %s\n",bufmusic)) bufmusic[0] = 0;

	// load sky
	buf[0] = 0;
	fscanf(f,"sky = %s\n",buf);
	if (buf[0])
	{
		skyMap = rr::RRBuffer::loadCube(buf);
		if (!skyMap)
			rr::RRReporter::report(rr::WARN,"Failed to load skybox %s.\n",buf);
	}

	// load objects
	dynamicObjects = new DynamicObjects();
	float diffuse,specular;
	unsigned specularMap,normalMap,emissiveMap;
	unsigned gatherDiffuseCubeSize;
	unsigned specularCubeSize;
	float scale;
	while (9==fscanf(f,"object = %f,%f,%d,%d,%d,%d,%d,%f,%s\n",
		&diffuse,&specular,&specularMap,&normalMap,&emissiveMap,&gatherDiffuseCubeSize,&specularCubeSize,&scale,buf))
	{
		/*
		rr_gl::UberProgramSetup material;
		material.MATERIAL_DIFFUSE = diffuse?1:0;
		material.MATERIAL_DIFFUSE_X2 = 0;
		material.MATERIAL_DIFFUSE_CONST = (diffuse && diffuse!=1)?1:0;
		material.MATERIAL_DIFFUSE_MAP = diffuse?1:0;
		material.MATERIAL_SPECULAR = specular?1:0;
		material.MATERIAL_SPECULAR_CONST = (specular && specular!=1)?1:0;
		material.MATERIAL_SPECULAR_MAP = specularMap?1:0;
		material.MATERIAL_NORMAL_MAP = normalMap?1:0;
		material.MATERIAL_EMISSIVE_MAP = emissiveMap?1:0;
		*/
		rr::RRReporter::report(rr::INF1,"Loading %s...\n",buf);
		if (dynamicObjects->addObject(buf,scale))
		{
			// adjust material
			rr::RRObject* object = (*dynamicObjects)[dynamicObjects->size()-1];
			if (object->faceGroups.size()==1)
			{
				rr::RRMaterial* material = object->faceGroups[0].material;
				material->diffuseReflectance.color = rr::RRVec3(diffuse);
				material->specularReflectance.color = rr::RRVec3(specular);
				material->updateSideBitsFromColors();
			}
			// alloc cubes
			object->illumination.gatherEnvMapSize = gatherDiffuseCubeSize;
			if (diffuse && gatherDiffuseCubeSize)
				object->illumination.diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,gatherDiffuseCubeSize,gatherDiffuseCubeSize,6,rr::BF_RGBA,true,NULL);
			if (specular && specularCubeSize)
				object->illumination.specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,specularCubeSize,specularCubeSize,6,rr::BF_RGBA,true,NULL);
		}
		else
		{
			rr::RRReporter::report(rr::WARN,"Object indices temporarily broken due to missing object.\n");
		}
	}

	// load scenes
	while (1==fscanf(f,"scene = %s\n",buf))
	{
		Level* level = new Level(new LevelSetup(buf),skyMap,supportEditor);
		level->solver->setDynamicObjects(*dynamicObjects);
		scenes.push_back(level);
	}
	nextSceneIndex = 0;

	// load projectors
	while (1==fscanf(f,"projector = %s\n",buf))
	{
		projectors.push_back(rr::RRBuffer::load(buf));
	}
	nextSceneIndex = 0;

	// load boost
	fscanf(f,"bigscreen_brightness = %f\n",&bigscreenBrightness);
	fscanf(f,"bigscreen_gamma = %f\n",&bigscreenGamma);
	Flash flash;
	while (4==fscanf(f,"flash = %f,%f,%f,%f\n",&flash.start,&flash.duration,&flash.brightness,&flash.gamma))
	{
		flashes.push_back(flash);
	}

	fclose(f);

	// load music - step2
	music = supportMusic ? Music::load(bufmusic) : NULL;
	pauseMusic = _pauseMusic;
	//if (music) music->setPaused(pauseMusic);
	paused = true;
}

DemoPlayer::~DemoPlayer()
{
	delete loadingMap;
	delete skyMap;
	for (unsigned i=0;i<scenes.size();i++)
	{
		LevelSetup* setup = scenes[i]->setup;
		delete scenes[i];
		delete setup;
	}
	for (unsigned i=0;i<projectors.size();i++)
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
	if (index<scenes.size())
		return scenes[index];
	else
		return NULL;
}

Level* DemoPlayer::getNextPart(bool seekInMusic, bool loop)
{
	if (loop && scenes.size())
		nextSceneIndex %= scenes.size();
	if (nextSceneIndex<scenes.size())
	{
		partStart += getPartLength();
		nextSceneIndex++;
		if (seekInMusic)
			setPartPosition(0);
		return scenes[nextSceneIndex-1];
	}
	else
		return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// timing

// cas je zde kompletni informace o pozici v demu
// editor (kod jinde) ale musi navic vedet cislo snimku. snimek muze mit delku 0, pak nejde cislo snimku dohledat z casu

void DemoPlayer::advance()
{
	if (!paused)
	{
		demoPosition = referenceTime.secondsPassed()+referencePosition;
	}
	if (music)
	{
		music->poll();
	}
}

void DemoPlayer::advanceBy(float seconds)
{
	demoPosition += seconds;
	referenceTime.setNow();
	referencePosition = demoPosition;
	if (music) music->poll();
}

void DemoPlayer::setPaused(bool _paused)
{
	if (!paused && _paused) demoPosition = getDemoPosition();
	if (paused && !_paused) setDemoPosition(demoPosition);
	paused = _paused;
	//if (music && pauseMusic)
	if (music && (pauseMusic || !paused)) // even if we don't pause music, it's paused from constructor, so proceed with unpausing
	{
		music->setPaused(paused);
	}
}

bool DemoPlayer::getPaused() const
{
	return paused;
}

void DemoPlayer::setDemoPosition(float seconds)
{
	if (music && pauseMusic) music->setPosition(seconds);
	demoPosition = seconds;
	referenceTime.setNow();
	referencePosition = demoPosition;
}

float DemoPlayer::getDemoPosition() const
{
	return demoPosition;
}

float DemoPlayer::getDemoLength() const
{
	float total = 0;
	for (unsigned i=0;i<scenes.size();i++)
		total += getPartLength(i);
	return total;
}

unsigned DemoPlayer::getPartIndex() const
{
	return nextSceneIndex-1;
}

unsigned DemoPlayer::getNumParts() const
{
	return (unsigned)scenes.size();
}

float DemoPlayer::getPartPosition() const
{
	float partPosition = getDemoPosition()-partStart;
	if (partPosition<0)
	{
		// rounding error could make it slightly subzero when new part starts
		// returning subzero in such situation would stop player (playing out of range)
		return 0;
	}
	return partPosition;
}

void DemoPlayer::setPartPosition(float seconds)
{
	setDemoPosition(partStart+seconds);
}

float DemoPlayer::getPartLength(unsigned part) const
{
	if (part==0xffff)
		part = nextSceneIndex-1;
	if (part>=scenes.size())
		return 0;
	return scenes[part]->setup->getTotalTime();
}

float DemoPlayer::getMusicPosition() const
{
	return music ? music->getPosition() : 0;
}

float DemoPlayer::getMusicLength() const
{
	return music ? music->getLength() : 0;
}

void DemoPlayer::setVolume(float volume)
{
	if (music)
		music->setVolume(volume);
}

/////////////////////////////////////////////////////////////////////////////
//
// projectors

unsigned DemoPlayer::getNumProjectors()
{
	return (unsigned)projectors.size();
}

rr::RRBuffer* DemoPlayer::getProjector(unsigned projectorIndex)
{
	if (projectorIndex<projectors.size())
	{
		return projectors[projectorIndex];
	}
	else
	{
		RR_LIMITED_TIMES(3,rr::RRReporter::report(rr::WARN,"\"projector = %d\" used in .ani, but only %d projectors defined in .cfg.\n",projectorIndex,projectors.size()));
		RR_ASSERT(0);
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
	if (bigscreen) boostBrightness *= bigscreenBrightness;
	if (bigscreen) boostGamma *= bigscreenGamma;
	float now = getDemoPosition();
	for (unsigned i=0;i<flashes.size();i++)
	{
		if (flashes[i].start<now && now<flashes[i].start+flashes[i].duration)
		{
			float fade = 1-(now-flashes[i].start)/flashes[i].duration; // goes through interval 1..0
			boostBrightness *= (flashes[i].brightness-1)*fade+1; // goes through interval brightness..1
			boostGamma *= (flashes[i].gamma-1)*fade+1; // goes through interval gamma..1
		}
	}
	frameBrightness *= boostBrightness;
	frameGamma *= boostGamma;
}
