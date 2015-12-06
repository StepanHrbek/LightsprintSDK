// pred releasem
// - vypnout SUPPORT_COLLADA/MGF/OBJ
// - prepnout FACTOR_FORMAT na 0
#define MAX_INSTANCES              10  // max number of light instances aproximating one area light
unsigned INSTANCES_PER_PASS;
#define SHADOW_MAP_SIZE_SOFT       512
#define SHADOW_MAP_SIZE_HARD       2048
#define GI_UPDATE_QUALITY          5 // default is 3, increase to 5 fixes book in first 5 seconds
#define GI_UPDATE_INTERVAL         -1 // start GI update as often as possible (but can be affected by MAX_LIGHT_UPDATE_FREQUENCY)
#define BACKGROUND_THREAD            // run improve+updateLightmaps+UpdateEnvironmentMap asynchronously, on background
#define DDI_EVERY_FRAME            1 // 1=slower but equal frame times, 0=faster but unequal frame times
#if defined(NDEBUG) && defined(_WIN32)
	//#define SET_ICON
#else
	#define CONSOLE
#endif
//#define SUPPORT_RR_ED // builds sceneViewer() in
//#define CORNER_LOGO
//#define PRODUCT_NAME "3+1"
//#define CFG_FILE "3+1.cfg"
//#define CFG_FILE "LightsprintDemo.cfg"
//#define CFG_FILE "Candella.cfg"
#define PRODUCT_NAME "Lightsmark 2015"
#define CFG_FILE "Lightsmark.cfg"
//#define CFG_FILE "mgf.cfg"
//#define CFG_FILE "test.cfg"
//#define CFG_FILE "eg-flat1.cfg"
//#define CFG_FILE "eg-quake.cfg"
//#define CFG_FILE "eg-sponza.cfg"
//#define CFG_FILE "eg-sponza-sun.cfg"
//#define CFG_FILE "Lowpoly.cfg"
int fullscreen = 1;
int resolutionx = 1280;
int resolutiony = 1024;
bool resolutionSet = false; // false = not set from cmdline, use default 1280x1024 and fallback to 1024x768
bool supportEditor = 0;
bool bigscreenCompensation = 0;
bool bigscreenSimulator = 0;
bool showTimingInfo = 0;
const char* captureVideo = 0;
float splitscreen = 0.0f; // 0=disabled, 0.5=leva pulka obrazovky ma konst.ambient
bool supportMusic = 1;
bool alphashadows = 1; // 0=opaque shadows, bad sun's shadow, 1=alpha keyed, fixes sun, danger:driver might optimize lightIndirectConstColor away
/*
!kamera nebo svetlo jsou mirne posunute, chyba je nejlip videt v prvnim snimku lightsmarku
 myslel jsem ze to zpusobila rev 2319 kdy jsem zrusil RealtimeLight bez originu, ale chyba byla pritomna uz nejmin 10 revizi driv

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
#include "Level.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <list>
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#ifdef __APPLE__
	#include <GLUT/glut.h>
	#include <ApplicationServices/ApplicationServices.h>
#else
	#ifdef _WIN32
		#include <GL/wglew.h>
	#endif
	#include <GL/glut.h>
#endif
#ifdef _WIN32
	#include <direct.h> // _chdir
	#include <shellapi.h> // CommandLineToArgvW
#else
	#include <unistd.h> // chdir
	#define _chdir chdir
#endif
//#include "Lightsprint/GL/PluginBloom.h"
//#include "Lightsprint/GL/PluginDOF.h"
#include "Lightsprint/GL/PluginFPS.h"
//#include "Lightsprint/GL/PluginLensFlare.h"
//#include "Lightsprint/GL/PluginPanorama.h"
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PluginSky.h"
//#include "Lightsprint/GL/PluginSSGI.h"
//#include "Lightsprint/GL/PluginStereo.h"
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#ifdef SUPPORT_RR_ED
	#include "Lightsprint/Ed/Ed.h"
#endif
#include "Lightsprint/IO/IO.h"
#include "AnimationEditor.h"
#include "DemoPlayer.h"
#include "DynamicObjects.h"
#include "../LightsprintCore/RRSolver/report.h"
#ifdef RR_DEVELOPMENT
	#include "../../src/LightsprintCore/RRStaticSolver/rrcore.h"
#endif
#include "resource.h"
#include <thread>
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

/////////////////////////////////////////////////////////////////////////////
//
// globals

rr::RRReporter* reporter;
AnimationFrame currentFrame(0);
GLUquadricObj *quadric;
rr::RRLight* rrLight = nullptr; // allocated/deleted once per program run, used by all levels
rr_gl::RealtimeLight* realtimeLight = nullptr; // never allocated/deleted, just shortcut for level->solver->realtimeLights[0]
#ifdef CORNER_LOGO
	rr_gl::Texture* lightsprintMap = nullptr; // small logo in the corner
#endif
rr_gl::Program* ambientProgram; // it had color controlled via glColor, but because of catalyst 8.11 bug, we switched to uniform lightIndirectConst
rr_gl::TextureRenderer* skyRenderer;
rr_gl::UberProgram* uberProgram;
rr_gl::UberProgramSetup uberProgramGlobalSetup;
int winWidth = 0;
int winHeight = 0;
bool needLightmapCacheUpdate = false;
int wireFrame = 0;
int showHelp = 0; // 0=none, 1=help
int showLightViewFrustum = 0;
bool modeMovingEye = 0;
unsigned movingEye = 0;
unsigned movingLight = 0;
bool needRedisplay = 0;
bool needReportEyeChange = 0;
bool needReportLightChange = 0;
bool needImmediateDDI = 1; // nastaveno pri strihu, pri rucnim pohybu svetlem mysi nebo klavesnici
bool gameOn = 0;
Level* level = nullptr;
bool seekInMusicAtSceneSwap = false;
//class DynamicObjects* dynaobjects;
bool shotRequested;
DemoPlayer* demoPlayer = nullptr;
unsigned selectedObject_indexInDemo = 0;
bool renderInfo = 1;
const char* cfgFile = CFG_FILE;
rr_gl::RRSolverGL::DDIQuality lightStability = rr_gl::RRSolverGL::DDI_AUTO;
char globalOutputDirectory[1000] = "."; // without trailing slash
const char* customScene = nullptr;
#ifdef BACKGROUND_THREAD
	std::thread backgroundThread;
	enum ThreadState { TS_NONE, TS_RUNNING, TS_FINISHED };
	ThreadState backgroundThreadState = TS_NONE;
#endif


/////////////////////////////////////////////////////////////////////////////
//
// Fps

rr_gl::FpsCounter  g_fpsCounter;
unsigned           g_playedFrames = 0;
float              g_fpsAvg = 0;


/////////////////////////////////////////////////////////////////////////////

bool exiting = false;

void stopBackgroundThread()
{
#ifdef BACKGROUND_THREAD
	if (backgroundThreadState!=TS_NONE)
	{
		backgroundThread.join();
		backgroundThreadState = TS_NONE;
	}
#endif
}

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if (gfxRelated)
		rr::RRReporter::report(rr::INF1,"\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported GPUs:\n - those that support OpenGL 2.0 or OpenGL ES 2.0 or higher\n - i.e. nearly all from ATI/AMD since 2002, NVIDIA since 2003, Intel since 2006");
	if (glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
		glutLeaveGameMode();
	else
		glutDestroyWindow(glutGetWindow());
#ifdef CONSOLE
	printf("\n\nPress ENTER to close.");
	fgetc(stdin);
#endif
	exiting = true;
	stopBackgroundThread();
	exit(0);
}

// sets our globals and rendering pipeline according to currentFrame.shadowType
void setShadowTechnique()
{
	// early exit
	static unsigned oldShadowType = 999;
	if (currentFrame.shadowType == oldShadowType) return;
	oldShadowType = currentFrame.shadowType;

	// cheap changes (no GL commands)
	realtimeLight->numInstancesInArea = (currentFrame.shadowType<3)?1:INSTANCES_PER_PASS;

	// expensive changes (GL commands)
	if (currentFrame.shadowType>=2)
	{
		realtimeLight->setShadowmapSize((resolutionx<800)?SHADOW_MAP_SIZE_SOFT/2:SHADOW_MAP_SIZE_SOFT);
		realtimeLight->setNumShadowSamples(4);
	}
	else
	{
		realtimeLight->setShadowmapSize((resolutionx<800)?SHADOW_MAP_SIZE_HARD/2:SHADOW_MAP_SIZE_HARD);
		realtimeLight->setNumShadowSamples(1);
	}
	level->solver->reportDirectIlluminationChange(0,true,false,false);
}

void init_gl_resources()
{
	quadric = gluNewQuadric();

	rrLight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(1),rr::RRVec3(1),rr::RRVec3(1),RR_DEG2RAD(40),0.1f);

#ifdef CORNER_LOGO
	lightsprintMap = rr_gl::Texture::load("maps/Lightsprint230.png", nullptr, false, false, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
#endif

	uberProgram = rr_gl::UberProgram::create("shaders/ubershader.vs", "shaders/ubershader.fs");
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_CONST = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = true;
	uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = true;
	ambientProgram = uberProgram->getProgram(uberProgramSetup.getSetupString());

	skyRenderer = new rr_gl::TextureRenderer("shaders/");

	if (!ambientProgram)
		error("\nFailed to compile or link GLSL program.\n",true);
	else
	{
		ambientProgram->useIt();
		ambientProgram->sendUniform("lightIndirectConst",rr::RRVec4(1));
	}
}

void done_gl_resources()
{
	rr::RR_SAFE_DELETE(skyRenderer);
	rr::RR_SAFE_DELETE(uberProgram);
	rr_gl::deleteAllTextures();
#ifdef CORNER_LOGO
	rr::RR_SAFE_DELETE(lightsprintMap);
#endif
	if (rrLight)
		rrLight->projectedTexture = nullptr;
	rr::RR_SAFE_DELETE(rrLight);
	gluDeleteQuadric(quadric);
}



/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects


const rr::RRCollider* getSceneCollider()
{
	if (!level) return nullptr;
	return level->solver->getMultiObject()->getCollider();
}


/////////////////////////////////////////////////////////////////////////////

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void drawLight(void)
{
	ambientProgram->useIt();
	glPushMatrix();
	glTranslatef(currentFrame.light.getPosition()[0],currentFrame.light.getPosition()[1],currentFrame.light.getPosition()[2]);
	ambientProgram->sendUniform("materialDiffuseConst",rr::RRVec4(1.0f,1.0f,0.0f,1.0f));
	gluSphere(quadric, 0.05f, 10, 10);
	glPopMatrix();
}

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance, rr::RRCamera& camera, const rr::RRLight* renderingFromThisLight)
{
	if (!level) return;

	RR_ASSERT(!uberProgramSetup.OBJECT_SPACE); 
	RR_ASSERT(demoPlayer);

	camera.setAspect( winHeight ? (float) winWidth / (float) winHeight : 1 );

	rr_gl::PluginParamsSky ppSky(nullptr,level->solver,1);
	rr_gl::PluginParamsScene ppScene(&ppSky,level->solver);
	ppScene.uberProgramSetup = uberProgramSetup;
	ppScene.renderingFromThisLight = renderingFromThisLight;
#ifdef BACKGROUND_THREAD
	ppScene.updateLayerLightmap = needImmediateDDI;
	ppScene.updateLayerEnvironment = needImmediateDDI;
#else
	ppScene.updateLayerLightmap = true;
	ppScene.updateLayerEnvironment = true;
#endif
	ppScene.layerLightmap = LAYER_LIGHTMAPS;
	ppScene.layerEnvironment = LAYER_ENVIRONMENT;
	ppScene.layerLDM = uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP ? level->getLDMLayer() : UINT_MAX;
	rr_gl::PluginParamsFPS ppFPS(&ppScene,g_fpsCounter,!true);
	rr_gl::PluginParamsShared ppShared;
	ppShared.camera = &camera;
	ppShared.viewport[2] = winWidth;
	ppShared.viewport[3] = winHeight;
	ppShared.brightness = currentFrame.brightness;
	ppShared.gamma = currentFrame.gamma;
	demoPlayer->getBoost(ppShared.brightness,ppShared.gamma);
	rrLight->projectedTexture = demoPlayer->getProjector(currentFrame.projectorIndex);

	level->solver->getRenderer()->render(captureVideo?(rr_gl::PluginParams*)&ppScene:(rr_gl::PluginParams*)&ppFPS,ppShared);
}

void drawEyeViewShadowed(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	if (!level) return;

	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	uberProgramSetup.POSTPROCESS_BIGSCREEN = bigscreenSimulator;

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	renderScene(uberProgramSetup,firstInstance,currentFrame.eye,nullptr);

	if (supportEditor)
		drawLight();
	if (showLightViewFrustum)
	{
		level->solver->renderLights(currentFrame.eye);
	}
}

void drawEyeViewSoftShadowed(void)
{
	unsigned numInstances = realtimeLight->getNumShadowmaps();
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	RR_ASSERT(numInstances<=INSTANCES_PER_PASS);

		glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
		if (splitscreen)
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(0,0,(unsigned)(winWidth*splitscreen),winHeight);
			rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 1;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = false;
			uberProgramSetup.FORCE_2D_POSITION = false;
			drawEyeViewShadowed(uberProgramSetup,0);
			glScissor((unsigned)(winWidth*splitscreen),0,winWidth-(unsigned)(winWidth*splitscreen),winHeight);
		}

		if (numInstances>1)
		{
			// Z only pre-pass before expensive penumbra shadows
			// optional but improves fps from 60 to 80
			rr_gl::UberProgramSetup uberProgramSetup;

			// (enable alpha keying - fixes missing transparency of sun with penumbra shadow)
			uberProgramSetup.LIGHT_INDIRECT_CONST = 1; // to prevent optimizing texture access away
			uberProgramSetup.MATERIAL_DIFFUSE = 1;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = 1;
			uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = 1;

			renderScene(uberProgramSetup,0,currentFrame.eye,nullptr);
		}

		// render everything
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = numInstances;
		uberProgramSetup.SHADOW_PENUMBRA = true;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_INDIRECT_CONST = currentFrame.wantsConstantAmbient();
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR =
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = currentFrame.wantsVertexColors();
		uberProgramSetup.LIGHT_INDIRECT_MAP = currentFrame.wantsLightmaps();
		uberProgramSetup.LIGHT_INDIRECT_DETAIL_MAP &= !currentFrame.wantsConstantAmbient();
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true; // for robot

		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,0);

		if (splitscreen)
			glDisable(GL_SCISSOR_TEST);
}

// captures current scene into thumbnail
void updateThumbnail(AnimationFrame& frame)
{
	//rr::RRReporter::report(rr::INF1,"Updating thumbnail.\n");
	// set frame
	demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->setup,frame,true);
	// calculate
	level->solver->calculate();
	// render into thumbnail
	if (!frame.thumbnail)
		frame.thumbnail = rr::RRBuffer::create(rr::BT_2D_TEXTURE,160,120,1,rr::BF_RGB,true,nullptr);
	glViewport(0,0,160,120);
	currentFrame.eye = frame.eye; // while rendering, we call setupForRender(currentFrame.eye);
	drawEyeViewSoftShadowed();
	rr_gl::readPixelsToBuffer(frame.thumbnail);
		//unsigned char* pixels = frame.thumbnail->lock(rr::BL_DISCARD_AND_WRITE);
		//glReadPixels(0,0,160,120,GL_RGB,GL_UNSIGNED_BYTE,pixels);
		//frame.thumbnail->unlock();
	rr_gl::getTexture(frame.thumbnail,true,true); // false,true made render of thumbnails terribly slow on X300
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

static void output(int x, int y, const char* string)
{
	glRasterPos2i(x, y);
	int len = (int) strlen(string);
	for (int i = 0; i < len; i++)
	{
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, string[i]);
	}
//	glCallLists(strlen(string), GL_UNSIGNED_BYTE, string); 
}

static void drawHelpMessage(int screen)
{
	if (shotRequested) return;
//	if (!big && gameOn) return;

/* misto glutu pouzije truetype fonty z windows
	static bool fontInited = false;
	if (!fontInited)
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
		glRecti(RR_MIN(winWidth-30,500), 30, 30, RR_MIN(winHeight-30,100));
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

	static const char* message[3][30] = 
	{
		{
		"H - help",
		nullptr
		},
		{
		PRODUCT_NAME ", (C) Stepan Hrbek",
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
		" F5,F6,F7      - ambient: none/const/realtimeradiosity",
		" F11           - save screenshot",
		" wheel         - zoom",
		" x,c           - lean",
		" +,-,*,/       - brightness/contrast",
		" 1,2,3,4...    - move n-th object",
		" enter         - maximize window",
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
		nullptr,
		}
/*
		,{
		"Works of following people were used in Lightsprint Demo:",
		"",
		"  - Stepan Hrbek, Daniel Sykora  : realtime global illumination",
		"  - Mark Kilgard, Nate Robins    : GLUT library",
		"  - Milan Ikits, Marcelo Magallon: GLEW library",
		"  - many contributors            : FreeImage library",
		"  - libav and FFmpeg authors     : FFmpeg library",
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
		nullptr
		}
*/
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
	ambientProgram->sendUniform("materialDiffuseConst",rr::RRVec4(0.0f,0.0f,0.0f,0.6f));

	// Drawn clockwise because the flipped Y axis flips CCW and CW.
	if (screen /*|| demoPlayer->getPaused()*/)
	{
		unsigned rectWidth = 530;
		unsigned rectHeight = 360;
		ambientProgram->sendUniform("materialDiffuseConst",rr::RRVec4(0.0f,0.1f,0.3f,0.6f));
		glRecti((winWidth+rectWidth)/2, (winHeight-rectHeight)/2, (winWidth-rectWidth)/2, (winHeight+rectHeight)/2);
		glDisable(GL_BLEND);
		ambientProgram->sendUniform("materialDiffuseConst",rr::RRVec4(1.0f,1.0f,1.0f,1.0f));
		int x = (winWidth-rectWidth)/2+20;
		int y = (winHeight-rectHeight)/2+30;
		for (i=0; message[screen][i] != nullptr; i++) 
		{
			if (message[screen][i][0] != '\0')
			{
				output(x, y, message[screen][i]);
			}
			y += 18;
		}
	}
	else
	if (showTimingInfo)
	{
		int x = 40, y = 50;
		glRecti(RR_MIN(winWidth-30,500), 30, 30, RR_MIN(winHeight-30,100));
		glDisable(GL_BLEND);
		ambientProgram->sendUniform("materialDiffuseConst",rr::RRVec4(1.0f,1.0f,1.0f,1.0f));
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
		if (demoPlayer->getPaused())
		{
			// paused: frame index = editor cursor
			frameIndex = level->animationEditor->frameCursor;
			transitionDone = 0;
			AnimationFrame* frame = level->setup->getFrameByIndex(frameIndex); // muze byt nullptr (kurzor za koncem)
			transitionTotal = frame ? frame->transitionToNextTime : 0;
		}
		else
		{
			// playing: frame index computed from current time
			frameIndex = level->setup->getFrameIndexByTime(demoPlayer->getPartPosition(),&transitionDone,&transitionTotal);
		}
		const AnimationFrame* frame = level->setup->getFrameByIndex(frameIndex+1);
		sprintf(buf,"scene %d/%d, frame %d(%d)/%d, %.1f/%.1fs",
			demoPlayer->getPartIndex()+1,demoPlayer->getNumParts(),
			frameIndex+1,frame?frame->layerNumber:0,(int)level->setup->frames.size(),
			transitionDone,transitionTotal);
		output(x,y+18,buf);
		sprintf(buf,"bright %.1f, gamma %.1f",
			currentFrame.brightness[0],currentFrame.gamma);
		output(x,y+36,buf);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
}

