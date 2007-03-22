#ifndef AUTOPILOT_H
#define AUTOPILOT_H

/////////////////////////////////////////////////////////////////////////////
//
// autopilot

#include "Lightsprint/DemoEngine/Camera.h"
#include "Lightsprint/RRMath.h"

struct AutopilotFrame
{
	de::Camera eyeLight[2];
	float dynaPosRot[DYNAOBJECTS][4];
	float staySeconds;
	de::Texture* thumbnail;

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AutopilotFrame* blend(const AutopilotFrame* that, float alpha) const
	{
		assert(that);
		static AutopilotFrame blended;
		float* a = (float*)this;
		float* b = (float*)that;
		float* c = (float*)&blended;
		for(unsigned i=0;i<sizeof(blended)/sizeof(float);i++)
			c[i] = a[i]*(1-alpha) + b[i]*alpha;
		blended.eyeLight[0].update(0);
		blended.eyeLight[1].update(0.3f);
		blended.thumbnail = NULL;
		return &blended;
	}

	// load frame from opened .ani file
	bool load(FILE* f)
	{
		if(!f) return false;
		for(unsigned i=0;i<2;i++)
			if(9!=fscanf(f,"{{%f,%f,%f},%f,%f,%f,%f,%f,%f},\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].height,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
				return false;
		for(unsigned i=0;i<DYNAOBJECTS;i++)
			if(4!=fscanf(f,"{%f,%f,%f,%f},\n",&dynaPosRot[i][0],&dynaPosRot[i][1],&dynaPosRot[i][2],&dynaPosRot[i][3]))
				return false;
		if(0!=fscanf(f,"\n"))
			return false;
		return true;
	}

	// save frame to opened .ani file
	bool save(FILE* f) const
	{
		if(!f) return false;
		for(unsigned i=0;i<2;i++)
			fprintf(f,"{{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f},\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].height,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
		for(unsigned i=0;i<DYNAOBJECTS;i++)
			fprintf(f,"{%.3f,%.3f,%.3f,%.3f},\n",dynaPosRot[i][0],dynaPosRot[i][1],dynaPosRot[i][2],dynaPosRot[i][3]);
		fprintf(f,"\n");
		return true;
	}
};

struct LevelSetup
{
	// constant setup
	const char* filename;
	float scale;
	enum {MAX_FRAMES=5};
	AutopilotFrame frames[MAX_FRAMES];
	unsigned numFrames;

	// create
	static LevelSetup* create(const char* filename)
	{
		LevelSetup* setup = new LevelSetup;
		extern LevelSetup koupelna4;
		*setup = koupelna4;
		setup->filename = _strdup(filename);
		setup->scale = 1;
		return setup;
	};
	void clear()
	{
		memset(this,0,sizeof(*this));
	}
	~LevelSetup()
	{
		//free((void*)filename);
	}

	// load all from .ani
	// afilename is name of scene, e.g. path/koupelna4.3ds
	bool load(const char* afilename)
	{
		if(!afilename)
			return false;
		if(filename)
			free((void*)filename);
		filename = _strdup(afilename);
		char* aniname = _strdup(filename);
		strcpy(aniname+strlen(aniname)-3,"ani");
		FILE* f = fopen(aniname,"rt");
		free(aniname);
		if(1!=fscanf(f,"scale = %f\n\n",&scale))
			return false;
		numFrames = 0;
		while(frames[numFrames].load(f))
			numFrames++;
		fclose(f);
		return numFrames>0;
	}

	// save all to .ani file
	bool save() const
	{
		char* aniname = _strdup(filename);
		strcpy(aniname+strlen(aniname)-3,"ani");
		FILE* f = fopen(aniname,"wt");
		free(aniname);
		fprintf(f,"scale = %.5f\n\n",scale);
		for(unsigned i=0;i<MAX_FRAMES;i++)
		{
			if(!frames[i].save(f))
				return false;
		}
		fclose(f);
		return true;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// Autopilot, navigates camera, light, characters according to LevelSetup

class Autopilot
{
public:
	enum 
	{
		TIME_TO_AUTOPILOT    = 15, // seconds, time of inactivity before autopilot starts
		TIME_OF_FLIGHT       = 5,  // seconds, length of flight between two frames
		TIME_OF_STAY_STILL   = 4,  // seconds, length of flight between two frames
		TIME_TO_CHANGE_LEVEL = 60,
	};
	Autopilot(const LevelSetup* levelSetup)
	{
		memset(this,0,sizeof(*this));
		setup = levelSetup;
		enabled = true;
		secondsSinceLastInteraction = TIME_TO_AUTOPILOT;
		frameA = getNextFrame();
		frameB = getNextFrame();
	}
	void reportInteraction()
	{
		secondsSinceLastInteraction = 0;
		enabled = false;
	}

	// generates new positions for eye, camera, characters
	const AutopilotFrame* autopilot(float seconds, bool* lightsChanged)
	{
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
		return (alpha<=0) ? &setup->frames[frameA] : setup->frames[frameA].blend(&setup->frames[frameB],alpha);
	}

	bool isTimeToChangeLevel()
	{
		return secondsSinceLastInteraction>TIME_TO_CHANGE_LEVEL+TIME_TO_AUTOPILOT;
	}

	const LevelSetup* setup;
private:
	bool enabled;
	unsigned pilotedFrames; // number of frames
	float secondsSinceLastInteraction;
	float secondsSinceFrameA;
	unsigned frameA; // 0..MAX_FRAMES-1
	unsigned frameB; // 0..MAX_FRAMES-1
	char frameVisitedTimes[LevelSetup::MAX_FRAMES]; // how many times was frame visited

	// picks next frame that was less often selected
	unsigned getNextFrame()
	{
//static unsigned q=0;return q++%LevelSetup::MAX_FRAMES;//!!!
		unsigned best = 0;
		for(unsigned i=0;i<100;i++)
		{
			unsigned j = rand()%LevelSetup::MAX_FRAMES;
			if(frameVisitedTimes[j]<frameVisitedTimes[best]) best = j;			
		}
		frameVisitedTimes[best]++;
		return best;
	}
};


#endif
