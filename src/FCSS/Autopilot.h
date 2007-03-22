#ifndef AUTOPILOT_H
#define AUTOPILOT_H

/////////////////////////////////////////////////////////////////////////////
//
// autopilot

#include "Lightsprint/DemoEngine/Camera.h"
#include "Lightsprint/RRMath.h"

struct AutopilotFrame
{
	// camera and light
	de::Camera eyeLight[2];
	// dynamic objects
	typedef std::vector<rr::RRVec4> DynaPosRot;
	DynaPosRot dynaPosRot;
	// thumbnail
	de::Texture* thumbnail;

	AutopilotFrame()
	{
		de::Camera tmp[2] =
			{{{-3.266,1.236,1.230},9.120,0.100,1.3,45.0,0.3,1000.0},
			{{-0.791,1.370,1.286},3.475,0.550,1.0,70.0,1.0,20.0}};
		eyeLight[0] = tmp[0];
		eyeLight[1] = tmp[1];
		thumbnail = NULL;
	}

	// returns blend between this and that frame
	// return this for alpha=0, that for alpha=1
	const AutopilotFrame* blend(const AutopilotFrame* that, float alpha) const
	{
		assert(that);
		static AutopilotFrame blended;
		// blend eyeLight
		float* a = (float*)(this->eyeLight);
		float* b = (float*)(that->eyeLight);
		float* c = (float*)(blended.eyeLight);
		for(unsigned i=0;i<sizeof(eyeLight)/sizeof(float);i++)
			c[i] = a[i]*(1-alpha) + b[i]*alpha;
		blended.eyeLight[0].update(0);
		blended.eyeLight[1].update(0.3f);
		// blend dynaPosRot
		blended.dynaPosRot.clear();
		for(unsigned i=0;i<dynaPosRot.size();i++)
		{
			rr::RRVec4 tmp;
			(rr::RRVec3)tmp = this->dynaPosRot[i]*(1-alpha) + that->dynaPosRot[i]*alpha;
			tmp[3] = this->dynaPosRot[i][3]*(1-alpha) + that->dynaPosRot[i][3]*alpha; // RRVec4 operators are 3-component
			blended.dynaPosRot.push_back(tmp);
		}
		// blend thumbnail
		blended.thumbnail = NULL;
		return &blended;
	}

	// load frame from opened .ani file
	bool load(FILE* f)
	{
		if(!f) return false;
		// load eyeLight
		unsigned i = 0;
		if(9!=fscanf(f,"camera = {{%f,%f,%f},%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].height,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
			return false;
		i = 1;
		if(9!=fscanf(f,"light = {{%f,%f,%f},%f,%f,%f,%f,%f,%f}\n",&eyeLight[i].pos[0],&eyeLight[i].pos[1],&eyeLight[i].pos[2],&eyeLight[i].angle,&eyeLight[i].height,&eyeLight[i].aspect,&eyeLight[i].fieldOfView,&eyeLight[i].anear,&eyeLight[i].afar))
			return false;
		// load dynaPosRot
		dynaPosRot.clear();
		rr::RRVec4 tmp;
		while(4==fscanf(f,"object = {%f,%f,%f,%f}\n",&tmp[0],&tmp[1],&tmp[2],&tmp[3]))
			dynaPosRot.push_back(tmp);
		//if(0!=fscanf(f,"\n"))
		//	return false;
		rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
		return true;
	}

	// save frame to opened .ani file
	bool save(FILE* f) const
	{
		if(!f) return false;
		// save eyeLight
		unsigned i=0;
		fprintf(f,"camera = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].height,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
		i = 1;
		fprintf(f,"light = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f}\n",eyeLight[i].pos[0],eyeLight[i].pos[1],eyeLight[i].pos[2],fmodf(eyeLight[i].angle+100*3.14159265f,2*3.14159265f),eyeLight[i].height,eyeLight[i].aspect,eyeLight[i].fieldOfView,eyeLight[i].anear,eyeLight[i].afar);
		// save dynaPosRot
		for(DynaPosRot::const_iterator i=dynaPosRot.begin();i!=dynaPosRot.end();i++)
			fprintf(f,"object = {%.3f,%.3f,%.3f,%.3f}\n",(*i)[0],(*i)[1],(*i)[2],(*i)[3]);
		fprintf(f,"\n");
		rr::RRReporter::report(rr::RRReporter::INFO,"  frame with %d objects\n",dynaPosRot.size());
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
		//extern LevelSetup koupelna4;
		//*setup = koupelna4;
		setup->filename = _strdup(filename);
		setup->scale = 1;
		return setup;
	};
	LevelSetup()
	{
		filename = NULL;
		scale = 1;
		numFrames = 0;
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
		rr::RRReporter::report(rr::RRReporter::INFO,"Loading %s...\n",aniname);
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
		rr::RRReporter::report(rr::RRReporter::INFO,"Saving %s...\n",aniname);
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
