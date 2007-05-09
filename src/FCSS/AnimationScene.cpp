#include "AnimationScene.h"
#include "Lightsprint/RRDebug.h"

/////////////////////////////////////////////////////////////////////////////
//
// LevelSetup, all data from .ani file in editable form

LevelSetup::LevelSetup(const char* afilename)
{
	filename = NULL;
	scale = 1;
	renderWater = 0;
	waterLevel = 0;
	overlayFilename[0] = 0;
	overlayMap = NULL;
	load(afilename);
};

LevelSetup::~LevelSetup()
{
	for(Frames::iterator i=frames.begin();i!=frames.end();i++)
		delete *i;
	free((void*)filename);
	delete overlayMap;
}

// load all from .ani
// afilename is name of scene, e.g. path/koupelna4.3ds
bool LevelSetup::load(const char* afilename)
{
	if(!afilename)
		return false;
	free((void*)filename);
	filename = _strdup(afilename);
	char* aniname = _strdup(filename);
	strcpy(aniname+strlen(aniname)-3,"ani");
	rr::RRReporter::report(rr::RRReporter::INFO,"Loading %s...\n",aniname);
	FILE* f = fopen(aniname,"rt");
	free(aniname);
	if(!f)
		return false;
	// load overlay
	overlayFilename[0] = 0;
	fscanf(f,"overlay = %s\n",overlayFilename);
	overlayMap = overlayFilename[0] ? de::Texture::load(overlayFilename, NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP) : NULL;
	// load scale
	scale = 1;
	fscanf(f,"scale = %f\n",&scale);
	// load water
	renderWater = 0;
	waterLevel = 0.1f;
	renderWater = fscanf(f,"water = %f\n",&waterLevel)==1;
	// load objects
	objects.clear();
	unsigned tmpobj;
	while(1==fscanf(f,"object = %d\n",&tmpobj))
	{
		objects.push_back(tmpobj-1);
	}
	// load frames
	frames.clear();
	AnimationFrame* tmp;
	while((tmp=AnimationFrame::load(f)))
	{
		tmp->validate(objects.size());
		if(!isOkForNewLayerNumber(tmp->layerNumber))
		{
			unsigned newNumber = newLayerNumber();
			rr::RRReporter::report(rr::RRReporter::INFO,"Changing layer number %d -> %d.\n",tmp->layerNumber,newNumber);
			tmp->layerNumber = newNumber;
		}
		frames.push_back(tmp);
	}
	fclose(f);
	return frames.size()>0;
}

// save all to .ani file
bool LevelSetup::save() const
{
	char* aniname = _strdup(filename);
	strcpy(aniname+strlen(aniname)-3,"ani");
	rr::RRReporter::report(rr::RRReporter::INFO,"Saving %s...\n",aniname);
	FILE* f = fopen(aniname,"wt");
	free(aniname);
	// save overlay
	if(overlayFilename[0])
		fprintf(f,"overlay = %s\n",overlayFilename);
	// save scale
	fprintf(f,"scale = %.5f\n",scale);
	// save water
	if(renderWater)
		fprintf(f,"water = %f\n",waterLevel);
	// save objects
	for(unsigned i=0;i<objects.size();i++)
	{
		fprintf(f,"object = %d\n",objects[i]+1);
	}
	fprintf(f,"\n");
	// save frames
	for(Frames::const_iterator i=frames.begin();i!=frames.end();i++)
	{
		if(!(*i)->save(f))
			return false;
	}
	fclose(f);
	return true;
}

bool LevelSetup::isOkForNewLayerNumber(unsigned number)
{
	if(number<2) return false;
	for(Frames::const_iterator i=frames.begin();i!=frames.end();i++)
	{
		if((*i)->layerNumber==number) return false;
	}
	return true;
}

unsigned LevelSetup::newLayerNumber()
{
	unsigned number = 0;
	while(!isOkForNewLayerNumber(number)) number++;
	return number;
}

LevelSetup::Frames::iterator LevelSetup::getFrameIterByIndex(unsigned index)
{
	//RR_ASSERT(index<frames.size());
	Frames::iterator i=frames.begin();
	for(unsigned j=0;j<index;j++) i++;
	return i;
}

AnimationFrame* LevelSetup::getFrameByIndex(unsigned index)
{
	if(index<frames.size())
		return *getFrameIterByIndex(index);
	else
		return NULL;
}

float LevelSetup::getFrameTime(unsigned index) const
{
	float seconds = 0;
	if(frames.size()>1)
	{
		Frames::const_iterator i=frames.begin();
		while(i!=frames.end() && index--)
		{
			seconds += (*i)->transitionToNextTime;
			i++;
		}
		// remove addition of last frame, animation stops at the beginning of last frame
		if(i==frames.end())
		{
			i--;
			seconds -= (*i)->transitionToNextTime;
		}
	}
	return seconds;
}

float LevelSetup::getTotalTime() const
{
	return getFrameTime(MAX(1,frames.size())-1);
}

const AnimationFrame* LevelSetup::getFrameByTime(float absSeconds)
{
	if(absSeconds<0)
		return NULL;
	Frames::const_iterator i=frames.begin();
	while(i!=frames.end() && (*i)->transitionToNextTime<absSeconds)
	{
		absSeconds -= (*i)->transitionToNextTime;
		i++;
	}
	if(i==frames.end())
		return NULL;
	Frames::const_iterator j = i; j++;
	if(j==frames.end())
		return NULL;
	return (*i)->blend(**j,absSeconds/(*i)->transitionToNextTime);
}

unsigned LevelSetup::getFrameIndexByTime(float absSeconds, float* transitionDone, float* transitionTotal)
{
	unsigned result = 0;
	Frames::const_iterator i=frames.begin();
	while(i!=frames.end() && (*i)->transitionToNextTime<=absSeconds)
	{
		absSeconds -= (*i)->transitionToNextTime;
		i++;
		result++;
	}
	if(transitionDone) *transitionDone = absSeconds;
	if(transitionTotal) *transitionTotal = (i==frames.end())? 0 : (*i)->transitionToNextTime;
	return result;
}

const de::Texture* LevelSetup::getOverlay()
{
	return overlayMap;
}
