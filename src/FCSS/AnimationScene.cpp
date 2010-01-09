#include "GL/glew.h"
#include "AnimationScene.h"
#include "Lightsprint/RRDebug.h"

// frequency of light and object position/rotation changes is limited
// faster changes would not be visible
#define MAX_LIGHT_UPDATE_FREQUENCY 60

/////////////////////////////////////////////////////////////////////////////
//
// LevelSetup, all data from .ani file in editable form

LevelSetup::LevelSetup(const char* afilename)
{
	filename = NULL;
	scale = 1;
	renderWater = 0;
	waterLevel = 0;
	load(afilename);
};

LevelSetup::~LevelSetup()
{
	for (Frames::iterator i=frames.begin();i!=frames.end();i++)
		delete *i;
	free((void*)filename);
}

// load all from .ani
// afilename is name of scene, e.g. path/koupelna4.3ds
bool LevelSetup::load(const char* afilename)
{
	if (!afilename)
		return false;
	free((void*)filename);
	filename = _strdup(afilename);
	char* aniname = _strdup(filename);
	strcpy(aniname+strlen(aniname)-3,"ani");
	rr::RRReporter::report(rr::INF1,"Loading %s...\n",aniname);
	FILE* f = fopen(aniname,"rt");
	free(aniname);
	if (!f)
		return false;
	// load scale
	scale = 1;
	fscanf(f,"scale = %f\n",&scale);
	// load water
	renderWater = 0;
	waterLevel = 0.1f;
	renderWater = fscanf(f,"water = %f\n",&waterLevel)==1;
	// skip min feature size in old files
	float minFeatureSize;
	fscanf(f,"min_feature = %f\n",&minFeatureSize);
	// load quality
	calculateParams = rr::RRDynamicSolver::CalculateParameters();
	fscanf(f,"indirect_quality = %d,%d\n",&calculateParams.qualityIndirectDynamic,&calculateParams.qualityIndirectStatic);
	// load objects
	objects.clear();
	unsigned tmpobj;
	while (1==fscanf(f,"object = %d\n",&tmpobj))
	{
		objects.push_back(tmpobj-1);
	}
	// load frames
	frames.clear();
	AnimationFrame frame(0); // working frame, data are loded over and over into tmp, so inheritance works
	while (frame.loadOver(f))
	{
		frame.validate((unsigned)objects.size());
		if (!isOkForNewLayerNumber(frame.layerNumber))
		{
			unsigned newNumber = newLayerNumber();
			rr::RRReporter::report(rr::INF1,"Changing layer number %d -> %d.\n",frame.layerNumber,newNumber);
			frame.layerNumber = newNumber;
		}
		frames.push_back(new AnimationFrame(frame));
	}
	fclose(f);
	return frames.size()>0;
}

// save all to .ani file
bool LevelSetup::save() const
{
	char* aniname = _strdup(filename);
	strcpy(aniname+strlen(aniname)-3,"ani");
	rr::RRReporter::report(rr::INF1,"Saving %s...\n",aniname);
	FILE* f = fopen(aniname,"wt");
	free(aniname);
	// save scale
	fprintf(f,"scale = %f\n",scale);
	// save water
	if (renderWater)
		fprintf(f,"water = %f\n",waterLevel);
	// save quality
	if ((calculateParams.qualityIndirectDynamic!=rr::RRDynamicSolver::CalculateParameters().qualityIndirectDynamic) ||
		(calculateParams.qualityIndirectStatic!=rr::RRDynamicSolver::CalculateParameters().qualityIndirectStatic))
	fprintf(f,"indirect_quality = %d,%d\n",calculateParams.qualityIndirectDynamic,calculateParams.qualityIndirectStatic);
	// save objects
	for (unsigned i=0;i<objects.size();i++)
	{
		fprintf(f,"object = %d\n",objects[i]+1);
	}
	fprintf(f,"\n");
	// save frames
	AnimationFrame empty(0);
	AnimationFrame* prev = &empty; // previous frame
	for (Frames::const_iterator i=frames.begin();i!=frames.end();i++)
	{
		if (!(*i)->save(f,*prev))
			return false;
		prev = *i;
	}
	fclose(f);
	return true;
}

bool LevelSetup::isOkForNewLayerNumber(unsigned number)
{
	if (number<2) return false;
	for (Frames::const_iterator i=frames.begin();i!=frames.end();i++)
	{
		if ((*i)->layerNumber==number) return false;
	}
	return true;
}

unsigned LevelSetup::newLayerNumber()
{
	unsigned number = 0;
	while (!isOkForNewLayerNumber(number)) number++;
	return number;
}

LevelSetup::Frames::iterator LevelSetup::getFrameIterByIndex(unsigned index)
{
	//RR_ASSERT(index<frames.size());
	Frames::iterator i=frames.begin();
	for (unsigned j=0;j<index;j++) i++;
	return i;
}

AnimationFrame* LevelSetup::getFrameByIndex(unsigned index)
{
	if (index<frames.size())
		return *getFrameIterByIndex(index);
	else
		return NULL;
}

float LevelSetup::getFrameTime(unsigned index) const
{
	float seconds = 0;
	if (frames.size()>1)
	{
		Frames::const_iterator i=frames.begin();
		while (i!=frames.end() && index--)
		{
			seconds += (*i)->transitionToNextTime;
			i++;
		}
		// remove addition of last frame, animation stops at the beginning of last frame
		if (i==frames.end())
		{
			i--;
			seconds -= (*i)->transitionToNextTime;
		}
	}
	return seconds;
}

float LevelSetup::getTotalTime() const
{
	return getFrameTime(RR_MAX(1,(unsigned)frames.size())-1);
}

const AnimationFrame* LevelSetup::getFrameByTime(float absSeconds)
{
	if (absSeconds<0)
		return NULL;
	Frames::const_iterator i=frames.begin();
	while (i!=frames.end() && (*i)->transitionToNextTime<absSeconds)
	{
		absSeconds -= (*i)->transitionToNextTime;
		i++;
	}
	if (i==frames.end())
		return NULL;
	Frames::const_iterator j = i; j++;
	if (j==frames.end())
		return NULL;

	// round absSeconds to nearest lower multiply of 1/60s to make light move in small steps, reduce number of shadowmap updates
	float absSecondsRounded = unsigned(absSeconds*MAX_LIGHT_UPDATE_FREQUENCY)/float(MAX_LIGHT_UPDATE_FREQUENCY);

	return (*i)->blend(**j,absSeconds/(*i)->transitionToNextTime,absSecondsRounded/(*i)->transitionToNextTime);
}

unsigned LevelSetup::getFrameIndexByTime(float absSeconds, float* transitionDone, float* transitionTotal)
{
	unsigned result = 0;
	Frames::const_iterator i=frames.begin();
	while (i!=frames.end() && (*i)->transitionToNextTime<=absSeconds)
	{
		absSeconds -= (*i)->transitionToNextTime;
		i++;
		result++;
	}
	if (transitionDone) *transitionDone = absSeconds;
	if (transitionTotal) *transitionTotal = (i==frames.end())? 0 : (*i)->transitionToNextTime;
	return result;
}