void showImage(const rr_gl::Texture* tex)
{
	if (!tex) return;
	skyRenderer->render2D(tex,nullptr,0,0,1,1);
	glutSwapBuffers();
}

void showOverlay(const rr::RRBuffer* tex)
{
	if (!tex) return;
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	rr_gl::ToneParameters tp;
	tp.color = rr::RRVec4(currentFrame.brightness,1);
	skyRenderer->render2D(rr_gl::getTexture(tex,false,false),&tp,0,0,1,1);
	glDisable(GL_BLEND);
}

void showOverlay(const rr::RRBuffer* logo,float intensity,float x,float y,float w,float h)
{
	if (!logo) return;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	rr_gl::ToneParameters tp;
	tp.color = rr::RRVec4(intensity);
	skyRenderer->render2D(rr_gl::getTexture(logo,true,false),&tp,x,y,w,h);
	glDisable(GL_BLEND);
}

/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

// prototypes
void keyboard(unsigned char c, int x, int y);
void enableInteraction(bool enable);

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
	if (!level) return;
	level->solver->reportDirectIlluminationChange(0,false,true,false);
}

void reportEyeMovement()
{
	if (!level) return;
	needRedisplay = 1;
	movingEye = 4;
}

void reportEyeMovementEnd()
{
	if (!level) return;
	movingEye = 0;
}

