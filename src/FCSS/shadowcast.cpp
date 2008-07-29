//#define BUGS
#define MAX_INSTANCES              10  // max number of light instances aproximating one area light
unsigned INSTANCES_PER_PASS;
#define SHADOW_MAP_SIZE_SOFT       512
#define SHADOW_MAP_SIZE_HARD       2048
#define LIGHTMAP_SIZE_FACTOR       10
#define LIGHTMAP_QUALITY           100
//#define BACKGROUND_WORKER // nejde v linuxu
#if defined(NDEBUG) && defined(WIN32)
	//#define SET_ICON
#else
	#define CONSOLE
#endif
//#define SUPPORT_LIGHTMAPS
//#define SUPPORT_WATER
//#define CORNER_LOGO
//#define CALCULATE_WHEN_PLAYING_PRECALCULATED_MAPS // calculate() is necessary only for correct envmaps (dynamic objects)
//#define RENDER_OPTIMIZED // kresli multiobjekt, ale non-indexed, takze jsou ohromne vertex buffery, pomalejsi. udajne nepodporuje fireball (zda se ale ze vse funguje)
//#define THREE_ONE
//#define CFG_FILE "3+1.cfg"
//#define CFG_FILE "LightsprintDemo.cfg"
//#define CFG_FILE "Candella.cfg"
#define PRODUCT_NAME "Lightsmark 2008"
#define CFG_FILE "Lightsmark2008.cfg"
//#define CFG_FILE "mgf.cfg"
//#define CFG_FILE "test.cfg"
//#define CFG_FILE "eg-flat1.cfg"
//#define CFG_FILE "eg-quake.cfg"
//#define CFG_FILE "eg-sponza.cfg"
//#define CFG_FILE "eg-sponza-sun.cfg"
//#define CFG_FILE "Lowpoly.cfg"
int fullscreen = 1;
bool startWithSoftShadows = 0;
int resolutionx = 1280;
int resolutiony = 1024;
bool supportEditor = 0;
bool bigscreenCompensation = 0;
bool bigscreenSimulator = 0;
bool showTimingInfo = 0;
bool captureVideo = 0;
float splitscreen = 0.0f; // 0=disabled, 0.5=leva pulka obrazovky ma konst.ambient
bool supportMusic = 1;
/*
co jeste pomuze:
30% za 3 dny: detect+reset po castech, kratsi improve
20% za 8 dnu:
 thread0: renderovat prechod mezi kanalem 0 a 1 podle toho v jake fazi je thread1
 thread1: vlastni gl kontext a nekonecny cyklus: detekce, update, 0.2s vypoctu, read results do kanalu k, k=1-k

!kdyz nenactu textury a vse je bile, vypocet se velmi rychle zastavi, mozna distribuuje ale nerefreshuje

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce (komplikovane pri multipass renderu)
scita se primary a zkorigovany indirect, vysledkem je ze primo osvicena mista jsou svetlejsi nez maji byt
*/

#ifdef MINGW
	#include <limits> // nutne aby uspel build v gcc4.3
#endif
#include "Level.h" // must be first, so collada is included before demoengine (#define SAFE_DELETE collides)
#include "Lightsprint/GL/Timer.h"
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <list>
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <GL/glew.h>
#ifdef _WIN32
	#include <GL/wglew.h>
#endif
#include <GL/glut.h>
#ifdef _WIN32
	#include <direct.h> // _chdir
	#include <shellapi.h> // CommandLineToArgvW
#else
	#include <unistd.h> // chdir
	#define _chdir chdir
#endif
#ifdef BACKGROUND_WORKER
	#include <process.h>
#endif
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/IO/ImportScene.h"
#ifdef SUPPORT_WATER
	#include "Lightsprint/GL/Water.h"
#endif
#include "DynamicObject.h"
#include "Bugs.h"
#include "AnimationEditor.h"
#include "Autopilot.h"
#include "DemoPlayer.h"
#include "DynamicObjects.h"
#include "../LightsprintCore/RRDynamicSolver/report.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
//
// globals

AnimationFrame currentFrame(0);
GLUquadricObj *quadric;
rr_gl::RealtimeLight* realtimeLight = NULL;
#ifdef SUPPORT_WATER
	rr_gl::Water* water = NULL;
#endif
#ifdef CORNER_LOGO
	rr_gl::Texture* lightsprintMap = NULL; // small logo in the corner
#endif
rr_gl::Program* ambientProgram;
rr_gl::TextureRenderer* skyRenderer;
rr_gl::UberProgram* uberProgram;
rr_gl::UberProgramSetup uberProgramGlobalSetup;
int winWidth = 0;
int winHeight = 0;
bool needLightmapCacheUpdate = false;
int wireFrame = 0;
int needMatrixUpdate = 1;
int showHelp = 0; // 0=none, 1=help
int showLightViewFrustum = 0;
bool modeMovingEye = 0;
unsigned movingEye = 0;
unsigned movingLight = 0;
bool needRedisplay = 0;
bool needReportEyeChange = 0;
bool needReportLightChange = 0;
bool needImmediateDDI = 1; // nastaveno pri strihu, pri rucnim pohybu svetlem mysi nebo klavesnici
#ifdef BUGS
bool gameOn = 1;
#else
bool gameOn = 0;
#endif
Level* level = NULL;
bool seekInMusicAtSceneSwap = false;
//class DynamicObjects* dynaobjects;
bool shotRequested;
DemoPlayer* demoPlayer = NULL;
unsigned selectedObject_indexInDemo = 0;
bool renderInfo = 1;
const char* cfgFile = CFG_FILE;
rr_gl::RRDynamicSolverGL::DDIQuality lightStability = rr_gl::RRDynamicSolverGL::DDI_AUTO;
bool preciseTimer = false;

/////////////////////////////////////////////////////////////////////////////
//
// Fps

#include <queue>

class Fps
{
public:
	static Fps* create()
	{
		Fps* fps = new Fps();
		if(!fps->mapFps) goto err;
		for(unsigned i=0;i<10;i++)
		{
			if(!fps->mapDigit[i]) goto err;
		}
		return fps;
err:
		delete fps;
		return NULL;
	}
	void render()
	{
		if(skyRenderer->render2dBegin(NULL))
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			char fpsstr[10];
			sprintf(fpsstr,"%d",fpsToRender);
			float x = 0.82f;
			float y = 0.0f;
			float wpix = 1/1280.f;
			float hpix = 1/960.f;
			skyRenderer->render2dQuad(rr_gl::getTexture(mapFps),x,y-0.012f,mapFps->getWidth()*wpix,mapFps->getHeight()*hpix);
			x += mapFps->getWidth()*wpix+0.01f;
			for(char* c=fpsstr;*c;c++)
			{
				rr::RRBuffer* digit = mapDigit[*c-'0'];
				skyRenderer->render2dQuad(rr_gl::getTexture(digit),x,y,digit->getWidth()*wpix,digit->getHeight()*hpix);
				x += digit->getWidth()*wpix - 0.005f;
			}
			skyRenderer->render2dEnd();
			glDisable(GL_BLEND);
		}
	}
	void update()
	{
		//if(demoPlayer->getPaused()) return;
		TIME now = GETTIME;
		while(times.size() && now-times.front()>PER_SEC) times.pop();
		times.push(GETTIME);
		fpsToRender = (unsigned)times.size();
		if(!demoPlayer->getPaused()) frames++;
		float seconds = demoPlayer->getDemoPosition();
		fpsAvg = frames/MAX(0.01f,seconds);
	}
	float getAvg()
	{
		return fpsAvg;
	}
	~Fps()
	{
		for(unsigned i=0;i<10;i++) delete mapDigit[i];
		delete mapFps;
	}
protected:
	Fps()
	{
		mapFps = rr::RRBuffer::load("maps/txt-fps.png");
		for(unsigned i=0;i<10;i++)
		{
			char buf[40];
			sprintf(buf,"maps/txt-%d.png",i);
			mapDigit[i] = rr::RRBuffer::load(buf);
		}
		frames = 0;
		fpsToRender = 0;
		fpsAvg = 0;
	}
	rr::RRBuffer* mapDigit[10];
	rr::RRBuffer* mapFps;
	std::queue<TIME> times;
	unsigned frames; // kolik snimku se stihlo behem 1 prehrani animace
	unsigned fpsToRender; // jake cislo renderovat
	float fpsAvg;
};

Fps* g_fps;

/////////////////////////////////////////////////////////////////////////////

bool exiting = false;

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if(gfxRelated)
		rr::RRReporter::report(rr::INF1,"\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	if(glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
		glutLeaveGameMode();
	else
		glutDestroyWindow(glutGetWindow());
#ifdef CONSOLE
	printf("\n\nPress ENTER to close.");
	fgetc(stdin);
#endif
	exiting = true;
	exit(0);
}

// sets our globals and rendering pipeline according to currentFrame.shadowType
void setShadowTechnique()
{
	// early exit
	static unsigned oldShadowType = 999;
	if(currentFrame.shadowType == oldShadowType) return;
	oldShadowType = currentFrame.shadowType;

	// cheap changes (no GL commands)
	realtimeLight->setNumInstances((currentFrame.shadowType<3)?1:INSTANCES_PER_PASS);

	// expensive changes (GL commands)
	if(currentFrame.shadowType>=2)
	{
		realtimeLight->setShadowmapSize(SHADOW_MAP_SIZE_SOFT);
		realtimeLight->softShadowsAllowed = true;
	}
	else
	{
		realtimeLight->setShadowmapSize(SHADOW_MAP_SIZE_HARD);
		realtimeLight->softShadowsAllowed = false;
	}
	level->solver->reportDirectIlluminationChange(0,true,false);
}

