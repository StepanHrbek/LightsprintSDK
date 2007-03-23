#ifndef AUTOPILOT_H
#define AUTOPILOT_H

/////////////////////////////////////////////////////////////////////////////
//
// autopilot

#include "Lightsprint/DemoEngine/Camera.h"
#include "Lightsprint/RRMath.h"

struct AnimationFrame
{
	// camera and light
	de::Camera eyeLight[2];
	// dynamic objects
	typedef std::vector<rr::RRVec4> DynaPosRot;
	DynaPosRot dynaPosRot;
	// runtime generated
	de::Texture* thumbnail;

	AnimationFrame()
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
	const AnimationFrame* blend(const AnimationFrame* that, float alpha) const
	{
		assert(that);
		static AnimationFrame blended;
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
		for(unsigned i=0;i<this->dynaPosRot.size() && i<that->dynaPosRot.size();i++)
		{
			rr::RRVec4 tmp = rr::RRVec4(
				this->dynaPosRot[i]   *(1-alpha) + that->dynaPosRot[i]   *alpha,
				this->dynaPosRot[i][3]*(1-alpha) + that->dynaPosRot[i][3]*alpha); // RRVec4 operators are 3-component
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
	typedef std::list<AnimationFrame> Frames;
	Frames frames;

	LevelSetup(const char* afilename)
	{
		filename = NULL;
		load(afilename);
	};
	~LevelSetup()
	{
		free((void*)filename);
	}

	// load all from .ani
	// afilename is name of scene, e.g. path/koupelna4.3ds
	bool load(const char* afilename)
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
		if(1!=fscanf(f,"scale = %f\n\n",&scale))
			return false;
		frames.clear();
		AnimationFrame tmp;
		while(tmp.load(f))
			frames.push_back(tmp);
		fclose(f);
		return frames.size()>0;
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
		for(Frames::const_iterator i=frames.begin();i!=frames.end();i++)
		{
			if(!(*i).save(f))
				return false;
		}
		fclose(f);
		return true;
	}

	Frames::iterator getFrame(unsigned index)
	{
		Frames::iterator i=frames.begin();
		for(unsigned j=0;j<index;j++) i++;
		return i;
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
	Autopilot(LevelSetup* levelSetup)
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
	const AnimationFrame* autopilot(float seconds, bool* lightsChanged)
	{
		if(setup->frames.size()==0)
			return NULL;

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
		LevelSetup::Frames::const_iterator a = setup->frames.begin(); for(unsigned i=0;i<frameA;i++) a++;
		LevelSetup::Frames::const_iterator b = setup->frames.begin(); for(unsigned i=0;i<frameB;i++) b++;
		return (alpha<=0) ? &(*a) : (*a).blend(&(*b),alpha);
	}

	bool isTimeToChangeLevel()
	{
		return secondsSinceLastInteraction>TIME_TO_CHANGE_LEVEL+TIME_TO_AUTOPILOT;
	}

	LevelSetup* setup;
private:
	bool enabled;
	unsigned pilotedFrames; // number of frames already played
	float secondsSinceLastInteraction;
	float secondsSinceFrameA;
	unsigned frameA; // 0..MAX_FRAMES-1
	unsigned frameB; // 0..MAX_FRAMES-1
	char frameVisitedTimes[1000]; 
	
	// picks next frame that was less often selected
	unsigned getNextFrame()
	{
		if(!setup->frames.size())
		{
			return 0;
		}
//static unsigned q=0;return q++%LevelSetup::MAX_FRAMES;//!!!
		unsigned best = 0;
		for(unsigned i=0;i<100;i++)
		{
			unsigned j = rand()%setup->frames.size();
			if(frameVisitedTimes[j]<frameVisitedTimes[best]) best = j;			
		}
		frameVisitedTimes[best]++;
		return best;
	}
};

class AnimationEditor
{
public:
	AnimationEditor(LevelSetup* levelSetup)
	{
		setup = levelSetup;
		movieClipMap = de::Texture::load("maps\\movie_clip.jpg", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		cursorMap = de::Texture::load("maps\\cursor.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		frameCursor = 0;
	}
	~AnimationEditor()
	{
		delete movieClipMap;
		delete cursorMap;
	}
	void renderThumbnails(de::TextureRenderer* renderer)
	{
		unsigned index = 0;
		unsigned count = MAX(6,setup->frames.size()+1);
		for(LevelSetup::Frames::const_iterator i=setup->frames.begin();;i++,index++)
		{
			float x = index/(float)count;
			float y = 0;
			float w = 1/(float)count;
			float h = 0.15f;
			// thumbnail
			if(i!=setup->frames.end())
			{
				float intensity = 1;//(index==frameA || (index==frameB && secondsSinceFrameA>TIME_OF_STAY_STILL))?0.1f:1;
				float color[4] = {intensity,intensity,1,1};
				renderer->render2D(movieClipMap,color,x,y,w,h);
				if((*i).thumbnail)
					renderer->render2D((*i).thumbnail,NULL,x+w*0.05f,y+h*0.15f,w*0.9f,h*0.8f);
			}
			// cursor
			if(index==frameCursor)
			{
				GLboolean blend = glIsEnabled(GL_BLEND);
				glEnable(GL_BLEND);
				//glBlendFunc(GL_ONE,GL_ONE);
				//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				renderer->render2D(cursorMap,NULL,x,y+h*0.6f,w*0.3f,h*0.4f);
				if(!blend)
					glDisable(GL_BLEND);
			}
			// end for cycle
			if(i==setup->frames.end())
				break;
		}
	}

	bool keyboard(unsigned char c, int x, int y)
	{
		switch(c)
		{
			case 127: // DELETE
				{LevelSetup::Frames::iterator i=setup->frames.begin();
				for(unsigned j=0;j<frameCursor;j++) i++;
				if(i!=setup->frames.end())
					setup->frames.erase(i);
				return true;}
			//default:
			//	printf("key=%d\n",(int)c);
		}
		return false;
	}
	bool special(unsigned char c, int x, int y)
	{
		switch(c)
		{
			case GLUT_KEY_HOME:
				frameCursor = 0;
				return true;
			case GLUT_KEY_END:
				frameCursor = setup->frames.size();
				return true;
			case GLUT_KEY_LEFT:
				if(frameCursor) frameCursor--;
				return true;
			case GLUT_KEY_RIGHT:
				if(frameCursor<setup->frames.size()) frameCursor++;
				return true;
			case GLUT_KEY_INSERT:
				{LevelSetup::Frames::iterator i=setup->frames.begin();
				for(unsigned j=0;j<frameCursor;j++) i++;
				AnimationFrame tmp;
				extern void copySceneToAnimationFrame(AnimationFrame& frame);
				copySceneToAnimationFrame(tmp);
				setup->frames.insert(i,tmp);
				return true;}
			case GLUT_KEY_PAGE_UP:
				if(frameCursor)
				{
					std::swap(*setup->getFrame(frameCursor),*setup->getFrame(frameCursor-1));
					frameCursor--;
				}
				return true;
			case GLUT_KEY_PAGE_DOWN:
				if(frameCursor+1<setup->frames.size())
				{
					std::swap(*setup->getFrame(frameCursor),*setup->getFrame(frameCursor+1));
					frameCursor++;
				}
				return true;
		}
		return false;
	}

	unsigned frameCursor; // cislo snimku nad kterym je kurzor, 0..n
private:
	LevelSetup* setup;
	de::Texture* movieClipMap;
	de::Texture* cursorMap;
};

#endif