void reportLightMovement()
{
	if (!level) return;
	level->solver->reportDirectIlluminationChange(0,true,true,false);
	needRedisplay = 1;
	movingLight = 3;
}

void reportLightMovementEnd()
{
	if (!level) return;
	movingLight = 0;
}

void reportObjectMovement()
{
	// shadowType 0 is static hard shadow, it doesn't need updates
	if (currentFrame.shadowType!=0)
		level->solver->reportDirectIlluminationChange(0,true,false,false);
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
	if (level->setup->frames.size())
	{
		// pokud je kurzor za koncem, vezmeme posledni frame
		unsigned existingFrameNumber = RR_MIN(level->animationEditor->frameCursor,(unsigned)level->setup->frames.size()-1);
		AnimationFrame* frame = level->setup->getFrameByIndex(existingFrameNumber);
		if (frame)
			demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->setup, *frame, true);
		else
			RR_ASSERT(0); // tohle se nemelo stat
	}
	// stary kod: podle casu vybiral spatny frame v pripade ze framy mely 0sec
	//demoPlayer->getDynamicObjects()->setupSceneDynamicForPartTime(level->setup, demoPlayer->getPartPosition());
}

void special(int c, int x, int y)
{
	needImmediateDDI = true; // chceme okamzitou odezvu kdyz klavesa hne svetlem

	if (level 
		&& demoPlayer->getPaused()
		&& (x||y) // arrows simulated by w/s/a/d are not intended for editor
		&& level->animationEditor
		&& level->animationEditor->special(c,x,y))
	{
		// kdyz uz editor hne kurzorem, posunme se na frame i v demoplayeru
		demoPlayer->setPartPosition(level->animationEditor->getCursorTime());
		if (c!=GLUT_KEY_INSERT) // insert moves cursor right but preserves scene
		{
			setupSceneDynamicAccordingToCursor(level);
		}
		needRedisplay = 1;
		return;
	}

	int modif = glutGetModifiers();
	float scale = 1;
	if (modif&GLUT_ACTIVE_SHIFT) scale=10;
	if (modif&GLUT_ACTIVE_CTRL) scale=3;
	if (modif&GLUT_ACTIVE_ALT) scale=0.1f;
	scale *= 3;

	switch (c) 
	{
		case GLUT_KEY_F1: showHelp = 1-showHelp; break;

		case GLUT_KEY_F2: currentFrame.shadowType = 1; break;
		case GLUT_KEY_F3: currentFrame.shadowType = 2; break;
		case GLUT_KEY_F4: currentFrame.shadowType = 3; if (modif&GLUT_ACTIVE_ALT) keyboard(27,0,0); break;

		case GLUT_KEY_F5: currentFrame.indirectType = 0; break;
		case GLUT_KEY_F6: currentFrame.indirectType = 1; break;
		case GLUT_KEY_F7: currentFrame.indirectType = 2; break;

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

	if (level
		&& demoPlayer->getPaused()
		&& level->animationEditor
		&& level->animationEditor->keyboard(c,x,y))
	{
		// kdyz uz editor hne kurzorem, posunme se na frame i v demoplayeru
		demoPlayer->setPartPosition(level->animationEditor->getCursorTime());
		{
			setupSceneDynamicAccordingToCursor(level);
		}
		needRedisplay = 1;
		return;
	}

	int modif = glutGetModifiers();
	float scale = 1;
	if (modif&GLUT_ACTIVE_SHIFT) scale=10;
	if (modif&GLUT_ACTIVE_CTRL) scale=3;
	if (modif&GLUT_ACTIVE_ALT) scale=0.1f;

	switch (c)
	{
		case 27:
			if (showHelp)
			{
				// first esc only closes help
				showHelp = 0;
				break;
			}

			rr::RRReporter::report(rr::INF1,supportEditor ? "Quitting editor.\n" : "Escaped by user, benchmarking unfinished.\n");
			stopBackgroundThread();
			// rychlejsi ukonceni:
			//if (supportEditor) delete level; // aby se ulozily zmeny v animaci
			// pomalejsi ukonceni s uvolnenim pameti:
			delete demoPlayer;

			done_gl_resources();
			delete reporter;
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
			if (!glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
			{
				fullscreen = !fullscreen;
				if (fullscreen)
				{
/*
 Workaround for glut bug "broken fullscreen in linux"

 Use glutFullScreen() instead of game mode,
  and set the _NET_WM_STATE_FULLSCREEN atom in the Window properties
  XInternAtom(xdisplay, "_NET_WM_STATE_FULLSCREEN", True);
 
 The "stock" glut uses _MOTIF_WM_HINTS to 
 set the application fullscreen.  Most modern window managers ignore the 
 MOTIF hints and use the NET_WM settings instead.
 Freeglut already has already talked about making the move, but the stock 
 glut doesn't seem to have been updated.

 What should be done: fix glut, don't touch Lightsmark
*/
					glutFullScreen();
				}
				else
				{
					unsigned w = glutGet(GLUT_SCREEN_WIDTH);
					unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
					glutReshapeWindow(resolutionx,resolutiony);
					glutPositionWindow((w-resolutionx)/2,(h-resolutiony)/2);
				}
			}
			break;

		case '[': realtimeLight->areaSize /= 1.2f; level->solver->reportDirectIlluminationChange(0,true,true,false); printf("%f\n",realtimeLight->areaSize); break;
		case ']': realtimeLight->areaSize *= 1.2f; level->solver->reportDirectIlluminationChange(0,true,true,false); break;

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
				rr::RRRay ray;
				rr::RRVec3 dir = currentFrame.eye.getDirection();
				ray.rayOrigin = currentFrame.eye.getPosition();
				ray.rayDir = dir;
				ray.rayLengthMin = 0;
				ray.rayLengthMax = 1000;
				ray.rayFlags = rr::RRRay::FILL_POINT3D;
				ray.hitObject = level->solver->getMultiObject();
				// kdyz neni kolize se scenou, umistit 10m daleko
				if (!ray.hitObject->getCollider()->intersect(ray))
				{
					ray.hitPoint3d = ray.rayOrigin+dir*10;
				}
				// keys 1/2/3... index one of few sceneobjects
				unsigned selectedObject_indexInScene = c-'1';
				if (selectedObject_indexInScene<level->setup->objects.size())
				{
					// we have more dynaobjects
					selectedObject_indexInDemo = level->setup->objects[selectedObject_indexInScene];
					if (!modif)
					{
						if (demoPlayer->getDynamicObjects()->getPos(selectedObject_indexInDemo)==ray.hitPoint3d)
							// hide (move to 0)
							demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,rr::RRVec3(0));
						else
							// move to given position
							demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,ray.hitPoint3d);//+rr::RRVec3(0,1.2f,0));
					}
				}
				level->solver->reportDirectIlluminationChange(0,true,false,false);
			}
			break;

#define CHANGE_ROT(dY,dZ) demoPlayer->getDynamicObjects()->setRot(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getRot(selectedObject_indexInDemo)+rr::RRVec2(dY,dZ))
#define CHANGE_POS(dX,dY,dZ) demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getPos(selectedObject_indexInDemo)+rr::RRVec3(dX,dY,dZ))
//		case 'j': CHANGE_ROT(-5,0); break;
//		case 'k': CHANGE_ROT(+5,0); break;
//		case 'u': CHANGE_ROT(0,-5); break;
//		case 'i': CHANGE_ROT(0,+5); break;
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
			for (unsigned i=0;i<4;i++) currentFrame.brightness[i] *= 1.2f;
			break;
		case '-':
			for (unsigned i=0;i<4;i++) currentFrame.brightness[i] /= 1.2f;
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
			break;
		case 'a':
			++realtimeLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		case 'S':
			level->solver->reportDirectIlluminationChange(0,true,true,false);
			break;
		case 'w':
		case 'W':
			toggleWireFrame();
			return;
		case 'n':
			light.anear *= 0.8;
			needDepthMapUpdate = 1;
			break;
		case 'N':
			light.anear /= 0.8;
			needDepthMapUpdate = 1;
			break;
		case 'c':
			light.afar *= 1.2;
			needDepthMapUpdate = 1;
			break;
		case 'C':
			light.afar /= 1.2;
			needDepthMapUpdate = 1;
			break;*/
		case 'l':
			uberProgramGlobalSetup.LIGHT_INDIRECT_DETAIL_MAP = !uberProgramGlobalSetup.LIGHT_INDIRECT_DETAIL_MAP;
			break;
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
#ifdef SUPPORT_RR_ED
	ME_SCENE_VIEWER,
#endif
	ME_TOGGLE_VIDEO,
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
#ifdef SUPPORT_RR_ED
		case ME_SCENE_VIEWER:
			rr_ed::sceneViewer(level->solver,"","","",nullptr,true);
			break;
#endif
		case ME_TOGGLE_VIDEO:
			captureVideo = captureVideo ? nullptr : "jpg";
			break;
		case ME_TOGGLE_INFO:
			renderInfo = !renderInfo;
			break;

		case ME_TOGGLE_HELP:
			showHelp = 1-showHelp;
			break;

		//case ME_PREVIOUS_SCENE:
		//	demoPlayer->setPart();
		//	break;

		case ME_NEXT_SCENE:
			if (supportEditor)
				level->setup->save();
			stopBackgroundThread();
			//delete level;
			level = nullptr;
			seekInMusicAtSceneSwap = true;
			break;
	}
	glutWarpPointer(winWidth/2,winHeight/2);
	glutPostRedisplay();
}