void init_gl_resources()
{
	quadric = gluNewQuadric();

	realtimeLight = new rr_gl::RealtimeLight(&currentFrame.light,MAX_INSTANCES,SHADOW_MAP_SIZE_SOFT);
//	realtimeLight = new rr_gl::RealtimeLight(*rr::RRLight::createSpotLightNoAtt(rr::RRVec3(-1.802,0.715,0.850),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),40*3.14159f/180,0.1f));
//	realtimeLight->parent = &currentFrame.light;
	realtimeLight->transparentMaterialShadows = rr_gl::RealtimeLight::FULLY_OPAQUE_SHADOWS; // disables alpha keying in shadows (to stay compatible with Lightsmark 2007)

#ifdef CORNER_LOGO
	lightsprintMap = rr_gl::Texture::load("maps/logo230awhite.png", NULL, false, false, GL_NEAREST, GL_NEAREST, GL_CLAMP, GL_CLAMP);
#endif
	g_fps = Fps::create();

	uberProgram = rr_gl::UberProgram::create("shaders/ubershader.vs", "shaders/ubershader.fs");
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	ambientProgram = uberProgram->getProgram(uberProgramSetup.getSetupString());

#ifdef SUPPORT_WATER
	water = new rr_gl::Water("shaders/",true,false);
#endif
	skyRenderer = new rr_gl::TextureRenderer("shaders/");

	if(!ambientProgram)
		error("\nFailed to compile or link GLSL program.\n",true);
}

void done_gl_resources()
{
	SAFE_DELETE(skyRenderer);
	SAFE_DELETE(uberProgram);
	SAFE_DELETE(g_fps);
#ifdef CORNER_LOGO
	SAFE_DELETE(lightsprintMap);
#endif
//!!! uvolnuje se vickrat SAFE_DELETE(realtimeLight);
	gluDeleteQuadric(quadric);
}

/////////////////////////////////////////////////////////////////////////////
//
// background worker

#ifdef BACKGROUND_WORKER
#if 1
// 4 kriticke sekce, bezi dobre i na 1 jadru
class BackgroundWorker
{
public:
	BackgroundWorker()
	{
		work = NULL;
		workNumber = 0;
		for(unsigned i=0;i<4;i++)
		{
			InitializeCriticalSection(&cs[i]);
		}
		EnterCriticalSection(&cs[0]);
		EnterCriticalSection(&cs[3]);
		_beginthread(worker,0,this);
	}
	~BackgroundWorker()
	{
	}
	void addWork(DemoPlayer* _work)
	{
		RR_ASSERT(!work);
		RR_ASSERT(_work);
		work = _work;
		LeaveCriticalSection(&cs[(workNumber+3)&3]); // unblock 2 firstgate
	//printf("\n<%d ",workNumber+2);
		EnterCriticalSection(&cs[(workNumber+2)&3]); // block 2 secondgate
	//printf(" %d>",workNumber+2);
		LeaveCriticalSection(&cs[(workNumber+0)&3]); // let worker start 0    worker musi mit lockle 1, ja musim mit unlockle 3 a lockle 2 (splneno)
	}
	void waitForCompletion()
	{
	//printf("waitforcompletion(%d...",workNumber+1);
		EnterCriticalSection(&cs[(workNumber+1)&3]); // wait for worker finish 0
	//printf(" %d)",workNumber+1);
		workNumber += 2;
		RR_ASSERT(!work);
	}
protected:
	unsigned workNumber;
	DemoPlayer* work;
	CRITICAL_SECTION cs[4];
	static void worker(void* w)
	{
		BackgroundWorker* worker = (BackgroundWorker*)w;
		EnterCriticalSection(&worker->cs[1]);
		SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
		unsigned localWorkNumber = 0;
		while(1)
		{
		//printf(" %d] waitforjob[%d...",localWorkNumber+1,localWorkNumber);
			EnterCriticalSection(&worker->cs[(localWorkNumber+0)&3]); // wait for incoming job 0
		//printf(" %d]",localWorkNumber);
			RR_ASSERT(worker->work);
			worker->work->getDynamicObjects()->updateSceneDynamic(level->solver);
			worker->work = NULL;
		//printf("work");
		//printf("[%d ",localWorkNumber+3);
			EnterCriticalSection(&worker->cs[(localWorkNumber+3)&3]);
			LeaveCriticalSection(&worker->cs[(localWorkNumber+0)&3]);
			LeaveCriticalSection(&worker->cs[(localWorkNumber+1)&3]); // finish 0
			localWorkNumber += 2;
		}
	}
};
#else
// sleep(0), uplne se zastavi na 1 jadru
class BackgroundWorker
{
public:
	BackgroundWorker()
	{
		work = NULL;
		_beginthread(worker,0,this);
	}
	void addWork(DemoPlayer* _work)
	{
		RR_ASSERT(!work);
		RR_ASSERT(_work);
		work = _work;
	}
	void waitForCompletion()
	{
		while(work) Sleep(0);
	}
protected:
	DemoPlayer* work;
	static void worker(void* w)
	{
		BackgroundWorker* worker = (BackgroundWorker*)w;
		SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
		while(1)
		{
			while(!worker->work) Sleep(0);
			worker->work->getDynamicObjects()->updateSceneDynamic(level->solver);
			worker->work = NULL;
		}
	}
};
#endif

BackgroundWorker* g_backgroundWorker = NULL;

#endif


/////////////////////////////////////////////////////////////////////////////
//
// Solver

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance, rr_gl::Camera* camera, const rr::RRLight* renderingFromThisLight);
void updateMatrices();

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("shaders/",lightStability)
	{
		setDirectIlluminationBoost(2);
	}
protected:
	virtual void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
	{
		::renderScene(uberProgramSetup,0,&currentFrame.eye,renderingFromThisLight);
	}
#ifdef BACKGROUND_WORKER
	virtual void calculate(CalculateParameters* params = NULL)
	{
		// assign background work: possibly updating triangleNumbers around dynobjects
		if(g_backgroundWorker) g_backgroundWorker->addWork(demoPlayer);
		// possibly calculate (update shadowmaps, DDI)
		RRDynamicSolverGL::calculate(params);
		// possibly wait for background work completion
		if(g_backgroundWorker) g_backgroundWorker->waitForCompletion();
	}
#endif
};

// called from Level.cpp
rr_gl::RRDynamicSolverGL* createSolver()
{
	return new Solver();
}


/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects


const rr::RRCollider* getSceneCollider()
{
	if(!level) return NULL;
	return level->solver->getMultiObjectCustom()->getCollider();
}


