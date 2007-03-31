#include "AnimationScene.h"
#include "Lightsprint/RRDebug.h"

/////////////////////////////////////////////////////////////////////////////
//
// LevelSetup, all data from .ani file in editable form

LevelSetup::LevelSetup(const char* afilename)
{
	filename = NULL;
	scale = 1;
	load(afilename);
};

LevelSetup::~LevelSetup()
{
	free((void*)filename);
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
	// load scale
	if(1!=fscanf(f,"scale = %f\n",&scale))
		return false;
	// load objects
	objects.clear();
	unsigned tmpobj;
	while(1==fscanf(f,"object = %d\n",&tmpobj))
		objects.push_back(tmpobj-1);
	// load frames
	frames.clear();
	AnimationFrame tmp;
	while(tmp.load(f))
	{
		tmp.validate(objects.size());
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
	// save scale
	fprintf(f,"scale = %.5f\n",scale);
	// save objects
	for(unsigned i=0;i<objects.size();i++)
	{
		fprintf(f,"object = %d\n",objects[i]+1);
	}
	fprintf(f,"\n");
	// save frames
	for(Frames::const_iterator i=frames.begin();i!=frames.end();i++)
	{
		if(!(*i).save(f))
			return false;
	}
	fclose(f);
	return true;
}

LevelSetup::Frames::iterator LevelSetup::getFrameByIndex(unsigned index)
{
	Frames::iterator i=frames.begin();
	for(unsigned j=0;j<index;j++) i++;
	return i;
}

float LevelSetup::getFrameTime(unsigned index) const
{
	float seconds = 0;
	if(frames.size()>1)
	{
		Frames::const_iterator i=frames.begin();
		while(i!=frames.end() && index--)
		{
			seconds += (*i).transitionToNextTime;
			i++;
		}
		// remove addition of last frame, animation stops at the beginning of last frame
		if(i==frames.end())
		{
			i--;
			seconds -= (*i).transitionToNextTime;
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
	while(i!=frames.end() && (*i).transitionToNextTime<absSeconds)
	{
		absSeconds -= (*i).transitionToNextTime;
		i++;
	}
	if(i==frames.end())
		return NULL;
	Frames::const_iterator j = i; j++;
	if(j==frames.end())
		return NULL;
	return (*i).blend(*j,absSeconds/(*i).transitionToNextTime);
}

unsigned LevelSetup::getFrameIndexByTime(float absSeconds, float* transitionDone, float* transitionTotal)
{
	unsigned result = 0;
	Frames::const_iterator i=frames.begin();
	while(i!=frames.end() && (*i).transitionToNextTime<=absSeconds)
	{
		absSeconds -= (*i).transitionToNextTime;
		i++;
		result++;
	}
	*transitionDone = absSeconds;
	*transitionTotal = (*i).transitionToNextTime;
	return result;
}