void initMenu()
{
	int menu = glutCreateMenu(mainMenu);
	glutAddMenuEntry("Toggle info panel",ME_TOGGLE_INFO);
#ifdef SUPPORT_RR_ED
	glutAddMenuEntry("Debugger",ME_SCENE_VIEWER);
#endif
	glutAddMenuEntry("Scene previous", ME_PREVIOUS_SCENE);
	glutAddMenuEntry("Scene next", ME_NEXT_SCENE);
	glutAddMenuEntry("Toggle help",ME_TOGGLE_HELP);
	glutAddMenuEntry("Toggle video capture while playing",ME_TOGGLE_VIDEO);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void reshape(int w, int h)
{
	// we are called with desktop resolution in vista from exit(), glViewport crashes in HD2400 driver
	if (exiting) return;

	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);

	if (!demoPlayer)
	{
		demoPlayer = new DemoPlayer(cfgFile,supportEditor,supportMusic,supportEditor);
		demoPlayer->setBigscreen(bigscreenCompensation);
		//demoPlayer->setPaused(supportEditor); unpaused after first display()
		//glutSwapBuffers();
	}
}

void mouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		modeMovingEye = !modeMovingEye;
	}
#ifdef GLUT_WITH_WHEEL_AND_LOOP
	float fov = currentFrame.eye.getFieldOfViewVerticalDeg();
	if (button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if (fov>13) fov -= 10; else fov /= 1.4f;
		needRedisplay = 1;
	}
	if (button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if (fov*1.4f<=3) fov *= 1.4f; else if (fov<130) fov += 10;
		needRedisplay = 1;
	}
	currentFrame.eye.setFieldOfViewVerticalDeg(fov);