/////////////////////////////////////////////////////////////////////////////

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void drawLight(void)
{
	ambientProgram->useIt();
	glPushMatrix();
	glTranslatef(currentFrame.light.pos[0]-0.3f*currentFrame.light.dir[0], currentFrame.light.pos[1]-0.3f*currentFrame.light.dir[1], currentFrame.light.pos[2]-0.3f*currentFrame.light.dir[2]);
	glColor3f(1,1,0);
	gluSphere(quadric, 0.05f, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	currentFrame.eye.aspect = winHeight ? (float) winWidth / (float) winHeight : 1;
	currentFrame.eye.update();
	currentFrame.light.update();
	needMatrixUpdate = false;
}

void renderSceneStatic(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance, const rr::RRLight* renderingFromThisLight)
{
	if(!level) return;

#ifdef RENDER_OPTIMIZED
	level->rendererOfScene->useOptimizedScene();
#else
	// update realtime layer 0
	static unsigned solutionVersion = 0;
	if((uberProgramSetup.LIGHT_INDIRECT_auto || uberProgramSetup.LIGHT_INDIRECT_VCOLOR) && // update vbuf only when needed (update may call calculate()!)
		level->solver->getSolutionVersion()!=solutionVersion)
	{
		solutionVersion = level->solver->getSolutionVersion();
		level->solver->updateLightmaps(0,-1,-1,NULL,NULL,NULL);
	}
	if(demoPlayer->getPaused())
	{
		// paused -> show realtime layer 0
		level->rendererOfScene->useOriginalScene(0);
	}
	else
	{
		// playing -> show precomputed layers (with fallback to realtime layer 0)
		//const AnimationFrame* frameBlended = level->pilot.setup->getFrameByTime(demoPlayer->getPartPosition());
		//demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup,*frameBlended,true);
		float transitionDone = 0;
		float transitionTotal = 0;
		unsigned frameIndex0 = level->pilot.setup->getFrameIndexByTime(demoPlayer->getPartPosition(),&transitionDone,&transitionTotal);
		const AnimationFrame* frame0 = level->pilot.setup->getFrameByIndex(frameIndex0);
		const AnimationFrame* frame1 = level->pilot.setup->getFrameByIndex(frameIndex0+1);
		level->rendererOfScene->useOriginalSceneBlend(frame0?frame0->layerNumber:0,frame1?frame1->layerNumber:0,transitionTotal?transitionDone/transitionTotal:0,0);
	}
#endif

	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	level->rendererOfScene->setBrightnessGamma(&globalBrightnessBoosted,globalGammaBoosted);

	level->rendererOfScene->setLDM(uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? level->getLDMLayer() : UINT_MAX );

	rr::RRVector<rr_gl::RealtimeLight*> lights;
	lights.push_back(realtimeLight);
	realtimeLight->lightDirectMap = demoPlayer->getProjector(currentFrame.projectorIndex);
	level->rendererOfScene->setParams(uberProgramSetup,&lights,renderingFromThisLight,false);
	level->rendererOfScene->render();
}

// camera must be already set in OpenGL, this one is passed only for frustum culling
void renderScene(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance, rr_gl::Camera* camera, const rr::RRLight* renderingFromThisLight)
{
	// render static scene
	assert(!uberProgramSetup.OBJECT_SPACE); 
	renderSceneStatic(uberProgramSetup,firstInstance,renderingFromThisLight);
	if(uberProgramSetup.FORCE_2D_POSITION) return;
	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	assert(demoPlayer);
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	rr::RRVector<rr_gl::RealtimeLight*> lights;
	lights.push_back(realtimeLight);
	realtimeLight->lightDirectMap = demoPlayer->getProjector(currentFrame.projectorIndex);
	demoPlayer->getDynamicObjects()->renderSceneDynamic(level->solver,uberProgram,uberProgramSetup,camera,&lights,firstInstance,&globalBrightnessBoosted,globalGammaBoosted);
}

void drawEyeViewShadowed(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	if(!level) return;

	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = (globalBrightnessBoosted[0]!=1 || globalBrightnessBoosted[1]!=1 || globalBrightnessBoosted[2]!=1 || globalBrightnessBoosted[3]!=1)?1:0;
	uberProgramSetup.POSTPROCESS_GAMMA = (globalGammaBoosted!=1)?1:0;
	uberProgramSetup.POSTPROCESS_BIGSCREEN = bigscreenSimulator;

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	currentFrame.eye.setupForRender();

	renderScene(uberProgramSetup,firstInstance,&currentFrame.eye,NULL);

#ifdef BUGS
	if(gameOn)
	{
		static Timer t;
		static bool runs = false;
		float seconds = runs?t.Watch():0.1f;
		CLAMP(seconds,0.001f,0.5f);
		t.Start();
		runs = true;
		if(!demoPlayer->getPaused())
			level->bugs->tick(seconds);
		level->bugs->render();
	}
#endif

	if(supportEditor)
		drawLight();
	if(showLightViewFrustum)
	{
		level->solver->renderLights();
	}
}

void drawEyeViewSoftShadowed(void)
{
	unsigned numInstances = realtimeLight->getNumInstances();
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	RR_ASSERT(numInstances<=INSTANCES_PER_PASS);

#ifdef SUPPORT_WATER
		// update water reflection
		if(water && level->pilot.setup->renderWater)
		{
			water->updateReflectionInit(winWidth/4,winHeight/4,&currentFrame.eye,level->pilot.setup->waterLevel);
			glClear(GL_DEPTH_BUFFER_BIT);
			rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_INDIRECT_CONST = currentFrame.wantsConstantAmbient();
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = currentFrame.wantsVertexColors();
			uberProgramSetup.LIGHT_INDIRECT_MAP = currentFrame.wantsLightmaps();
			uberProgramSetup.LIGHT_INDIRECT_auto = currentFrame.wantsLightmaps();
			uberProgramSetup.LIGHT_INDIRECT_ENV = false;
			drawEyeViewShadowed(uberProgramSetup,0);
			water->updateReflectionDone();
		}
#endif
		glClear(GL_DEPTH_BUFFER_BIT);
		if(splitscreen)
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(0,0,(unsigned)(winWidth*splitscreen),winHeight);
			rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 1;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_auto = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = false;
			uberProgramSetup.FORCE_2D_POSITION = false;
			drawEyeViewShadowed(uberProgramSetup,0);
			glScissor((unsigned)(winWidth*splitscreen),0,winWidth-(unsigned)(winWidth*splitscreen),winHeight);
		}

		if(numInstances>1)
		{
			// Z only pre-pass before expensive penumbra shadows
			// optional but improves fps from 60 to 80
			rr_gl::UberProgramSetup uberProgramSetup;
			currentFrame.eye.setupForRender();
			renderScene(uberProgramSetup,0,&currentFrame.eye,NULL);
		}

		// render everything except water
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = numInstances;
		uberProgramSetup.SHADOW_PENUMBRA = true;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_INDIRECT_CONST = currentFrame.wantsConstantAmbient();
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = currentFrame.wantsVertexColors();
		uberProgramSetup.LIGHT_INDIRECT_MAP = currentFrame.wantsLightmaps();
		uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP = !currentFrame.wantsConstantAmbient();
		uberProgramSetup.LIGHT_INDIRECT_auto = currentFrame.wantsLightmaps();
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = false;

		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,0);

		if(splitscreen)
			glDisable(GL_SCISSOR_TEST);

#ifdef SUPPORT_WATER
		// render water
		if(water && level->pilot.setup->renderWater)
		{
			water->render(100);
		}
#endif
}

// captures current scene into thumbnail
void updateThumbnail(AnimationFrame& frame)
{
	//rr::RRReporter::report(rr::INF1,"Updating thumbnail.\n");
	// set frame
	demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup,frame,true);
	// calculate
	level->solver->calculate();
	/*/ update shadows in advance, so following render doesn't touch FBO
	unsigned numInstances = realtimeLight->getNumInstances();
	for(unsigned j=0;j<numInstances;j++)
	{
		updateDepthMap(j,numInstances);
	}*/
	// render into thumbnail
	if(!frame.thumbnail)
		frame.thumbnail = rr::RRBuffer::create(rr::BT_2D_TEXTURE,160,120,1,rr::BF_RGB,true,NULL);
	glViewport(0,0,160,120);
	//frame.thumbnail->renderingToBegin();
	drawEyeViewSoftShadowed();
	//frame.thumbnail->renderingToEnd();
	rr_gl::getTexture(frame.thumbnail)->bindTexture();
	glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,160,120);
	glViewport(0,0,winWidth,winHeight);
}

// fills animation frame with current scene
// generates thumbnail
// called from AnimationEditor
void copySceneToAnimationFrame(AnimationFrame& frame, const LevelSetup* setup)
{
	demoPlayer->getDynamicObjects()->copySceneToAnimationFrame_ignoreThumbnail(frame,setup);
	updateThumbnail(frame);
}

static void output(int x, int y, const char *string)
{
	glRasterPos2i(x, y);
	int len = (int) strlen(string);
	for(int i = 0; i < len; i++)
	{
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, string[i]);
	}
//	glCallLists(strlen(string), GL_UNSIGNED_BYTE, string); 
}

static void drawHelpMessage(int screen)
{
	if(shotRequested) return;
//	if(!big && gameOn) return;

/* misto glutu pouzije truetype fonty z windows
	static bool fontInited = false;
	if(!fontInited)
	{
		fontInited = true;
		HFONT font = CreateFont(	-24,							// Height Of Font
			0,								// Width Of Font
			0,								// Angle Of Escapement
			0,								// Orientation Angle
			FW_THIN,						// Font Weight
			FALSE,							// Italic
			FALSE,							// Underline
			FALSE,							// Strikeout
			ANSI_CHARSET,					// Character Set Identifier
			OUT_TT_PRECIS,					// Output Precision
			CLIP_DEFAULT_PRECIS,			// Clipping Precision
			ANTIALIASED_QUALITY,			// Output Quality
			FF_DONTCARE|DEFAULT_PITCH,		// Family And Pitch
			"Arial");					// Font Name
		SelectObject(wglGetCurrentDC(), font); 
		wglUseFontBitmaps(wglGetCurrentDC(), 0, 127, 1000); 
		glListBase(1000); 
		glNewList(999,GL_COMPILE);

		// set state
		ambientProgram->useIt();
		glDisable(GL_DEPTH_TEST);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, winWidth, winHeight, 0);
		// box
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glColor4f(0.0,0.0,0.0,0.6);
		glRecti(MIN(winWidth-30,500), 30, 30, MIN(winHeight-30,100));
		glDisable(GL_BLEND);
		// text
		glColor3f(1,1,1);
		output(100,60,"For more information on Realtime Global Illumination");
		output(100,80,"or Penumbra Shadows, visit http://lightsprint.com");
		// restore state
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glEnable(GL_DEPTH_TEST);

		glEndList();
	}

	glCallList(999);
	return;*/

	static const char *message[3][30] = 
	{
		{
		"H - help",
		NULL
		},
		{
#ifdef THREE_ONE
		PRODUCT_NAME,
#else
		PRODUCT_NAME ", (C) Stepan Hrbek",
#endif
		"",
		"Controls:",
		" space         - pause/play",
		" mouse         - look",
		" arrows,wsadqz - move",
		" left button   - toggle camera/light control",
		"",
		"Extra controls:",
		" F1            - help",
		" F2,F3,F4      - shadows: hard/soft/penumbra",
#ifdef SUPPORT_LIGHTMAPS
		" F5,F6,F7,F8   - ambient: none/const/realtime/precalc",
#else
		" F5,F6,F7      - ambient: none/const/realtimeradiosity",
#endif
//		" F11           - save screenshot",
		" wheel         - zoom",
		" x,c           - lean",
		" +,-,*,/       - brightness/contrast",
		" 1,2,3,4...    - move n-th object",
		" enter         - maximize window",
#ifdef THREE_ONE
		"",
		"For more information on Realtime Global Illumination",
		"or Penumbra Shadows, visit http://lightsprint.com",
#endif
/*
		" 'f'   - toggle showing spotlight frustum",
		" 'a'   - cycle through linear, rectangular and circular area light",
		" 's'   - change spotlight",
		" 'w'   - toggle wire frame",
		"",
		"'p'   - narrow shadow frustum field of view",
		"'P'   - widen shadow frustum field of view",
		"'n'   - compress shadow frustum near clip plane",
		"'N'   - expand shadow frustum near clip plane",
		"'c'   - compress shadow frustum far clip plane",
		"'C'   - expand shadow frustum far clip plane",*/
		NULL,
		}
/*#ifndef THREE_ONE
		,{
		"Works of following people were used in Lightsprint Demo:",
		"",
		"  - Stepan Hrbek, Daniel Sykora  : realtime global illumination",
		"  - Mark Kilgard, Nate Robins    : GLUT library",
		"  - Milan Ikits, Marcelo Magallon: GLEW library",
		"  - many contributors            : FreeImage library",
		"  - Firelight Technologies       : FMOD library",
		"  - Matthew Fairfax              : 3ds loader",
		"  - Nicolas Baudrey              : bsp loader",
		"  - Vojta Nedved                 : \"Difficult life\" music",
		"  - Anthony Butler               : \"The Soremill\" scene",
		"  - David Cherry                 : \"Mortal Wounds\" scene",
		"  - Q                            : \"Triangulation\" scene",
		"  - Petr Stastny                 : \"Koupelna\" scene",
		"  - Sirda                        : \"Black man\", \"Man in hat\"",
		"  - orillionbeta                 : \"I Robot\" model",
		"  - ?                            : \"Woman statue\" model",
		"  - flipper42                    : \"Jessie\" model",
		"  - atp creations                : \"Scary frog alien\" model",
		"  - Stora_tomtefar               : \"Viking\" model",
		"  - Amethyst7                    : \"Purple Nebula\" skybox",
		NULL
		}
#endif*/
	};
	int i;

	ambientProgram->useIt();

	glDisable(GL_DEPTH_TEST);

	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, winWidth, winHeight, 0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glColor4f(0,0,0,0.6f);

	// Drawn clockwise because the flipped Y axis flips CCW and CW.
	if(screen /*|| demoPlayer->getPaused()*/)
	{
		unsigned rectWidth = 530;
		unsigned rectHeight = 360;
		glColor4f(0,0.1f,0.3f,0.6f);
		glRecti((winWidth+rectWidth)/2, (winHeight-rectHeight)/2, (winWidth-rectWidth)/2, (winHeight+rectHeight)/2);
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
		int x = (winWidth-rectWidth)/2+20;
		int y = (winHeight-rectHeight)/2+30;
		for(i=0; message[screen][i] != NULL; i++) 
		{
			if (message[screen][i][0] != '\0')
			{
				output(x, y, message[screen][i]);
			}
			y += 18;
		}
	}
	else
	if(showTimingInfo)
	{
		int x = 40, y = 50;
		glRecti(MIN(winWidth-30,500), 30, 30, MIN(winHeight-30,100));
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
		char buf[200];
		float demoLength = demoPlayer->getDemoLength();
		float musicLength = demoPlayer->getMusicLength();
		sprintf(buf,"demo %.1f/%.1fs, byt %.1f/%.1fs, music %.1f/%.1f",
			demoPlayer->getDemoPosition(),demoLength,
			demoPlayer->getPartPosition(),demoPlayer->getPartLength(),
			demoPlayer->getMusicPosition(),musicLength);
		output(x,y,buf);
		float transitionDone;
		float transitionTotal;
		unsigned frameIndex;
		if(demoPlayer->getPaused())
		{
			// paused: frame index = editor cursor
			frameIndex = level->animationEditor->frameCursor;
			transitionDone = 0;
			AnimationFrame* frame = level->pilot.setup->getFrameByIndex(frameIndex); // muze byt NULL (kurzor za koncem)
			transitionTotal = frame ? frame->transitionToNextTime : 0;
		}
		else
		{
			// playing: frame index computed from current time
			frameIndex = level->pilot.setup->getFrameIndexByTime(demoPlayer->getPartPosition(),&transitionDone,&transitionTotal);
		}
		const AnimationFrame* frame = level->pilot.setup->getFrameByIndex(frameIndex+1);
		sprintf(buf,"scene %d/%d, frame %d(%d)/%d, %.1f/%.1fs",
			demoPlayer->getPartIndex()+1,demoPlayer->getNumParts(),
			frameIndex+1,frame?frame->layerNumber:0,level->pilot.setup->frames.size(),
			transitionDone,transitionTotal);
		output(x,y+18,buf);
		sprintf(buf,"bright %.1f, gamma %.1f",
			currentFrame.brightness[0],currentFrame.gamma);
		output(x,y+36,buf);
	}
	else
	{
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
	}

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
}

void showImage(const rr_gl::Texture* tex)
{
	if(!tex) return;
	skyRenderer->render2D(tex,NULL,0,0,1,1);
	glutSwapBuffers();
}

void showOverlay(const rr::RRBuffer* tex)
{
	if(!tex) return;
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	float color[4] = {currentFrame.brightness[0],currentFrame.brightness[1],currentFrame.brightness[2],1};
	skyRenderer->render2D(rr_gl::getTexture(tex,false,false),color,0,0,1,1);
	glDisable(GL_BLEND);
}

void showOverlay(const rr::RRBuffer* logo,float intensity,float x,float y,float w,float h)
{
	if(!logo) return;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	float color[4] = {intensity,intensity,intensity,intensity};
	skyRenderer->render2D(rr_gl::getTexture(logo,true,false),color,x,y,w,h);
	glDisable(GL_BLEND);
}

/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

// prototypes
void keyboard(unsigned char c, int x, int y);
void enableInteraction(bool enable);

void display()
{
	REPORT(rr::RRReportInterval report(rr::INF3,"display()\n"));
	if(!winWidth) return; // can't work without window
	//printf("<Display.>\n");
	if(!level) return; // we can't render without scene, idle() should create it first

	// pro jednoduchost je to tady
	// kdyby to bylo u vsech stisku/pusteni/pohybu klaves/mysi a animaci,
	//  nevolalo by se to zbytecne v situaci kdy redisplay vyvola calculate() hlaskou ze zlepsil osvetleni
	// zisk by ale byl miniaturni
	level->solver->reportInteraction();

	if(needMatrixUpdate)
		updateMatrices();

	rr::RRDynamicSolver::CalculateParameters calculateParams = level->pilot.setup->calculateParams;
	if(currentFrame.wantsConstantAmbient()) calculateParams.qualityIndirectDynamic = 0; // disable direct detection and indirect calculation in "no radiosity" part
	calculateParams.secondsBetweenDDI = needImmediateDDI ? 0 : 0.05;
	needImmediateDDI = false;
	level->solver->calculate(&calculateParams);

	needRedisplay = 0;

	drawEyeViewSoftShadowed();

	if(wireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if(renderInfo && (!shotRequested || captureVideo))
	{
#ifdef CORNER_LOGO
		float w = 230/(float)winWidth;
		float h = 50/(float)winHeight;
		float x = 1-w;
		float y = 1-h;
		showOverlay(lightsprintMap,x,y,w,h);
#endif
		float now = demoPlayer->getPartPosition();
		float frameStart = 0;
		if(!demoPlayer->getPaused())
		{
			for(LevelSetup::Frames::const_iterator i = level->pilot.setup->frames.begin(); i!=level->pilot.setup->frames.end(); i++)
			{
				rr::RRBuffer* texture = (*i)->overlayMap;
				if(texture && now>=frameStart && now<frameStart+(*i)->overlaySeconds)
				{
					switch((*i)->overlayMode)
					{
						case 0:
							showOverlay(texture);
							break;
						case 1:
							float pos = (now-frameStart)/(*i)->overlaySeconds; //0..1
							//float rand01 = rand()/float(RAND_MAX);
							float intensity = (1-(pos*2-1)*(pos*2-1)*(pos*2-1)*(pos*2-1)) ;//* MAX(0,MIN(rand01*20,1)-rand01/10);
							float h = 0.13f+0.11f*pos;
							float w = h*texture->getWidth()*winHeight/winWidth/texture->getHeight();
							showOverlay(texture,intensity,0.5f-w/2,0.25f-h/2,w,h);
							break;
					}
				}
				frameStart += (*i)->transitionToNextTime;
			}
		}
		if(g_fps && !captureVideo)
		{
			g_fps->update();
			g_fps->render();
		}

		if(demoPlayer->getPaused() && level->animationEditor)
		{
			level->animationEditor->renderThumbnails(skyRenderer);
		}

		drawHelpMessage(showHelp);
	}

	if(shotRequested)
	{
		static unsigned shots = 0;
		shots++;
		char buf[100];
		//sprintf(buf,"Lightsprint3+1_%02d.png",shots);
		sprintf(buf,"video/frame%04d.jpg",shots);
		/*if(rr_gl::Texture::saveBackbuffer(buf))
			rr::RRReporter::report(rr::INF1,"Saved %s.\n",buf);
		else*/
			rr::RRReporter::report(rr::WARN,"Error: Failed to saved %s.\n",buf);
		shotRequested = 0;
	}

	glutSwapBuffers();

	//printf("cache: hits=%d misses=%d",rr::RRStaticSolver::getSceneStatistics()->numIrradianceCacheHits,rr::RRStaticSolver::getSceneStatistics()->numIrradianceCacheMisses);

	{
		// animaci nerozjede hned ale az po 2 snimcich (po 1 nestaci)
		// behem kresleni prvnich snimku driver kompiluje shadery nebo co, pauza by narusila fps
		static int framesDisplayed = 0;
		framesDisplayed++;
		if(framesDisplayed==2 && demoPlayer)
		{
			demoPlayer->setPaused(supportEditor);
		}
	}
}

void toggleWireFrame(void)
{
	wireFrame = !wireFrame;
	if (wireFrame) {
		glClearColor(0.1f,0.2f,0.2f,0);
	} else {
		glClearColor(0,0,0,0);
	}
	glutPostRedisplay();
}

void changeSpotlight()
{
	currentFrame.projectorIndex = (currentFrame.projectorIndex+1)%demoPlayer->getNumProjectors();
	//light.fieldOfView = 50+40.0*rand()/RAND_MAX;
	if(!level) return;
	level->solver->reportDirectIlluminationChange(0,false,true);
}

void reportEyeMovement()
{
	if(!level) return;
	needMatrixUpdate = 1;
	needRedisplay = 1;
	movingEye = 4;
}

void reportEyeMovementEnd()
{
	if(!level) return;
	movingEye = 0;
}

void reportLightMovement()
{
	if(!level) return;
	level->solver->reportDirectIlluminationChange(0,true,true);
	needMatrixUpdate = 1;
	needRedisplay = 1;
	movingLight = 3;
}

void reportLightMovementEnd()
{
	if(!level) return;
	movingLight = 0;
}

void reportObjectMovement()
{
	// shadowType 0 is static hard shadow, it doesn't need updates
	if(currentFrame.shadowType!=0)
		level->solver->reportDirectIlluminationChange(0,true,false);
}

float speedForward = 0;
float speedBack = 0;
float speedRight = 0;
float speedLeft = 0;
float speedUp = 0;
float speedDown = 0;
float speedLean = 0;

void setupSceneDynamicAccordingToCursor(Level* level)
{
	// novy kod: jsme paused, takze zobrazime co je pod kurzorem, neridime se casem
	// makame jen pokud vubec existuji framy (pokud neex, nechame kameru jak je)
	if(level->pilot.setup->frames.size())
	{
		// pokud je kurzor za koncem, vezmeme posledni frame
		unsigned existingFrameNumber = MIN(level->animationEditor->frameCursor,(unsigned)level->pilot.setup->frames.size()-1);
		AnimationFrame* frame = level->pilot.setup->getFrameByIndex(existingFrameNumber);
		if(frame)
			demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup, *frame, true);
		else
			RR_ASSERT(0); // tohle se nemelo stat
	}
	// stary kod: podle casu vybiral spatny frame v pripade ze framy mely 0sec
	//demoPlayer->getDynamicObjects()->setupSceneDynamicForPartTime(level->pilot.setup, demoPlayer->getPartPosition());
}