#endif
}

void passive(int x, int y)
{
	if (!winWidth || !winHeight) return;
	RR_LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if (x || y)
	{
#if defined(LINUX) || defined(linux)
		const float mouseSensitivity = 0.0002f;
#else
		const float mouseSensitivity = 0.005f;
#endif
		rr::RRCamera& cam = modeMovingEye ? currentFrame.eye : currentFrame.light;
		rr::RRVec3 yawPitchRollRad = cam.getYawPitchRollRad()-rr::RRVec3(x,y,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		cam.setYawPitchRollRad(yawPitchRollRad);
		if (modeMovingEye)
			reportEyeMovement();
		else
		{
			reportLightMovement();
			needImmediateDDI = true; // chceme okamzitou odezvu pri rucnim hybani svetlem
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	glutPostRedisplay();
}

void display()
{
	REPORT(rr::RRReportInterval report(rr::INF3,"display()\n"));
	if (!winWidth) return; // can't work without window

	if (!level)
	{
no_level:
		stopBackgroundThread();
		level = demoPlayer->getNextPart(seekInMusicAtSceneSwap,supportEditor);
		needRedisplay = 1;

		// end of the demo
		if (!level)
		{
			rr::RRReporter::report(rr::INF1,"Finished, average fps = %.2f.\n",g_fpsAvg);
			stopBackgroundThread();
			exiting = true;
			exit((unsigned)(g_fpsAvg*10));
			//keyboard(27,0,0);
		}

		// implant our light into solver
		rr::RRLights lights;
		lights.push_back(rrLight);
		level->solver->setLights(lights);
		realtimeLight = level->solver->realtimeLights[0];
		realtimeLight->setCamera(&currentFrame.light);
		realtimeLight->numInstancesInArea = 1;
		realtimeLight->setShadowmapSize((resolutionx<800)?SHADOW_MAP_SIZE_SOFT/2:SHADOW_MAP_SIZE_SOFT);
		realtimeLight->shadowTransparencyRequested = alphashadows
			? rr_gl::RealtimeLight::ALPHA_KEYED_SHADOWS // cweb_m01drk.tga is blended, but it is well hidden in scene, it is not worth enabling colored shadows mode
			: rr_gl::RealtimeLight::FULLY_OPAQUE_SHADOWS; // disables alpha keying in shadows (to stay compatible with Lightsmark 2007)

		//for (unsigned i=0;i<6;i++)
		//	level->solver->calculate();

		// capture thumbnails
		if (supportEditor)
		{
			rr::RRReportInterval report(rr::INF1,"Updating thumbnails...\n");
			for (LevelSetup::Frames::iterator i=level->setup->frames.begin();i!=level->setup->frames.end();i++)
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
	static rr::RRTime previousFrameStartTime;
	float previousFrameDuration = previousFrameStartTime.secondsSinceLastQuery();
	RR_CLAMP(previousFrameDuration,0.0001f,0.3f);
	rr::RRCamera* cam = modeMovingEye?&currentFrame.eye:&currentFrame.light;
	if (speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
	{
		//printf(" %f ",seconds);
		cam->setPosition(cam->getPosition()
			+ cam->getDirection() * ((speedForward-speedBack)*previousFrameDuration)
			+ cam->getRight() * ((speedRight-speedLeft)*previousFrameDuration)
			+ cam->getUp() * ((speedUp-speedDown)*previousFrameDuration)
			);
		cam->setYawPitchRollRad(cam->getYawPitchRollRad()+rr::RRVec3(0,0,speedLean*previousFrameDuration));
		if (cam==&currentFrame.light) reportLightMovement(); else reportEyeMovement();
	}
	if (!demoPlayer->getPaused())
	{
		if (captureVideo)
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

		// find current frame
		const AnimationFrame* frame = level->setup ? level->setup->getFrameByTime(demoPlayer->getPartPosition()) : nullptr;
		unsigned frameIndex = level->setup->getFrameIndexByTime(demoPlayer->getPartPosition(),nullptr,nullptr);
		if (frame)
		{
			// if it exists, use it
			static AnimationFrame prevFrame(0);
			static unsigned prevFrameIndex(0);
			bool animationCut = frameIndex>prevFrameIndex+1; // if we skip frame, there was probably animation cut (cut requires frame with duration 0)
			if (animationCut && !frame->wantsConstantAmbient()) needImmediateDDI = true; // after cut, GI should be updated without delays
			demoPlayer->setVolume(frame->volume);
			bool lightChanged = memcmp(&frame->light,&prevFrame.light,sizeof(rr::RRCamera))!=0;
			bool objMoved = demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->setup,*frame,lightChanged);
			if (objMoved)
				reportObjectMovement();
			prevFrame = *frame;
			prevFrameIndex = frameIndex;
		}
		else
		{
			// if it does not exist, go to next level or terminate
no_frame:
			if (level->animationEditor)
			{
				// play scene finished, jump to editor
				demoPlayer->setPaused(true);
				enableInteraction(true);
				level->animationEditor->frameCursor = RR_MAX(1,(unsigned)level->setup->frames.size())-1;
			}
			else
			{
				// play scene finished, jump to next scene
				stopBackgroundThread();
				//delete level;
				level = nullptr;
				seekInMusicAtSceneSwap = false;
				goto no_level;
			}
		}
	}
	else
	{
		// paused
		if (!supportEditor)
		{
//			float secondsSincePrevFrame = demoPlayer->advance();
//			demoPlayer->getDynamicObjects()->advanceRot(secondsSincePrevFrame);
			//if (object on screen)
//				reportObjectMovement();
//			needRedisplay = 1;
		}
	}
	setShadowTechnique();

	if (movingEye && !--movingEye)
	{
		reportEyeMovementEnd();
	}
	if (movingLight && !--movingLight)
	{
		reportLightMovementEnd();
	}
	if (!demoPlayer->getPaused())
	{
		needRedisplay = 1;
	}

	// pro jednoduchost je to tady
	// kdyby to bylo u vsech stisku/pusteni/pohybu klaves/mysi a animaci,
	//  nevolalo by se to zbytecne v situaci kdy redisplay vyvola calculate() hlaskou ze zlepsil osvetleni
	// zisk by ale byl miniaturni
	level->solver->reportInteraction();

	rr::RRSolver::CalculateParameters calculateParams = level->setup->calculateParams;
	calculateParams.lightMultiplier = 2;
	calculateParams.secondsBetweenDDI = GI_UPDATE_INTERVAL;
	calculateParams.qualityIndirectStatic = calculateParams.qualityIndirectDynamic = currentFrame.wantsConstantAmbient() ? 0 : GI_UPDATE_QUALITY; // [#53] limits work in no radiosity mode
#ifdef BACKGROUND_THREAD
	if (!needImmediateDDI)
	{
		calculateParams.skipRRSolver = true;
		calculateParams.delayDDI = 3;
	}
	else
	{
		realtimeLight->dirtyGI = true;
	}
	if (backgroundThreadState==TS_FINISHED || (backgroundThreadState==TS_RUNNING && needImmediateDDI))
	{
		backgroundThread.join();
		backgroundThreadState = TS_NONE;
	}
#if !DDI_EVERY_FRAME
	if (backgroundThreadState!=TS_NONE)
	{
		// [#53] if we can't start new background thread, skip DDI and all CPU work, update only shadowmaps
		calculateParams.qualityIndirectDynamic = 0;
	}
#endif
	bool requestingDDI = calculateParams.qualityIndirectDynamic && realtimeLight->dirtyGI;
#endif
	level->solver->calculate(&calculateParams);
#ifdef BACKGROUND_THREAD
	// early exit if quality=0
	// [#53] used in "no radiosity" part of Lightsmark
	if (backgroundThreadState==TS_NONE && requestingDDI && !needImmediateDDI)
	{
		RR_ASSERT(backgroundThreadState==TS_NONE);
		backgroundThreadState = TS_RUNNING;
		backgroundThread = std::thread([calculateParams]()
		{
			// asynchronously update indirect illumination
			// 1. update it in solver
			level->solver->RRSolver::calculate(&calculateParams);
			// 2. update it in lightmaps/envmaps (we can let renderer do this synchronously, but this asynchronous update is cheaper)
			level->solver->updateLightmaps(LAYER_LIGHTMAPS,-1,-1,NULL,NULL);
			for (unsigned i=0;i<level->solver->getDynamicObjects().size();i++)
			{
				rr::RRObject* object = level->solver->getDynamicObjects()[i];
				if (object->enabled)
					level->solver->RRSolver::updateEnvironmentMap(&object->illumination,LAYER_ENVIRONMENT,-1,LAYER_LIGHTMAPS);
			}
			backgroundThreadState = TS_FINISHED;
		});
	}
#endif
	needRedisplay = 0;

	drawEyeViewSoftShadowed();

	needImmediateDDI = false; // don't clear before rendering, renderer would not update vbufs+env

	if (wireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (renderInfo && (!shotRequested || captureVideo))
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
		if (!demoPlayer->getPaused())
		{
			for (LevelSetup::Frames::const_iterator i = level->setup->frames.begin(); i!=level->setup->frames.end(); i++)
			{
				rr::RRBuffer* texture = (*i)->overlayMap;
				if (texture && now>=frameStart && now<frameStart+(*i)->overlaySeconds)
				{
					switch((*i)->overlayMode)
					{
						case 0:
							showOverlay(texture);
							break;
						case 1:
							float pos = (now-frameStart)/(*i)->overlaySeconds; //0..1
							//float rand01 = rand()/float(RAND_MAX);
							float intensity = (1-(pos*2-1)*(pos*2-1)*(pos*2-1)*(pos*2-1)) ;//* RR_MAX(0,RR_MIN(rand01*20,1)-rand01/10);
							float h = 0.13f+0.11f*pos;
							float w = h*texture->getWidth()*winHeight/winWidth/texture->getHeight();
							showOverlay(texture,intensity,0.5f-w/2,0.25f-h/2,w,h);
							break;
					}
				}
				frameStart += (*i)->transitionToNextTime;
			}
		}

		// fps
		if (!demoPlayer->getPaused()) g_playedFrames++;
		float seconds = demoPlayer->getDemoPosition();
		g_fpsAvg = g_playedFrames/RR_MAX(0.01f,seconds);

		if (demoPlayer->getPaused() && level->animationEditor)
		{
			level->animationEditor->renderThumbnails(skyRenderer);
		}

		drawHelpMessage(showHelp);
	}

	if (shotRequested)
	{
		static unsigned manualShots = 0;
		static unsigned videoShots = 0;
		char buf[1000];
		if (captureVideo)
			sprintf(buf,"%s/frame%04d.%s",globalOutputDirectory,++videoShots,captureVideo);
		else
			sprintf(buf,"%s/Lightsmark_%02d.png",globalOutputDirectory,++manualShots);
		rr::RRBuffer* sshot = rr::RRBuffer::create(rr::BT_2D_TEXTURE,winWidth,winHeight,1,rr::BF_RGB,true,nullptr);
		glReadBuffer(GL_BACK);
		rr_gl::readPixelsToBuffer(sshot);
			//unsigned char* pixels = sshot->lock(rr::BL_DISCARD_AND_WRITE);
			//glReadPixels(0,0,winWidth,winHeight,GL_RGB,GL_UNSIGNED_BYTE,pixels);
			//sshot->unlock();
		if (sshot->save(buf))
			rr::RRReporter::report(rr::INF1,"Saved %s.\n",buf);
		else
			rr::RRReporter::report(rr::WARN,"Error: Failed to saved %s.\n",buf);
		delete sshot;
		shotRequested = 0;
	}

	glutSwapBuffers();

	{
		// animaci nerozjede hned ale az po 2 snimcich (po 1 nestaci)
		// behem kresleni prvnich snimku driver kompiluje shadery nebo co, pauza by narusila fps
		static int framesDisplayed = 0;
		framesDisplayed++;
		if (framesDisplayed==2 && demoPlayer)
		{
			demoPlayer->setPaused(supportEditor);
		}
	}

	g_fpsCounter.addFrame();
}

void enableInteraction(bool enable)
{
	if (enable)
	{
		glutSpecialFunc(special);
		glutSpecialUpFunc(specialUp);
		glutMouseFunc(mouse);
		glutPassiveMotionFunc(passive);
		glutKeyboardFunc(keyboard);
		glutKeyboardUpFunc(keyboardUp);
		if (winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	else
	{
		glutSpecialFunc(specialPlayerOnly);
		glutSpecialUpFunc(nullptr);
		glutMouseFunc(nullptr);
		glutPassiveMotionFunc(nullptr);
		glutKeyboardFunc(keyboardPlayerOnly);
		glutKeyboardUpFunc(nullptr);
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

	/* GL_LEQUAL ensures that when fragments with equal depth are
	generated within a single rendering pass, the last fragment
	results. */

	glLineStipple(1, 0xf0f0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	glFrontFace(GL_CCW);

#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif
}

void parseOptions(int argc, const char*const*argv)
{
	int i,tmp;
	bool badArgument = false;
	boost::system::error_code ec;

	for (i=1; i<argc; i++)
	{
		if (strstr(argv[i],".cfg"))
		{
			cfgFile = argv[i];
		}
		else
		if (!strcmp("editor", argv[i]))
		{
			supportEditor = 1;
			fullscreen = 0;
			showTimingInfo = 1;
		}
		else
		if (!strcmp("bigscreen", argv[i]))
		{
			bigscreenCompensation = 1;
		}
		else
		if (sscanf(argv[i],"%dx%d",&resolutionx,&resolutiony)==2)
		{
			resolutionSet = true;
		}
		else
		if (!strcmp("window", argv[i]))
		{
			fullscreen = 0;
		}
		else
		if (!strcmp("fullscreen", argv[i]))
		{
			fullscreen = 1;
		}
		else
		if (!strcmp("stability=low", argv[i]))
		{
			lightStability = rr_gl::RRSolverGL::DDI_4X4;
		}
		else
		if (!strcmp("stability=auto", argv[i]))
		{
			lightStability = rr_gl::RRSolverGL::DDI_AUTO;
		}
		else
		if (!strcmp("stability=high", argv[i]))
		{
			lightStability = rr_gl::RRSolverGL::DDI_8X8;
		}
		else
		if (!strcmp("silent", argv[i]))
		{
			supportMusic = false;
		}
		else
		if (!strcmp("opaqueshadows", argv[i]))
		{
			alphashadows = false;
		}
		else
		if (sscanf(argv[i],"penumbra%d", &tmp)==1)
		{
			// handled elsewhere
		}
		else
		if (!strcmp("verbose", argv[i]))
		{
			rr::RRReporter::setFilter(true,2,true);
			rr_gl::Program::logMessages(true);
		}
		else
		if (!strcmp("capture=jpg", argv[i]))
		{
			captureVideo = "jpg";
			supportMusic = false;
		}
		else
		if (!strcmp("capture=tga", argv[i]))
		{
			captureVideo = "tga";
			supportMusic = false;
		}
		else
#ifdef SUPPORT_RR_ED
		if (!strcmp("editor1", argv[i]))
		{
			customScene = "";
		}
		else
#endif
		if (bf::exists(argv[i],ec))
		{
			customScene = argv[i];
		}
		else
		{
			printf("Unknown commandline argument: %s\n",argv[i]);
			badArgument = true;
		}
	}
	if (badArgument)
	{
		const char* caption = 
			"Lightsmark 2015 back-end                                       (C) Stepan Hrbek";
		const char* usage = 
#if defined(_WIN32)
			"Usage: backend.exe [arg1] [arg2] ...\n"
#else
			"Usage: backend [arg1] [arg2] ...\n"
#endif
			"\nArguments:\n"
			"  window                    - run in window\n"
			"  640x480                   - run in given resolution (default is 1280x1024)\n"
			"  silent                    - run without music (default si music)\n"
			"  bigscreen                 - boost brightness\n"
			"  stability=[low|auto|high] - set lighting stability (default is auto)\n"
			"  penumbra[1|2|3|4|5|6|7|8] - set penumbra precision (default is auto)\n"
			"  filename.cfg              - run custom content (default is Lightsmark.cfg)\n"
			"  verbose                   - log also shader diagnostic messages\n"
			"  capture=[jpg|tga]         - capture into sequence of images at 30fps\n"
			"  opaqueshadows             - use simpler shadows\n"
#ifdef SUPPORT_RR_ED
			"  editor1                   - open scene editor\n"
#endif
			"  filename.dae/obj/3ds/...  -  with custom 3d scene (40 fileformats)\n"
			"  editor                    - open animation editor\n";
#if defined(_WIN32)
		MessageBoxA(0,usage,caption,MB_OK);
#else
		printf("\n%s\n\n%s",caption,usage);
#endif
		exit(0);
	}
}

#ifdef SET_ICON
HANDLE hIcon = 0;
#endif

int main(int argc, char** argv)
{
	//_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 1154356;

	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}

#ifdef _WIN32
	// remember cwd before we change it (we can't print it yet because log is opened after change)
	char* cwd = _getcwd(nullptr,0);
#endif

	// data are in ../../data
	// solution: change dir [1] rather than expect that caller calls us from data [2]
	// why?
	// WINE can't emulate [2]
	// GPU PerfStudio can't [2]
	_chdir("../../data");

	// do this before parseOption, options might override our defaults
	rr::RRReporter::setFilter(true,1,false); // never mind that reporter doesn't exist yet, this is global setting

	parseOptions(argc, argv);

#ifdef CONSOLE
	reporter = rr::RRReporter::createPrintfReporter();
#else
	// this is windows only code
	// designed to work on vista with restricted write access
	reporter = rr::RRReporter::createFileReporter("../log.txt",false);
	if (!reporter)
	{
		// primary output directory is . (data)
		// if it fails (program files in vista is not writeable), secondary is created in appdata
		const char* appdata = getenv("LOCALAPPDATA");
		if (appdata)
		{
			sprintf(globalOutputDirectory,"%s\\%s",appdata,PRODUCT_NAME);
			_mkdir(globalOutputDirectory);

			char logname[1000];
			sprintf(logname,"%s/log.txt",globalOutputDirectory);
			reporter = rr::RRReporter::createFileReporter(logname,false);
		}
	}
#endif
#ifdef _WIN32
	rr::RRReporter::report(rr::INF1,"This is Lightsmark 2015 [Windows %dbit] log. Check it if benchmark doesn't work properly.\n",sizeof(void*)*8);
	rr::RRReporter::report(rr::INF1,"Started: %s in %s\n",GetCommandLine(),cwd);
	free(cwd);
	if (globalOutputDirectory[1])
		rr::RRReporter::report(rr::INF1,"Program directory not writeable, log+screenshots sent to %s\n",globalOutputDirectory);
#elif defined(__APPLE__)
	rr::RRReporter::report(rr::INF1,"This is Lightsmark 2015 [OSX %dbit] log. Check it if benchmark doesn't work properly.\n",sizeof(void*)*8);
#else
	rr::RRReporter::report(rr::INF1,"This is Lightsmark 2015 [Linux %dbit] log. Check it if benchmark doesn't work properly.\n",sizeof(void*)*8);
#endif

	rr_io::registerIO(argc,argv);

#ifdef SUPPORT_RR_ED
	if (customScene)
	{
		if (customScene[0])
			rr_ed::sceneViewer(nullptr,customScene,"maps/skybox/skybox_bk.jpg","",nullptr,false);
		else
		{
			rr_ed::SceneViewerState svs;
			svs.renderLDM = true;
			svs.renderTonemapping = false;
			svs.camera.setPosition(rr::RRVec3(23.554733f,-5.993851f,-3.134015f));
			svs.camera.setDirection(rr::RRVec3(0.64f,-0.3f,-0.7f));
			svs.cameraMetersPerSecond = 1;
			svs.autodetectCamera = false;
			rr_ed::sceneViewer(nullptr,"scenes/wop_padattic/wop_padatticBB.bsp","","",&svs,false);
		}
		return 0;
	}
#endif

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_ALPHA); // | GLUT_ACCUM | GLUT_ALPHA accum na high quality soft shadows, alpha na filtrovani ambient map
	if (fullscreen)
	{
		int scrx = glutGet(GLUT_SCREEN_WIDTH);
		int scry = glutGet(GLUT_SCREEN_HEIGHT);
		if (!resolutionSet && (scrx<resolutionx || scry<resolutiony))
		{
			rr::RRReporter::report(rr::WARN,"Default 1280x1024 too high for your screen, switching to %dx%d.\n",scrx,scry);
			resolutionx = scrx;
			resolutiony = scry;
		}
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
		if (resolutionx==w && resolutiony==h)
			glutFullScreen();
		if (supportEditor)
			initMenu();
	}
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);
	if (glutGet(GLUT_WINDOW_WIDTH)!=resolutionx || glutGet(GLUT_WINDOW_HEIGHT)!=resolutiony
	    || (fullscreen && (glutGet(GLUT_SCREEN_WIDTH)<resolutionx || glutGet(GLUT_SCREEN_HEIGHT)<resolutiony)))
	{
		rr::RRReporter::report(rr::ERRO,"Sorry, unable to set %dx%d %s, try different mode.\n",resolutionx,resolutiony,fullscreen?"fullscreen":"window");
		exiting = true;
		exit(0);
	};
#ifdef __APPLE__
	// OSX kills events ruthlessly
	// see http://stackoverflow.com/questions/728049/glutpassivemotionfunc-and-glutwarpmousepointer
	CGSetLocalEventsSuppressionInterval(0.0);
#endif

	// init GL
	const char* err = rr_gl::initializeGL();
	if (err)
	{
		rr::RRReporter::report(rr::ERRO,err);
		exiting = true;
		exit(0);
	}

	int i1=0,i2=0,i3=0,i4=0,i5=0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&i1);
	glGetIntegerv(GL_MAX_TEXTURE_UNITS,&i2);
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&i3);
	glGetIntegerv(GL_MAX_TEXTURE_COORDS,&i4);
	glGetIntegerv(GL_MAX_VARYING_FLOATS,&i5);
	rr::RRReporter::report(rr::INF1,"  %d image units, %d units, %d combined, %d coords, %d varyings.\n",i1,i2,i3,i4,i5);

	init_gl_states();

	uberProgramGlobalSetup.SHADOW_MAPS = 1;
	uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
	uberProgramGlobalSetup.SHADOW_PENUMBRA = 1;
	uberProgramGlobalSetup.LIGHT_DIRECT = 1;
	uberProgramGlobalSetup.LIGHT_DIRECT_MAP = 1;
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR =
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR_LINEAR = 1;
	uberProgramGlobalSetup.LIGHT_INDIRECT_DETAIL_MAP = 1;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE = 1;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = 1;
	uberProgramGlobalSetup.MATERIAL_SPECULAR = 1; // for robot
	uberProgramGlobalSetup.MATERIAL_SPECULAR_CONST = 1; // for robot
	uberProgramGlobalSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = 1;
	uberProgramGlobalSetup.POSTPROCESS_BRIGHTNESS = 1;

	// init shaders
	// init textures
	init_gl_resources();

	// adjust INSTANCES_PER_PASS to GPU
	INSTANCES_PER_PASS = uberProgramGlobalSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	rr::RRReporter::report(rr::INF1,"  penumbra quality: %d\n",INSTANCES_PER_PASS);
	if (!INSTANCES_PER_PASS) error("",true);

#ifdef SET_ICON
	HWND hWnd = FindWindowA(nullptr,PRODUCT_NAME);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
#endif

	enableInteraction(supportEditor);

	glutMainLoop();
	return 0;
}

#ifndef CONSOLE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
#ifdef SET_ICON
	hIcon = LoadImage(hInstance,MAKEINTRESOURCE(IDI_ICON1),IMAGE_ICON,0,0,0);
#endif
	int argc = 0;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argvw && argc)
	{
		// build argv from commandline
		char** argv = new char*[argc+1];
		for (int i=0;i<argc;i++)
		{
			argv[i] = (char*)malloc(wcslen(argvw[i])+1);
			sprintf(argv[i], "%ws", argvw[i]);
		}
		argv[argc] = nullptr;
		return main(argc,argv);
	}
	else
	{
		// someone calls us with invalid arguments, but don't panic, build argv from module filename
		char szFileName[MAX_PATH];
		GetModuleFileNameA(nullptr,szFileName,MAX_PATH);
		char* argv[2] = {szFileName,nullptr};
		return main(1,argv);
	}
}
#endif