void special(int c, int x, int y)
{
	needImmediateDDI = true; // chceme okamzitou odezvu kdyz klavesa hne svetlem

	if(level 
		&& demoPlayer->getPaused()
		&& (x||y) // arrows simulated by w/s/a/d are not intended for editor
		&& level->animationEditor
		&& level->animationEditor->special(c,x,y))
	{
		// kdyz uz editor hne kurzorem, posunme se na frame i v demoplayeru
		demoPlayer->setPartPosition(level->animationEditor->getCursorTime());
		if(c!=GLUT_KEY_INSERT) // insert moves cursor right but preserves scene
		{
			setupSceneDynamicAccordingToCursor(level);
		}
		level->pilot.reportInteraction();
		needRedisplay = 1;
		return;
	}

	int modif = glutGetModifiers();
	float scale = 1;
	if(modif&GLUT_ACTIVE_SHIFT) scale=10;
	if(modif&GLUT_ACTIVE_CTRL) scale=3;
	if(modif&GLUT_ACTIVE_ALT) scale=0.1f;
	scale *= 3;

	switch (c) 
	{
		case GLUT_KEY_F1: showHelp = 1-showHelp; break;

		case GLUT_KEY_F2: currentFrame.shadowType = 1; break;
		case GLUT_KEY_F3: currentFrame.shadowType = 2; break;
		case GLUT_KEY_F4: currentFrame.shadowType = 3; if(modif&GLUT_ACTIVE_ALT) keyboard(27,0,0); break;

		case GLUT_KEY_F5: currentFrame.indirectType = 0; break;
		case GLUT_KEY_F6: currentFrame.indirectType = 1; break;
		case GLUT_KEY_F7: currentFrame.indirectType = 2; break;
#ifdef SUPPORT_LIGHTMAPS
		case GLUT_KEY_F8: currentFrame.indirectType = 3; break;
#endif

		case GLUT_KEY_F11:
			shotRequested = 1;
			break;

		case GLUT_KEY_UP:
			speedForward = scale;
			break;
		case GLUT_KEY_DOWN:
			speedBack = scale;
			break;
		case GLUT_KEY_LEFT:
			speedLeft = scale;
			break;
		case GLUT_KEY_RIGHT:
			speedRight = scale;
			break;
		case GLUT_KEY_PAGE_UP:
			speedUp = scale;
			break;
		case GLUT_KEY_PAGE_DOWN:
			speedDown = scale;
			break;

		default:
			return;
	}

	glutPostRedisplay();
}

void specialUp(int c, int x, int y)
{
	switch (c) 
	{
		case GLUT_KEY_UP:
			speedForward = 0;
			break;
		case GLUT_KEY_DOWN:
			speedBack = 0;
			break;
		case GLUT_KEY_LEFT:
			speedLeft = 0;
			break;
		case GLUT_KEY_RIGHT:
			speedRight = 0;
			break;
		case GLUT_KEY_PAGE_UP:
			speedUp = 0;
			break;
		case GLUT_KEY_PAGE_DOWN:
			speedDown = 0;
			break;
	}
}

void keyboardPlayerOnly(unsigned char c, int x, int y)
{
	switch (c)
	{
		case ' ':
		case 27:
			keyboard(c,x,y);
	}
}

void specialPlayerOnly(int c, int x, int y)
{
	switch (c)
	{
		case GLUT_KEY_F1:
		case GLUT_KEY_F11:
			special(c,x,y);
	}
}

void keyboard(unsigned char c, int x, int y)
{
	needImmediateDDI = true; // chceme okamzitou odezvu kdyz klavesa hne svetlem

#ifdef SUPPORT_LIGHTMAPS
	const char* cubeSideNames[6] = {"x+","x-","y+","y-","z+","z-"};
#endif

	if(level
		&& demoPlayer->getPaused()
		&& level->animationEditor
		&& level->animationEditor->keyboard(c,x,y))
	{
		// kdyz uz editor hne kurzorem, posunme se na frame i v demoplayeru
		demoPlayer->setPartPosition(level->animationEditor->getCursorTime());
		{
			setupSceneDynamicAccordingToCursor(level);
		}
		level->pilot.reportInteraction();
		needRedisplay = 1;
		return;
	}

	int modif = glutGetModifiers();
	float scale = 1;
	if(modif&GLUT_ACTIVE_SHIFT) scale=10;
	if(modif&GLUT_ACTIVE_CTRL) scale=3;
	if(modif&GLUT_ACTIVE_ALT) scale=0.1f;

	switch (c)
	{
		case 27:
			if(showHelp)
			{
				// first esc only closes help
				showHelp = 0;
				break;
			}

			rr::RRReporter::report(rr::INF1,"Escaped by user, benchmarking unfinished.\n");
			// rychlejsi ukonceni:
			//if(supportEditor) delete level; // aby se ulozily zmeny v animaci
			// pomalejsi ukonceni s uvolnenim pameti:
			delete demoPlayer;

			done_gl_resources();
			delete rr::RRReporter::getReporter();
			rr::RRReporter::setReporter(NULL);
			exiting = true;
			exit(30000);
			break;
		case 'a':
		case 'A':
			special(GLUT_KEY_LEFT,0,0);
			break;
		case 's':
		case 'S':
			special(GLUT_KEY_DOWN,0,0);
			break;
		case 'd':
		case 'D':
			special(GLUT_KEY_RIGHT,0,0);
			break;
		case 'w':
		case 'W':
			special(GLUT_KEY_UP,0,0);
			break;
		case 'q':
		case 'Q':
			special(GLUT_KEY_PAGE_UP,0,0);
			break;
		case 'z':
		case 'Z':
			special(GLUT_KEY_PAGE_DOWN,0,0);
			break;
		case 'x':
		case 'X':
			speedLean = -scale;
			break;
		case 'c':
		case 'C':
			speedLean = +scale;
			break;

		case ' ':
			demoPlayer->setPaused(!demoPlayer->getPaused());
			enableInteraction(demoPlayer->getPaused());
			break;

		case 13:
			if(!glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
			{
				fullscreen = !fullscreen;
				if(fullscreen)
					glutFullScreen();
				else
				{
					unsigned w = glutGet(GLUT_SCREEN_WIDTH);
					unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
					glutReshapeWindow(resolutionx,resolutiony);
					glutPositionWindow((w-resolutionx)/2,(h-resolutiony)/2);
				}
			}
			break;

		case '[': realtimeLight->areaSize /= 1.2f; level->solver->reportDirectIlluminationChange(0,true,true); printf("%f\n",realtimeLight->areaSize); break;
		case ']': realtimeLight->areaSize *= 1.2f; level->solver->reportDirectIlluminationChange(0,true,true); break;			

		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			{
				rr::RRRay* ray = rr::RRRay::create();
				rr::RRVec3 dir = rr::RRVec3(currentFrame.eye.dir[0],currentFrame.eye.dir[1],currentFrame.eye.dir[2]).normalized();
				ray->rayOrigin = rr::RRVec3(currentFrame.eye.pos[0],currentFrame.eye.pos[1],currentFrame.eye.pos[2]);
				ray->rayDirInv[0] = 1/dir[0];
				ray->rayDirInv[1] = 1/dir[1];
				ray->rayDirInv[2] = 1/dir[2];
				ray->rayLengthMin = 0;
				ray->rayLengthMax = 1000;
				ray->rayFlags = rr::RRRay::FILL_POINT3D;
				// kdyz neni kolize se scenou, najit kolizi s vodou
				if(!level->solver->getMultiObjectCustom()->getCollider()->intersect(ray))
				{
					float cameraLevel = currentFrame.eye.pos[1];
					float waterLevel = level->pilot.setup->waterLevel;
					float levelChangeIn1mDistance = dir[1];
					float distance = levelChangeIn1mDistance ? (waterLevel-cameraLevel)/levelChangeIn1mDistance : 10;
					if(distance<0) distance=10;
					ray->hitPoint3d = ray->rayOrigin+dir*distance;
				}
				// keys 1/2/3... index one of few sceneobjects
				unsigned selectedObject_indexInScene = c-'1';
				if(selectedObject_indexInScene<level->pilot.setup->objects.size())
				{
					// we have more dynaobjects
					selectedObject_indexInDemo = level->pilot.setup->objects[selectedObject_indexInScene];
					if(!modif)
					{
						if(demoPlayer->getDynamicObjects()->getPos(selectedObject_indexInDemo)==ray->hitPoint3d)
							// hide (move to 0)
							demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,rr::RRVec3(0));
						else
							// move to given position
							demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,ray->hitPoint3d);//+rr::RRVec3(0,1.2f,0));
					}
				}
				level->solver->reportDirectIlluminationChange(0,true,false);
			}
			break;

#define CHANGE_ROT(dY,dZ) demoPlayer->getDynamicObjects()->setRot(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getRot(selectedObject_indexInDemo)+rr::RRVec2(dY,dZ))
#define CHANGE_POS(dX,dY,dZ) demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getPos(selectedObject_indexInDemo)+rr::RRVec3(dX,dY,dZ))
		case 'j': CHANGE_ROT(-5,0); break;
		case 'k': CHANGE_ROT(+5,0); break;
		case 'u': CHANGE_ROT(0,-5); break;
		case 'i': CHANGE_ROT(0,+5); break;
		case 'f': CHANGE_POS(-0.05f,0,0); break;
		case 'h': CHANGE_POS(+0.05f,0,0); break;
		case 'v': CHANGE_POS(0,-0.05f,0); break;
		case 'r': CHANGE_POS(0,+0.05f,0); break;
		case 'g': CHANGE_POS(0,0,-0.05f); break;
		case 't': CHANGE_POS(0,0,+0.05f); break;

		case 'm':
			changeSpotlight();
			break;

		case '+':
			for(unsigned i=0;i<4;i++) currentFrame.brightness[i] *= 1.2f;
			break;
		case '-':
			for(unsigned i=0;i<4;i++) currentFrame.brightness[i] /= 1.2f;
			break;
		case '*':
			currentFrame.gamma *= 1.2f;
			break;
		case '/':
			currentFrame.gamma /= 1.2f;
			break;

		case 'b':
			bigscreenSimulator = !bigscreenSimulator;
			bigscreenCompensation = !bigscreenCompensation;
			demoPlayer->setBigscreen(bigscreenCompensation);
			break;
		case 'B':
			bigscreenCompensation = !bigscreenCompensation;
			demoPlayer->setBigscreen(bigscreenCompensation);
			break;

			/*
		case 'f':
		case 'F':
			showLightViewFrustum = !showLightViewFrustum;
			if (showLightViewFrustum) needMatrixUpdate = 1;
			break;
		case 'a':
			++realtimeLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		case 'S':
			level->solver->reportDirectIlluminationChange(0,true,true);
			break;
		case 'w':
		case 'W':
			toggleWireFrame();
			return;
		case 'n':
			light.anear *= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'N':
			light.anear /= 0.8;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'c':
			light.afar *= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'C':
			light.afar /= 1.2;
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 't':
			uberProgramGlobalSetup.MATERIAL_DIFFUSE_VCOLOR = !uberProgramGlobalSetup.MATERIAL_DIFFUSE_VCOLOR;
			uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = !uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP;
			break;*/
		default:
			return;
	}
	glutPostRedisplay();
}

void keyboardUp(unsigned char c, int x, int y)
{
	switch(c)
	{
		case 'a':
		case 'A':
			specialUp(GLUT_KEY_LEFT,0,0);
			break;
		case 's':
		case 'S':
			specialUp(GLUT_KEY_DOWN,0,0);
			break;
		case 'd':
		case 'D':
			specialUp(GLUT_KEY_RIGHT,0,0);
			break;
		case 'w':
		case 'W':
			specialUp(GLUT_KEY_UP,0,0);
			break;
		case 'q':
		case 'Q':
			specialUp(GLUT_KEY_PAGE_UP,0,0);
			break;
		case 'z':
		case 'Z':
			specialUp(GLUT_KEY_PAGE_DOWN,0,0);
			break;
		case 'x':
		case 'X':
		case 'c':
		case 'C':
			speedLean = 0;
			break;
	}
}

enum
{
	ME_SCENE_VIEWER,
	ME_TOGGLE_VIDEO,
	ME_TOGGLE_WATER,
	ME_TOGGLE_INFO,
	ME_UPDATE_LIGHTMAPS_0,
	ME_UPDATE_LIGHTMAPS_0_ENV,
	ME_SAVE_LIGHTMAPS_0,
	ME_LOAD_LIGHTMAPS_0,
	ME_UPDATE_LIGHTMAPS_ALL,
	ME_SAVE_LIGHTMAPS_ALL,
	ME_LOAD_LIGHTMAPS_ALL,
	ME_PREVIOUS_SCENE,
	ME_NEXT_SCENE,
	ME_TOGGLE_HELP,
};

void mainMenu(int item)
{
	switch (item)
	{
		case ME_SCENE_VIEWER:
			rr_gl::sceneViewer(level->solver,false,"shaders/",NULL);
			break;
		case ME_TOGGLE_VIDEO:
			captureVideo = !captureVideo;
			break;
#ifdef SUPPORT_WATER
		case ME_TOGGLE_WATER:
			level->pilot.setup->renderWater = !level->pilot.setup->renderWater;
			break;
#endif
		case ME_TOGGLE_INFO:
			renderInfo = !renderInfo;
			break;

		case ME_TOGGLE_HELP:
			showHelp = 1-showHelp;
			break;

#ifdef SUPPORT_LIGHTMAPS
		todo: create buffers for computed lightmaps
		case ME_UPDATE_LIGHTMAPS_0_ENV:
			{
				// set lights
				//rr::RRLights lights;
				//lights.push_back(rr::RRLight::createPointLight(rr::RRVec3(1,1,1),rr::RRVec3(0.5f))); //!!! not freed
				//lights.push_back(rr::RRLight::createDirectionalLight(rr::RRVec3(2,-5,1),rr::RRVec3(0.7f))); //!!! not freed
				//level->solver->setLights(lights);
				// updates maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect(LIGHTMAP_QUALITY);
				rr::RRDynamicSolver::UpdateParameters paramsIndirect(LIGHTMAP_QUALITY/4);

				// update all objects
				level->solver->updateLightmaps(0,-1,true,&paramsDirect,&paramsIndirect);

				// stop updating maps in realtime, stay with what we computed here
				modeMovingEye = true;
				currentFrame.indirectType = 3;
			}
			break;

		case ME_UPDATE_LIGHTMAPS_0:
			{
				// updates maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect();
				paramsDirect.quality = LIGHTMAP_QUALITY;

				// update 1 object
				static unsigned obj=0;
				if(level->solver->getIllumination(obj))
				{
					if(!level->solver->getIllumination(obj)->getLayer(0)->pixelBuffer)
						level->solver->getIllumination(obj)->getLayer(0)->pixelBuffer = ((rr_gl::RRDynamicSolverGL*)(level->solver))->createIlluminationPixelBuffer(512*2,512*2);
					level->solver->updateLightmap(obj,level->solver->getIllumination(obj)->getLayer(0)->pixelBuffer,NULL,&paramsDirect);
				}
				//

				// stop updating maps in realtime, stay with what we computed here
				modeMovingEye = true;
				currentFrame.indirectType = 3;
			}
			break;

		case ME_UPDATE_LIGHTMAPS_ALL:
			{
				// updates all maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect();
				paramsDirect.quality = LIGHTMAP_QUALITY;

				for(LevelSetup::Frames::const_iterator i=level->pilot.setup->frames.begin();i!=level->pilot.setup->frames.end();i++)
				{
					demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup,**i,true);
					printf("(");
					for(unsigned j=0;j<10+LIGHTMAP_QUALITY/10;j++)
						level->solver->calculate();
					printf(")");
					unsigned layerNumber = (*i)->layerNumber;
					// update all vbufs
					level->solver->updateLightmaps(layerNumber,-1,NULL,NULL);
					// update 1 lmap
					static unsigned obj=12;
					if(!level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer)
						level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer = ((rr_gl::RRDynamicSolverGL*)(level->solver))->createIlluminationPixelBuffer(512,512);
					level->solver->updateLightmap(obj,level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer,NULL,&paramsDirect);
				}

				// stop updating maps in realtime, stay with what we computed here
				modeMovingEye = true;
				currentFrame.indirectType = 3;
			}
			break;

		case ME_SAVE_LIGHTMAPS_0:
			// save current illumination maps
			{
				static unsigned captureIndex = 0;
				char filename[100];
				// save all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<level->solver->getNumObjects();objectIndex++)
				{
					rr::RRBuffer* map = level->solver->getIllumination(objectIndex)->getLayer(0)->pixelBuffer;
					if(map)
					{
						sprintf(filename,"export/cap%02d_statobj%d.png",captureIndex,objectIndex);
						bool saved = map->save(filename);
						printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
					}
				}
				/*/ save all environment maps (dynamic objects)
				for(unsigned objectIndex=0;objectIndex<DYNAOBJECTS;objectIndex++)
				{
					if(dynaobjects->getObject(objectIndex)->diffuseMap)
					{
						sprintf(filename,"export/cap%02d_dynobj%d_diff_%cs.png",captureIndex,objectIndex,'%');
						bool saved = dynaobjects->getObject(objectIndex)->diffuseMap->save(filename,cubeSideNames);
						printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
					}
					if(dynaobjects->getObject(objectIndex)->specularMap)
					{
						sprintf(filename,"export/cap%02d_dynobj%d_spec_%cs.png",captureIndex,objectIndex,'%');
						bool saved = dynaobjects->getObject(objectIndex)->specularMap->save(filename,cubeSideNames);
						printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
					}
				}*/
				captureIndex++;
				break;
			}

		case ME_LOAD_LIGHTMAPS_0:
			// load illumination from disk
			{
				unsigned captureIndex = 0;
				char filename[200];
				// load all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<level->solver->getNumObjects();objectIndex++)
				{
					sprintf(filename,"export/cap%02d_statobj%d.png",captureIndex,objectIndex);
					rr::RRBuffer* loaded = rr::RRBuffer::load(filename);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete level->solver->getIllumination(objectIndex)->getLayer(0);
						level->solver->getIllumination(objectIndex)->getLayer(0) = loaded;
					}
				}
				/*/ load all environment maps (dynamic objects)
				for(unsigned objectIndex=0;objectIndex<DYNAOBJECTS;objectIndex++)
				{
					// diffuse
					sprintf(filename,"export/cap%02d_dynobj%d_diff_%cs.png",captureIndex,objectIndex,'%');
					rr::RRBuffer* loaded = rr::RRBuffer::load(filename,cubeSideNames);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete dynaobjects->getObject(objectIndex)->diffuseMap;
						dynaobjects->getObject(objectIndex)->diffuseMap = loaded;
					}
					// specular
					sprintf(filename,"export/cap%02d_dynobj%d_spec_%cs.png",captureIndex,objectIndex,'%');
					loaded = rr::RRBuffer::load(filename,cubeSideNames);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete dynaobjects->getObject(objectIndex)->specularMap;
						dynaobjects->getObject(objectIndex)->specularMap = loaded;
					}
				}*/
				modeMovingEye = true;
				needLightmapCacheUpdate = true;
				// start rendering ambient maps instead of vertex colors, so loaded maps get visible
				currentFrame.indirectType = 3;
				break;
			}

		case ME_SAVE_LIGHTMAPS_ALL:
			// save all illumination maps
			if(level) rr::RRReporter::report(rr::INF1,"Saved %d buffers.\n",level->saveIllumination("export/",true,true));
			break;

		case ME_LOAD_LIGHTMAPS_ALL:
			// load all illumination from disk
			if(level) rr::RRReporter::report(rr::INF1,"Loaded %d buffers.\n",level->loadIllumination("export/",true,true));
			break;
#endif // SUPPORT_LIGHTMAPS

		//case ME_PREVIOUS_SCENE:
		//	demoPlayer->setPart();
		//	break;

		case ME_NEXT_SCENE:
			if(supportEditor)
				level->pilot.setup->save();
			//delete level;
			level = NULL;
			seekInMusicAtSceneSwap = true;
			break;
	}
	glutWarpPointer(winWidth/2,winHeight/2);
	glutPostRedisplay();
}

void initMenu()
{
	int menu = glutCreateMenu(mainMenu);
#ifdef SUPPORT_WATER
	glutAddMenuEntry("Toggle water",ME_TOGGLE_WATER);
#endif
	glutAddMenuEntry("Toggle info",ME_TOGGLE_INFO);
	glutAddMenuEntry("Debugger",ME_SCENE_VIEWER);
#ifdef SUPPORT_LIGHTMAPS
	glutAddMenuEntry("Lightmaps update(rt light)", ME_UPDATE_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps update(env+lights)", ME_UPDATE_LIGHTMAPS_0_ENV);
	glutAddMenuEntry("Lightmaps save current", ME_SAVE_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps load current", ME_LOAD_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps update(rt light) all frames", ME_UPDATE_LIGHTMAPS_ALL);
	glutAddMenuEntry("Lightmaps save all frames", ME_SAVE_LIGHTMAPS_ALL);
	glutAddMenuEntry("Lightmaps load all frames", ME_LOAD_LIGHTMAPS_ALL);
#endif
	glutAddMenuEntry("Scene previous", ME_PREVIOUS_SCENE);
	glutAddMenuEntry("Scene next", ME_NEXT_SCENE);
	glutAddMenuEntry("Toggle help",ME_TOGGLE_HELP);
	glutAddMenuEntry("Toggle video capture",ME_TOGGLE_VIDEO);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void reshape(int w, int h)
{
	// we are called with desktop resolution in vista from exit(), glViewport crashes in HD2400 driver
	if(exiting) return;

	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	needMatrixUpdate = 1;

	if(!demoPlayer)
	{
		demoPlayer = new DemoPlayer(cfgFile,supportEditor,supportMusic,supportEditor,preciseTimer);
		demoPlayer->setBigscreen(bigscreenCompensation);
		//demoPlayer->setPaused(supportEditor); unpaused after first display()
		//glutSwapBuffers();
	}
}

void mouse(int button, int state, int x, int y)
{
	if(level && state==GLUT_DOWN) level->pilot.reportInteraction();
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		modeMovingEye = !modeMovingEye;
	}
#ifdef GLUT_WITH_WHEEL_AND_LOOP
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(currentFrame.eye.fieldOfView>13) currentFrame.eye.fieldOfView -= 10;
		needMatrixUpdate = 1;
		needRedisplay = 1;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(currentFrame.eye.fieldOfView<130) currentFrame.eye.fieldOfView+=10;
		needMatrixUpdate = 1;
		needRedisplay = 1;
	}
#endif
}

void passive(int x, int y)
{
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(level && (x || y)) level->pilot.reportInteraction();
	if(x || y)
	{
#if defined(LINUX) || defined(linux)
		const float mouseSensitivity = 0.0002f;
#else
		const float mouseSensitivity = 0.005f;
#endif
		if(modeMovingEye)
		{
			currentFrame.eye.angle -= mouseSensitivity*x;
			currentFrame.eye.angleX -= mouseSensitivity*y;
			CLAMP(currentFrame.eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
			reportEyeMovement();
		}
		else
		{
			currentFrame.light.angle -= mouseSensitivity*x;
			currentFrame.light.angleX -= mouseSensitivity*y;
			CLAMP(currentFrame.light.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
			// changes also position a bit, together with rotation
			currentFrame.light.pos += currentFrame.light.dir*0.3f;
			currentFrame.light.update();
			currentFrame.light.pos -= currentFrame.light.dir*0.3f;
			reportLightMovement();
			needImmediateDDI = true; // chceme okamzitou odezvu pri rucnim hybani svetlem
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	REPORT(rr::RRReportInterval report(rr::INF3,"idle()\n"));
	if(!winWidth) return; // can't work without window

	if(level && level->pilot.isTimeToChangeLevel())
	{
		//showImage(loadingMap);
		//delete level;
		level = NULL;
		seekInMusicAtSceneSwap = false;
	}

	if(!level)
	{
		//		showImage(loadingMap);
		//		showImage(loadingMap); // neznamo proc jeden show nekdy nestaci na spravny uvodni obrazek
		//delete level;
		level = demoPlayer->getNextPart(seekInMusicAtSceneSwap,supportEditor);
		needMatrixUpdate = 1;
		needRedisplay = 1;

		// end of the demo
		if(!level)
		{
			rr::RRReporter::report(rr::INF1,"Finished, average fps = %.2f.\n",g_fps?g_fps->getAvg():0);
			exiting = true;
			exit(g_fps ? (unsigned)(g_fps->getAvg()*10) : 0);
			//keyboard(27,0,0);
		}

		// implant our light into solver
		level->solver->realtimeLights.push_back(realtimeLight);

		//for(unsigned i=0;i<6;i++)
		//	level->solver->calculate();

		// capture thumbnails
		if(supportEditor)
		{
			rr::RRReportInterval report(rr::INF1,"Updating thumbnails...\n");
			for(LevelSetup::Frames::iterator i=level->pilot.setup->frames.begin();i!=level->pilot.setup->frames.end();i++)
			{
				updateThumbnail(**i);
			}
		}
	}

	// 1. move movables
	// 2. calculate (optionally update all depthmaps)
	// 3. render (when possible, reuse existing depthmaps)
	// avoid: 
	// 1. calculate updates depthmaps
	// 2. movables move
	// 3. display updates depthmaps again

	// keyboard movements with key repeat turned off
	static TIME prev = 0;
	TIME now = GETTIME;
	float seconds = (now-prev)/(float)PER_SEC;//timer.Watch();
	if(!prev || now==prev) seconds = 0;
	CLAMP(seconds,0.001f,0.3f);
	rr_gl::Camera* cam = modeMovingEye?&currentFrame.eye:&currentFrame.light;
	if(speedForward) cam->moveForward(speedForward*seconds);
	if(speedBack) cam->moveBack(speedBack*seconds);
	if(speedRight) cam->moveRight(speedRight*seconds);
	if(speedLeft) cam->moveLeft(speedLeft*seconds);
	if(speedUp) cam->moveUp(speedUp*seconds);
	if(speedDown) cam->moveDown(speedDown*seconds);
	if(speedLean) cam->lean(speedLean*seconds);
	if(speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
	{
		//printf(" %f ",seconds);
		if(cam==&currentFrame.light) reportLightMovement(); else reportEyeMovement();
	}
	if(!demoPlayer->getPaused())
	{
		if(captureVideo)
		{
			// advance by 1/30
			demoPlayer->advanceBy(1/30.f);
			shotRequested = 1;
		}
		else
		{
			// advance according to real time
			demoPlayer->advance();
		}
		if(level)
		{
			// najde aktualni frame
			const AnimationFrame* frame = level->pilot.setup ? level->pilot.setup->getFrameByTime(demoPlayer->getPartPosition()) : NULL;
			if(frame)
			{
				// pokud existuje, nastavi ho
				demoPlayer->setVolume(frame->volume);
				static AnimationFrame prevFrame(0);
				bool objMoved = demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup,*frame,memcmp(&frame->light,&prevFrame.light,sizeof(rr_gl::Camera))!=0);
				if(objMoved)
					reportObjectMovement();
				for(unsigned i=0;i<10;i++)
				{
					// vsem objektum nastavi animacni cas (ten je pak konstantni pro shadowmapy i final render)
					DynamicObject* dynobj = demoPlayer->getDynamicObjects()->getObject(i);
					if(dynobj) dynobj->animationTime = demoPlayer->getPartPosition();
				}
				if(frame->layerNumber!=prevFrame.layerNumber) needImmediateDDI = true; // chceme okamzitou odezvu pri strihu					
				prevFrame = *frame;
			}
			else
			{
				// pokud neexistuje, jde na dalsi level nebo skonci 
				if(level->animationEditor)
				{
					// play scene finished, jump to editor
					demoPlayer->setPaused(true);
					enableInteraction(true);
					level->animationEditor->frameCursor = MAX(1,(unsigned)level->pilot.setup->frames.size())-1;
				}
				else
				{
					// play scene finished, jump to next scene
					//delete level;
					level = NULL;
					seekInMusicAtSceneSwap = false;
				}
			}
		}
	}
	else
	{
		// paused
		if(!supportEditor)
		{
			float secondsSincePrevFrame = demoPlayer->advance();
			demoPlayer->getDynamicObjects()->advanceRot(secondsSincePrevFrame);
			//if(object on screen)
				reportObjectMovement();
			needRedisplay = 1;
		}
	}
	prev = now;
	setShadowTechnique();

	if(movingEye && !--movingEye)
	{
		reportEyeMovementEnd();
	}
	if(movingLight && !--movingLight)
	{
		reportLightMovementEnd();
	}
	if(!demoPlayer->getPaused())
	{
		needRedisplay = 1;
	}
	glutPostRedisplay();
}

void enableInteraction(bool enable)
{
	if(enable)
	{
		glutSpecialFunc(special);
		glutSpecialUpFunc(specialUp);
		glutMouseFunc(mouse);
		glutPassiveMotionFunc(passive);
		glutKeyboardFunc(keyboard);
		glutKeyboardUpFunc(keyboardUp);
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	else
	{
		glutSpecialFunc(specialPlayerOnly);
		glutSpecialUpFunc(NULL);
		glutMouseFunc(NULL);
		glutPassiveMotionFunc(NULL);
		glutKeyboardFunc(keyboardPlayerOnly);
		glutKeyboardUpFunc(NULL);
	}
}

void init_gl_states()
{
	//GLint depthBits;
	//glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	//printf("backbuffer depth precision = %d\n", depthBits);

	//GLint samplers1=0,samplers2=0;
	//glGetIntegerv(GL_MAX_TEXTURE_UNITS, &samplers1);
	//glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &samplers2);
	//printf("GPU limits: samplers=%d / %d\n",samplers1,samplers2);

	glClearColor(0,0,0,0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	/* GL_LEQUAL ensures that when fragments with equal depth are
	generated within a single rendering pass, the last fragment
	results. */
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	glLineStipple(1, 0xf0f0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	glFrontFace(GL_CCW);

#if defined(_WIN32)
	if(wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif
}

void parseOptions(int argc, const char*const*argv)
{
	int i,tmp;
	bool badArgument = false;

	for(i=1; i<argc; i++)
	{
		if(strstr(argv[i],".cfg"))
		{
			cfgFile = argv[i];
		}
		else
		if(!strcmp("editor", argv[i]))
		{
			supportEditor = 1;
			fullscreen = 0;
			showTimingInfo = 1;
		}
		else
		if(!strcmp("bigscreen", argv[i]))
		{
			bigscreenCompensation = 1;
		}
		else
		if(sscanf(argv[i],"%dx%d",&resolutionx,&resolutiony)==2)
		{
		}
		else
		if(!strcmp("window", argv[i]))
		{
			fullscreen = 0;
		}
		else
		if(!strcmp("fullscreen", argv[i]))
		{
			fullscreen = 1;
		}
		else
		if(!strcmp("stability=low", argv[i]))
		{
			lightStability = rr_gl::RRDynamicSolverGL::DDI_4X4;
		}
		else
		if(!strcmp("stability=auto", argv[i]))
		{
			lightStability = rr_gl::RRDynamicSolverGL::DDI_AUTO;
		}
		else
		if(!strcmp("stability=high", argv[i]))
		{
			lightStability = rr_gl::RRDynamicSolverGL::DDI_8X8;
		}
		else
		if(!strcmp("silent", argv[i]))
		{
			supportMusic = false;
		}
		else
		if(!strcmp("timer_precision=high", argv[i]))
		{
			preciseTimer = true;
		}
		else
		if(sscanf(argv[i],"penumbra%d", &tmp)==1)
		{
			// handled elsewhere
		}
		else
		{
			printf("Unknown commandline argument: %s\n",argv[i]);
			badArgument = true;
		}
	}
	if(badArgument)
	{
		printf("\nLightsmark 2008 back-end                                       (C) Stepan Hrbek\n");
		printf("\nUsage: backend.exe [arg1] [arg2] ...\n");
		printf("\nArguments:\n");
		printf("  window                    - run in window (default is fullscreen)\n");
		printf("  640x480                   - run in given resolution (default is 1280x1024)\n");
		printf("  silent                    - run without music (default si music)\n");
		printf("  bigscreen                 - boost brightness\n");
		printf("  stability=[low|auto|high] - set lighting stability (default is auto)\n");
		printf("  penumbra[1|2|3|4|5|6|7|8] - set penumbra precision (default is auto)\n");
		printf("  editor                    - run editor (default is benchmark)\n");
		printf("  filename.cfg              - run custom content (default is Lightsmark2008.cfg)\n");
		exit(0);
	}
}

#ifdef SET_ICON
HANDLE hIcon = 0;
#endif

int main(int argc, char **argv)
{
	//_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 1154356;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}

	// data are in ../../data
	// solution: change dir [1] rather than expect that caller calls us from data [2]
	// why?
	// WINE can't emulate [2]
	// GPU PerfStudio can't [2]
	_chdir("../../data");

	parseOptions(argc, argv);

#ifdef CONSOLE
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
#else
	rr::RRReporter::setReporter(rr::RRReporter::createFileReporter("../log.txt",false));
#endif
	rr::RRReporter::setFilter(true,2,false);
	REPORT(rr::RRReporter::setFilter(true,3,true));
	//rr_gl::Program::showLog = true;
	rr::RRReporter::report(rr::INF2,"This is Lightsmark 2008 [%dbit] log. Check it if benchmark doesn't work properly.\n",sizeof(void*)*8);
#ifdef _WIN32
	rr::RRReporter::report(rr::INF2,"Started: %s\n",GetCommandLine());
#endif

	rr_io::setImageLoader();

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_ALPHA); // | GLUT_ACCUM | GLUT_ALPHA accum na high quality soft shadows, alpha na filtrovani ambient map
	if(fullscreen)
	{
		char buf[100];
		sprintf(buf,"%dx%d:32",resolutionx,resolutiony);
		glutGameModeString(buf);
		glutEnterGameMode();
		glutSetWindowTitle(PRODUCT_NAME);
	}
	else
	{
		unsigned w = glutGet(GLUT_SCREEN_WIDTH);
		unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
		glutInitWindowSize(resolutionx,resolutiony);
		glutInitWindowPosition((w-resolutionx)/2,(h-resolutiony)/2);
		glutCreateWindow(PRODUCT_NAME);
		if(resolutionx==w && resolutiony==h)
			glutFullScreen();
		if(supportEditor)
			initMenu();
	}
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);
	if(glutGet(GLUT_WINDOW_WIDTH)!=resolutionx || glutGet(GLUT_WINDOW_HEIGHT)!=resolutiony)
	{
		rr::RRReporter::report(rr::ERRO,"Sorry, unable to set %dx%d%s, try different mode.\n",resolutionx,resolutiony,fullscreen?" fullscreen":"");
		exiting = true;
		exit(0);
	};

	// init GLEW
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("Graphics card driver doesn't support OpenGL 2.0.\n",true);
	init_gl_states();
	const char* vendor = (const char*)glGetString(GL_VENDOR);
	const char* renderer = (const char*)glGetString(GL_RENDERER);

	updateMatrices(); // needed for startup without area lights (realtimeLight doesn't update matrices for 1 instance)

	uberProgramGlobalSetup.SHADOW_MAPS = 1;
	uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
	uberProgramGlobalSetup.SHADOW_PENUMBRA = 1;
	uberProgramGlobalSetup.LIGHT_DIRECT = 1;
	uberProgramGlobalSetup.LIGHT_DIRECT_MAP = 1;
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = 1;
	uberProgramGlobalSetup.LIGHT_INDIRECT_DETAIL_MAP = 1;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE = 1;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = 1;
	uberProgramGlobalSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = 1;
	uberProgramGlobalSetup.POSTPROCESS_BRIGHTNESS = 1;

	// init shaders
	// init textures
	init_gl_resources();

	// adjust INSTANCES_PER_PASS to GPU
#ifdef SUPPORT_LIGHTMAPS
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = 0;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = 1;
#endif
	INSTANCES_PER_PASS = uberProgramGlobalSetup.detectMaxShadowmaps(uberProgram,argc,argv);
#ifdef SUPPORT_LIGHTMAPS
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = currentFrame.wantsVertexColors();
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = currentFrame.wantsLightmaps();
#endif
	if(!INSTANCES_PER_PASS) error("",true);
	realtimeLight->setNumInstances(startWithSoftShadows?INSTANCES_PER_PASS:1);

	if(rr::RRLicense::loadLicense("licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.",false);

#ifdef BACKGROUND_WORKER
#ifdef _OPENMP
	if(omp_get_max_threads()>1)
#endif
		g_backgroundWorker = new BackgroundWorker;
#endif

#ifdef SET_ICON
	HWND hWnd = FindWindowA(NULL,PRODUCT_NAME);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
#endif

	enableInteraction(supportEditor);

	glutMainLoop();
	return 0;
}

#ifndef CONSOLE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	int argc;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	char** argv = new char*[argc+1];
	for(int i=0;i<argc;i++)
	{
		argv[i] = (char*)malloc(wcslen(argvw[i])+1);
		sprintf(argv[i], "%ws", argvw[i]);
	}
	argv[argc] = NULL;
#ifdef SET_ICON
	hIcon = LoadImage(hInstance,MAKEINTRESOURCE(IDI_ICON1),IMAGE_ICON,0,0,0);
#endif
	return main(argc,argv);
}
#endif
