//#define BUGS
#define MAX_INSTANCES              10  // max number of light instances aproximating one area light
unsigned INSTANCES_PER_PASS;
#define SHADOW_MAP_SIZE_SOFT       512
#define SHADOW_MAP_SIZE_HARD       2048
#define LIGHTMAP_SIZE_FACTOR       10
#define LIGHTMAP_QUALITY           100
#define PRIMARY_SCAN_PRECISION     1 // 1nejrychlejsi/2/3nejpresnejsi, 3 s texturami nebude fungovat kvuli cachovani pokud se detekce vseho nevejde na jednu texturu - protoze displaylist myslim neuklada nastaveni textur
#define SUPPORT_LIGHTMAPS          1
//#define CALCULATE_WHEN_PLAYING_PRECALCULATED_MAPS // calculate() is necessary only for correct envmaps (dynamic objects)
//#define RENDER_OPTIMIZED
#define THREE_ONE
#define CFG_FILE "3+1.cfg"
//#define CFG_FILE "LightsprintDemo.cfg"
//#define CFG_FILE "Candella.cfg"
//#define CFG_FILE "test.cfg"
//#define CFG_FILE "Lowpoly.cfg"
bool ati = 1;
int fullscreen = 1;
bool startWithSoftShadows = 1;
int resolutionx = 1024;
int resolutiony = 768;
bool twosided = 0;
bool supportEditor = 0;
bool bigscreenCompensation = 0;
bool bigscreenSimulator = 0;
bool showTimingInfo = 0;
bool captureMovie = 0; // when replaying, fix to 30Hz and capture all screenshots

bool renderConstantAmbient = 0;
bool renderVertexColors = 1;
bool renderLightmaps = 0;
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
#include <GL/wglew.h>
#include <GL/glut.h>
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/RendererOfRRObject.h"
#include "Lightsprint/GL/AreaLight.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/Water.h"
#include "DynamicObject.h"
#include "Bugs.h"
#include "AnimationEditor.h"
#include "Autopilot.h"
#include "DemoPlayer.h"
#include "DynamicObjects.h"
//#include "../RRVision/RRDynamicSolver/report.h"


/////////////////////////////////////////////////////////////////////////////
//
// globals

AnimationFrame currentFrame(0);
GLUquadricObj *quadric;
rr_gl::AreaLight* areaLight = NULL;
rr_gl::Water* water = NULL;
#ifdef THREE_ONE
#else
	rr_gl::Texture* lightsprintMap = NULL; // small logo in the corner
#endif
rr_gl::Program* ambientProgram;
rr_gl::TextureRenderer* skyRenderer;
rr_gl::UberProgram* uberProgram;
rr_gl::UberProgramSetup uberProgramGlobalSetup;
int winWidth = 0;
int winHeight = 0;
int depthBias24 = 50;//23;//42;
int depthScale24;
GLfloat slopeScale = 5;//2.3;//4.0;
int needDepthMapUpdate = 1;
bool needLightmapCacheUpdate = false;
int wireFrame = 0;
int needMatrixUpdate = 1;
int showHelp = 0; // 0=none, 1=help, 2=credits
int showLightViewFrustum = 0;
bool modeMovingEye = 0;
unsigned movingEye = 0;
unsigned movingLight = 0;
bool needRedisplay = 0;
bool needReportEyeChange = 0;
bool needReportLightChange = 0;
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
char* cfgFile = CFG_FILE;

/////////////////////////////////////////////////////////////////////////////

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 6xxx\n - GeForce 7xxx\n - GeForce 8xxx\n - Radeon 9500-9800\n - Radeon Xxxx\n - Radeon X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

void updateDepthBias(int delta)
{
	depthBias24 += delta;
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needDepthMapUpdate = 1;
	//printf("%f %d %d\n",slopeScale,depthBias24,depthScale24);
}

void setupAreaLight()
{
	bool wantSoft = uberProgramGlobalSetup.SHADOW_SAMPLES>1 || areaLight->getNumInstances()>1;
	if(wantSoft)
	{
		areaLight->setShadowmapSize(SHADOW_MAP_SIZE_SOFT);
		// 8800@24bit // x300+gf6150
		depthBias24 = 50;//23;
		slopeScale = 5;//2.3f;
	}
	else
	{
		areaLight->setShadowmapSize(SHADOW_MAP_SIZE_HARD);
		depthBias24 = 30;//1;
		slopeScale = 3;//0.1f;
	}
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needDepthMapUpdate = 1;
}

void init_gl_resources()
{
	quadric = gluNewQuadric();

	areaLight = new rr_gl::AreaLight(&currentFrame.light,MAX_INSTANCES,SHADOW_MAP_SIZE_SOFT);

	// update states, but must be done after initing shadowmaps (inside arealight)
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getTexelBits();
	depthScale24 = 1 << (shadowDepthBits-16);
	updateDepthBias(0);  /* Update with no offset change. */

#ifdef THREE_ONE
#else
	lightsprintMap = rr_gl::Texture::load("maps\\logo230awhite.png", NULL, false, false, GL_NEAREST, GL_NEAREST, GL_CLAMP, GL_CLAMP);
#endif

	uberProgram = rr_gl::UberProgram::create("shaders\\ubershader.vs", "shaders\\ubershader.fs");
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	ambientProgram = uberProgram->getProgram(uberProgramSetup.getSetupString());

	water = new rr_gl::Water("shaders/",true,false);
	skyRenderer = new rr_gl::TextureRenderer("shaders/");

	if(!ambientProgram)
		error("\nFailed to compile or link GLSL program.\n",true);
}

void done_gl_resources()
{
	SAFE_DELETE(skyRenderer);
	SAFE_DELETE(uberProgram);
#ifdef THREE_ONE
#else
	SAFE_DELETE(lightsprintMap);
#endif
	SAFE_DELETE(areaLight);
	gluDeleteQuadric(quadric);
}

/////////////////////////////////////////////////////////////////////////////
//
// Solver

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance);
void updateMatrices();
void updateDepthMap(unsigned mapIndex,unsigned mapIndices);

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("shaders/")
	{
		boostDetectedDirectIllumination = 2;
	}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		unsigned res = 16; // don't create maps below 16x16, otherwise you risk poor performance on Nvidia cards
		while(res<2048 && res<LIGHTMAP_SIZE_FACTOR*sqrtf(object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		needLightmapCacheUpdate = true; // pokazdy kdyz pridam/uberu jakoukoliv lightmapu, smaznout z cache
		return rr_gl::RRDynamicSolverGL::createIlluminationPixelBuffer(res,res);
	}
	virtual void detectMaterials()
	{
	}
	virtual bool detectDirectIllumination()
	{
		if(!demoPlayer)
			return false;

		// first time illumination is detected, no shadowmap has been created yet
		if(needDepthMapUpdate)
		{
			if(needMatrixUpdate) updateMatrices(); // probably not necessary
			// to avoid depth updates only for us, all depthmaps are updated and then used by following display()
			unsigned numInstances = areaLight->getNumInstances();
			for(unsigned i=0;i<numInstances;i++)
			{
				updateDepthMap(i,numInstances);
			}
		}

		// setup shader for rendering direct illumination+shadows without materials
		return RRDynamicSolverGL::detectDirectIllumination();
	}
	virtual void setupShader(unsigned objectNumber)
	{
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = 1;
		uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
#if PRIMARY_SCAN_PRECISION==1 // 110ms
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = false;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
#elif PRIMARY_SCAN_PRECISION==2 // 150ms
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = true;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
#else // PRIMARY_SCAN_PRECISION==3 // 220ms
		uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = false;
		uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
#endif
		uberProgramSetup.MATERIAL_SPECULAR = false;
		uberProgramSetup.MATERIAL_SPECULAR_CONST = false;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		uberProgramSetup.MATERIAL_NORMAL_MAP = false;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = true;

		if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,demoPlayer->getProjector(currentFrame.projectorIndex),NULL,1))
			error("Failed to compile or link GLSL program.\n",true);
	}
};

// called from Level.cpp
rr::RRDynamicSolver* createSolver()
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
	glTranslatef(currentFrame.light.pos[0]-0.3*currentFrame.light.dir[0], currentFrame.light.pos[1]-0.3*currentFrame.light.dir[1], currentFrame.light.pos[2]-0.3*currentFrame.light.dir[2]);
	glColor3f(1,1,0);
	gluSphere(quadric, 0.05, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	currentFrame.eye.aspect = winHeight ? (float) winWidth / (float) winHeight : 1;
	currentFrame.eye.update(0);
	currentFrame.light.update(0.3f);
	needMatrixUpdate = false;
}

/* drawShadowMapFrustum - Draw dashed lines around the light's view
   frustum to help visualize the region captured by the depth map. */
void drawShadowMapFrustum(void)
{
	/* Go from light clip space, ie. the cube [-1,1]^3, to world space by
	transforming each clip space cube corner by the inverse of the light
	frustum transform, then by the inverse of the light view transform.
	This results in world space coordinate that can then be transformed
	by the eye view transform and eye projection matrix and on to the
	screen. */

	ambientProgram->useIt();
	glColor3f(1,1,1);

	glEnable(GL_LINE_STIPPLE);
	glPushMatrix();
	glMultMatrixd(currentFrame.light.inverseViewMatrix);
	glMultMatrixd(currentFrame.light.inverseFrustumMatrix);
	/* Draw a wire frame cube with vertices at the corners
	of clip space.  Draw the top square, drop down to the
	bottom square and finish it, then... */
	glBegin(GL_LINE_STRIP);
	glVertex3f(1,1,1);
	glVertex3f(1,1,-1);
	glVertex3f(-1,1,-1);
	glVertex3f(-1,1,1);
	glVertex3f(1,1,1);
	glVertex3f(1,-1,1);
	glVertex3f(-1,-1,1);
	glVertex3f(-1,-1,-1);
	glVertex3f(1,-1,-1);
	glVertex3f(1,-1,1);
	glEnd();
	/* Draw the final three line segments connecting the top
	and bottom squares. */
	glBegin(GL_LINES);
	glVertex3f(1,1,-1);
	glVertex3f(1,-1,-1);

	glVertex3f(-1,1,-1);
	glVertex3f(-1,-1,-1);

	glVertex3f(-1,1,1);
	glVertex3f(-1,-1,1);
	glEnd();
	glPopMatrix();
	glDisable(GL_LINE_STIPPLE);
}

// callback that feeds 3ds renderer with our vertex illumination
const float* lockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRDynamicSolver*)solver)->getIllumination(object)->getLayer(0)->vertexBuffer;
	return vertexBuffer ? &vertexBuffer->lock()->x : NULL;
}

// callback that cleans vertex illumination
void unlockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRDynamicSolver*)solver)->getIllumination(object)->getLayer(0)->vertexBuffer;
	if(vertexBuffer) vertexBuffer->unlock();
}

void renderSceneStatic(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	if(!level) return;

#ifdef RENDER_OPTIMIZED
	level->rendererOfScene->useOptimizedScene();
#else
	// update realtime layer 0
	static unsigned solutionVersion = 0;
	if(level->solver->getSolutionVersion()!=solutionVersion)
	{
		solutionVersion = level->solver->getSolutionVersion();
		level->solver->updateVertexBuffers(0,-1,true,NULL,NULL);
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

	// boost quake map intensity
	if(level->type==Level::TYPE_BSP && uberProgramSetup.MATERIAL_DIFFUSE_MAP)
	{
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = true;
	}

	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	level->rendererOfScene->setBrightnessGamma(&globalBrightnessBoosted[0],globalGammaBoosted);

	level->rendererOfScene->setParams(uberProgramSetup,areaLight,demoPlayer->getProjector(currentFrame.projectorIndex));
	level->rendererOfScene->render();
}

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	// render static scene
	assert(!uberProgramSetup.OBJECT_SPACE); 
	renderSceneStatic(uberProgramSetup,firstInstance);
	if(uberProgramSetup.FORCE_2D_POSITION) return;
	rr::RRVec4 globalBrightnessBoosted = currentFrame.brightness;
	rr::RRReal globalGammaBoosted = currentFrame.gamma;
	assert(demoPlayer);
	demoPlayer->getBoost(globalBrightnessBoosted,globalGammaBoosted);
	demoPlayer->getDynamicObjects()->renderSceneDynamic(level->solver,uberProgram,uberProgramSetup,areaLight,firstInstance,demoPlayer->getProjector(currentFrame.projectorIndex),&globalBrightnessBoosted[0],globalGammaBoosted);
}

void updateDepthMap(unsigned mapIndex,unsigned mapIndices)
{
	if(!needDepthMapUpdate) return;
	assert(mapIndex>=0);
	rr_gl::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;

	glColorMask(0,0,0,0);
	rr_gl::Texture* shadowmap = areaLight->getShadowMap((mapIndex>=0)?mapIndex:0);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClearDepth(0.9999); // prevents backprojection
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);

	rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
	uberProgramSetup.SHADOW_MAPS = 0;
	uberProgramSetup.SHADOW_SAMPLES = 0;
	uberProgramSetup.LIGHT_DIRECT = false;
	uberProgramSetup.LIGHT_DIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_CONST = false;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_auto = false;
	uberProgramSetup.LIGHT_INDIRECT_ENV = false;
	uberProgramSetup.MATERIAL_DIFFUSE = false;
	uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
	uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
	uberProgramSetup.MATERIAL_SPECULAR = false;
	uberProgramSetup.MATERIAL_SPECULAR_CONST = false;
	uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
	uberProgramSetup.MATERIAL_NORMAL_MAP = false;
	uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
	//uberProgramSetup.OBJECT_SPACE = false;
	uberProgramSetup.FORCE_2D_POSITION = false;
	renderScene(uberProgramSetup,0);
	shadowmap->renderingToEnd();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);


	if(mapIndex==mapIndices-1) 
	{
		needDepthMapUpdate = 0;
	}
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

	if(firstInstance==0) glClear(GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	currentFrame.eye.setupForRender();

	renderScene(uberProgramSetup,firstInstance);

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
		drawShadowMapFrustum();
}

void drawEyeViewSoftShadowed(void)
{
	// update shadowmaps
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++)
	{
		updateDepthMap(i,numInstances);
	}
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// optimized path without accum
	if(numInstances<=INSTANCES_PER_PASS)
	{
		// update water reflection
		if(water && level->pilot.setup->renderWater)
		{
			water->updateReflectionInit(winWidth/4,winHeight/4,&currentFrame.eye,level->pilot.setup->waterLevel);
			glClear(GL_DEPTH_BUFFER_BIT);
			rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			uberProgramSetup.LIGHT_INDIRECT_CONST = renderConstantAmbient;
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = renderVertexColors;
			uberProgramSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
			uberProgramSetup.LIGHT_INDIRECT_auto = renderLightmaps;
			uberProgramSetup.LIGHT_INDIRECT_ENV = false;
			drawEyeViewShadowed(uberProgramSetup,0);
			water->updateReflectionDone();
		}

		// render everything except water
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = numInstances;
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = renderConstantAmbient;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = renderVertexColors;
		uberProgramSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_auto = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_CONST = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		//uberProgramSetup.MATERIAL_SPECULAR = ;
		//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
		//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,0);

		// render water
		if(water && level->pilot.setup->renderWater)
		{
			water->render(100);
		}

		return;
	}

	glClear(GL_ACCUM_BUFFER_BIT);

	// add direct
	for(unsigned i=0;i<numInstances;i+=INSTANCES_PER_PASS)
	{
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = MIN(INSTANCES_PER_PASS,numInstances);
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_CONST = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		//uberProgramSetup.MATERIAL_SPECULAR = ;
		//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
		//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,i);
		glAccum(GL_ACCUM,1./(numInstances/MIN(INSTANCES_PER_PASS,numInstances)));
	}
	// add indirect
	{
		rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.LIGHT_DIRECT = false;
		uberProgramSetup.LIGHT_DIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = renderVertexColors;
		uberProgramSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_auto = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_CONST = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		//uberProgramSetup.MATERIAL_SPECULAR = ;
		//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
		//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = false;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderScene(uberProgramSetup,0);
		glAccum(GL_ACCUM,1);
	}

	glAccum(GL_RETURN,1);
}

// captures current scene into thumbnail
void updateThumbnail(AnimationFrame& frame)
{
	//rr::RRReporter::report(rr::INF1,"Updating thumbnail.\n");
	// set frame
	demoPlayer->getDynamicObjects()->copyAnimationFrameToScene(level->pilot.setup,frame,true);
	// calculate
	level->solver->calculate();
	// update shadows in advance, so following render doesn't touch FBO
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned j=0;j<numInstances;j++)
	{
		updateDepthMap(j,numInstances);
	}
	// render into thumbnail
	if(!frame.thumbnail)
		frame.thumbnail = rr_gl::Texture::create(NULL,160,120,false,rr_gl::Texture::TF_RGB,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);
	glViewport(0,0,160,120);
	//frame.thumbnail->renderingToBegin();
	drawEyeViewSoftShadowed();
	//frame.thumbnail->renderingToEnd();
	frame.thumbnail->bindTexture();
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
}

static void drawHelpMessage(int screen)
{
	if(shotRequested) return;
//	if(!big && gameOn) return;

	static const char *message[3][30] = 
	{
		{
		"h - help",
		NULL
		},
		{
#ifdef THREE_ONE
		"3+1 by clrsrc+Lightsprint",
#else
		"Lightsprint Realtime Radiosity",
		"  http://lightsprint.com",
		"  realtime global illumination, NO PRECALCULATIONS",
#endif
		"",
		"Controls:",
		" space         - pause/play",
		" mouse         - look",
		" arrows/wsadqz - move",
		" left button   - switch between camera/light",
		" right button  - next scene",
		"",
		"Extra controls:",
		" F1/F2/F3      - shadows: hard/soft/penumbra",
		" F5/F6/F7/F8   - ambient: none/const/realtime/precalc",
#ifndef THREE_ONE
		" F6            - credits",
#endif
		" F11           - save screenshot",
		" wheel         - zoom",
		" x/c           - lean",
		" enter         - fullscreen/window",
		" +/-/*//       - brightness/contrast",
		" 1/2/3/4...    - move n-th object",
#ifdef THREE_ONE
		"",
		"For more information on Realtime Global Illumination",
		"or Penumbra Shadows, visit http://lightsprint.com",
#endif
/*
		" space - toggle global illumination",
		" '+ -' - increase/decrease penumbra (soft shadow) precision",
		" '* /' - increase/decrease penumbra (soft shadow) smoothness",
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
		"'C'   - expand shadow frustum far clip plane",
		"'b'   - increment the depth bias for 1st pass glPolygonOffset",
		"'B'   - decrement the depth bias for 1st pass glPolygonOffset",
		"'q'   - increment depth slope for 1st pass glPolygonOffset",
		"'Q'   - increment depth slope for 1st pass glPolygonOffset",*/
		NULL,
		}
#ifndef THREE_ONE
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
#endif
	};
	int i;
	int x = 40, y = 50;

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
	glColor4f(0.0,0.0,0.0,0.6);

	// Drawn clockwise because the flipped Y axis flips CCW and CW.
	if(screen /*|| demoPlayer->getPaused()*/)
	{
		glRecti(winWidth - 30, 30, 30, winHeight - 30);
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
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
		glRecti(MIN(winWidth-30,500), 30, 30, MIN(winHeight-30,100));
		glDisable(GL_BLEND);
		glColor3f(1,1,1);
		char buf[200];
		float demoLength = demoPlayer->getDemoLength();
		float musicLength = demoPlayer->getMusicLength();
		sprintf(buf,"demo %.1f/%.1fs, byt %.1f/%.1fs, music %.1f/%.1f%s",
			demoPlayer->getDemoPosition(),demoLength,
			demoPlayer->getPartPosition(),demoPlayer->getPartLength(),
			demoPlayer->getMusicPosition(),musicLength,
			(musicLength<demoLength)?" MUSIC<DEMO!!!":"");
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
		sprintf(buf,"byt %d/%d, frame %d/%d, %.1f/%.1fs",
			demoPlayer->getPartIndex()+1,demoPlayer->getNumParts(),
			frameIndex+1,level->pilot.setup->frames.size(),
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

void showOverlay(const rr_gl::Texture* tex)
{
	if(!tex) return;
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	float color[4] = {currentFrame.brightness[0],currentFrame.brightness[1],currentFrame.brightness[2],1};
	skyRenderer->render2D(tex,color,0,0,1,1);
	glDisable(GL_BLEND);
}

void showLogo(const rr_gl::Texture* logo)
{
	if(!logo) return;
	float w = logo->getWidth()/(float)winWidth;
	float h = logo->getHeight()/(float)winHeight;
	float x = 1-w;
	float y = 1-h;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	skyRenderer->render2D(logo,NULL,x,y,w,h);
	glDisable(GL_BLEND);
}

/*
// shortcut for multiple scenes displayed at once
void displayScene(unsigned sceneIndex, float sceneTime, rr::RRVec3 offset)
{
	// backup old state
	Level* oldLevel = level;
	rr_gl::Camera oldEye = eye;
	rr_gl::Camera oldLight = light;

	level = demoPlayer->getPart(sceneIndex);
	assert(level);
	demoPlayer->getDynamicObjects()->setupSceneDynamicForPartTime(level->pilot.setup,sceneTime);
	eye = oldEye;
	eye.pos[0] -= offset[0];
	eye.pos[1] -= offset[1];
	eye.pos[2] -= offset[2];
	eye.update(0);
	light.update(0.3f);

	//!!! always updated = waste of GPU performance
	needDepthMapUpdate = 1;
	updateDepthMap(0,0);

	rr_gl::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = false;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_ENV = false;
	//uberProgramSetup.MATERIAL_DIFFUSE = ;
	//uberProgramSetup.MATERIAL_DIFFUSE_CONST = ;
	//uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = ;
	//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
	//uberProgramSetup.MATERIAL_SPECULAR = ;
	//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
	//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
	//uberProgramSetup.OBJECT_SPACE = false;
	uberProgramSetup.FORCE_2D_POSITION = false;
	eye.setupForRender();
	renderScene(uberProgramSetup,0);

	// restore old state
	light = oldLight;
	eye = oldEye;
	level = oldLevel;
}

void displayScenes()
{
	if(demoPlayer->getNumParts()>1)
	{
	//	displayScene(0,8,rr::RRVec3( 0,0, 0));
		displayScene(0,8,rr::RRVec3(20,0,12)); //!!! 8
		displayScene(1,8,rr::RRVec3(20,0, 0));
		displayScene(0,8,rr::RRVec3( 0,0,12));
	}
	//eye.update(0);
	//light.update(0.3f);
}
*/

/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void keyboard(unsigned char c, int x, int y);

void display()
{
	if(!winWidth) return; // can't work without window
	static unsigned skipFrames = 0;
	//printf("<Display.>\n");
	if(!level)
	{
//		showImage(loadingMap);
//		showImage(loadingMap); // neznamo proc jeden show nekdy nestaci na spravny uvodni obrazek
		//delete level;
		level = demoPlayer->getNextPart(seekInMusicAtSceneSwap,supportEditor);

		// end of the demo
		if(!level)
			keyboard(27,0,0);

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

		// don't display first frame, characters have bad position (dunno why)
		skipFrames = 1;
	}

	// pro jednoduchost je to tady
	// kdyby to bylo u vsech stisku/pusteni/pohybu klaves/mysi a animaci,
	//  nevolalo by se to zbytecne v situaci kdy redisplay vyvola calculate() hlaskou ze zlepsil osvetleni
	// zisk by ale byl miniaturni
	level->solver->reportInteraction();

	if(needMatrixUpdate)
		updateMatrices();

	needRedisplay = 0;

	drawEyeViewSoftShadowed();

	if(wireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if(renderInfo)
	{
#ifdef THREE_ONE
		if(!demoPlayer->getPaused())
			showOverlay(level->pilot.setup->getOverlay());
#else
		showLogo(lightsprintMap);
#endif

		if(demoPlayer->getPaused() && level->animationEditor)
		{
			level->animationEditor->renderThumbnails(skyRenderer);
		}

		drawHelpMessage(showHelp);
	}

	/*
#ifndef THREE_ONE
	// skip first frame in level
	// character positions are wrong (dunno why)
	// it is always longer because buffers and display lists are created,
	//  so don't count it into average fps
	if(skipFrames)
	{
		skipFrames--;
		// update dynobjects when movement is paused
		// without this update, dynobjects would have bad pos after "pause, next level"
		if(paused)
			dynaobjects->updateSceneDynamic(level->pilot.setup,0);
		return;
	}
#endif
	*/

	if(shotRequested)
	{
		static unsigned shots = 0;
		shots++;
		char buf[100];
		//sprintf(buf,"Lightsprint3+1_%02d.png",shots);
		sprintf(buf,"video\\frame%04d.jpg",shots);
		if(rr_gl::Texture::saveBackbuffer(buf))
			rr::RRReporter::report(rr::INF1,"Saved %s.\n",buf);
		else
			rr::RRReporter::report(rr::WARN,"Error: Failed to saved %s.\n",buf);
		shotRequested = 0;
	}

	glutSwapBuffers();

	//printf("cache: hits=%d misses=%d",rr::RRStaticSolver::getSceneStatistics()->numIrradianceCacheHits,rr::RRStaticSolver::getSceneStatistics()->numIrradianceCacheMisses);

	// fallback to hard shadows if fps<30
	static int framesDisplayed = 0;
	static TIME frame0Time;
	if(!framesDisplayed)
		frame0Time = GETTIME;
	if(framesDisplayed>0)
	{
		float secs = (GETTIME-frame0Time)/(float)PER_SEC;
		if(secs>1)
		{
			if(framesDisplayed<30 && areaLight->getNumInstances()>1 && uberProgramGlobalSetup.SHADOW_SAMPLES==4)
			{
				areaLight->setNumInstances(1);
				// nvidia 6150 has free blur, set hard blurred
				// ati x1600 has expensive blur, set hard
				if(ati)
					uberProgramGlobalSetup.SHADOW_SAMPLES = 1;
				setupAreaLight();
			}
			framesDisplayed = -1; // disable
		}
	}
	if(framesDisplayed>=0)
		framesDisplayed++;
}

void toggleWireFrame(void)
{
	wireFrame = !wireFrame;
	if (wireFrame) {
		glClearColor(0.1,0.2,0.2,0);
	} else {
		glClearColor(0,0,0,0);
	}
	glutPostRedisplay();
}

void changeSpotlight()
{
	currentFrame.projectorIndex = (currentFrame.projectorIndex+1)%demoPlayer->getNumProjectors();
	//light.fieldOfView = 50+40.0*rand()/RAND_MAX;
	needDepthMapUpdate = 1;
	if(!level) return;
	level->solver->reportDirectIlluminationChange(true);
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
	// Behem pohybu svetla v male scene dava lepsi vysledky update (false)
	//  scena neni behem pohybu tmavsi, setrvacnost je neznatelna.
	// Ve velke scene dava lepsi vysledky reset (true),
	//  scena sice behem pohybu ztmavne,
	//  pri false je ale velka setrvacnost, nekdy dokonce stary indirect vubec nezmizi.
	level->solver->reportDirectIlluminationChange(level->solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
	needDepthMapUpdate = 1;
	needMatrixUpdate = 1;
	needRedisplay = 1;
	movingLight = 3;
}

void reportLightMovementEnd()
{
	if(!level) return;
	movingLight = 0;
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
		unsigned existingFrameNumber = MIN(level->animationEditor->frameCursor,level->pilot.setup->frames.size()-1);
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

	switch (c) 
	{
		case GLUT_KEY_F4:
			if(modif&GLUT_ACTIVE_ALT)
			{
				keyboard(27,0,0);
			}
			break;

		case GLUT_KEY_F1:
			areaLight->setNumInstances(1);
			uberProgramGlobalSetup.SHADOW_SAMPLES = 1;
			setupAreaLight();
			break;
		case GLUT_KEY_F2:
			areaLight->setNumInstances(1);
			uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
			setupAreaLight();
			break;
		case GLUT_KEY_F3:
			areaLight->setNumInstances(INSTANCES_PER_PASS);
			uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
			setupAreaLight();
			break;

		case GLUT_KEY_F5: renderConstantAmbient = false; renderVertexColors = false; renderLightmaps = false; break;
		case GLUT_KEY_F6: renderConstantAmbient = true; renderVertexColors = false; renderLightmaps = false; break;
		case GLUT_KEY_F7: renderConstantAmbient = false; renderVertexColors = true; renderLightmaps = false; break;
		case GLUT_KEY_F8: renderConstantAmbient = false; renderVertexColors = false; renderLightmaps = true; break;

		case GLUT_KEY_F11:
			shotRequested = 1;
			break;

			/*
#ifndef THREE_ONE
		case GLUT_KEY_F6:
			switch(showHelp)
			{
				case 0: showHelp = 2; break;
				case 1: showHelp = 2; break;
				case 2: showHelp = 0; break;
			}
			break;
#endif
			*/

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

void keyboard(unsigned char c, int x, int y)
{
#if SUPPORT_LIGHTMAPS
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
			// rychlejsi ukonceni:
			//if(supportEditor) delete level; // aby se ulozily zmeny v animaci
			// pomalejsi ukonceni s uvolnenim pameti:
			delete demoPlayer;

			done_gl_resources();
			delete rr::RRReporter::getReporter();
			rr::RRReporter::setReporter(NULL);
			exit(0);
			break;
		case 'H':
			switch(showHelp)
			{
				case 0: showHelp = 1; break;
				case 1: showHelp = 0; break;
				case 2: showHelp = 1; break;
			}
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
			break;

		case 13:
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
			break;

		case '[': areaLight->areaSize /= 1.2f; needDepthMapUpdate = 1; printf("%f\n",areaLight->areaSize); break;
		case ']': areaLight->areaSize *= 1.2f; needDepthMapUpdate = 1; break;			

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
						demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,ray->hitPoint3d);//+rr::RRVec3(0,1.2f,0));
				}
				/*
#ifndef THREE_ONE
				dynaobjects->updateSceneDynamic(level->pilot.setup,0,c-'1');
#endif
				*/
				needDepthMapUpdate = 1;
			}
			break;

#define CHANGE_ROT(dY,dZ) demoPlayer->getDynamicObjects()->setRot(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getRot(selectedObject_indexInDemo)+rr::RRVec2(dY,dZ))
#define CHANGE_POS(dX,dY,dZ) demoPlayer->getDynamicObjects()->setPos(selectedObject_indexInDemo,demoPlayer->getDynamicObjects()->getPos(selectedObject_indexInDemo)+rr::RRVec3(dX,dY,dZ))
		case 'j': CHANGE_ROT(-5,0); break;
		case 'k': CHANGE_ROT(+5,0); break;
		case 'u': CHANGE_ROT(0,-5); break;
		case 'i': CHANGE_ROT(0,+5); break;
		case 'f': CHANGE_POS(-0.05,0,0); break;
		case 'h': CHANGE_POS(+0.05,0,0); break;
		case 'v': CHANGE_POS(0,-0.05,0); break;
		case 'r': CHANGE_POS(0,+0.05,0); break;
		case 'g': CHANGE_POS(0,0,-0.05); break;
		case 't': CHANGE_POS(0,0,+0.05); break;

		case 'm':
			changeSpotlight();
			break;

		case '+':
			for(unsigned i=0;i<4;i++) currentFrame.brightness[i] *= 1.2;
			break;
		case '-':
			for(unsigned i=0;i<4;i++) currentFrame.brightness[i] /= 1.2;
			break;
		case '*':
			currentFrame.gamma *= 1.2;
			break;
		case '/':
			currentFrame.gamma /= 1.2;
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
		case 'b':
			updateDepthBias(+1);
			break;
		case 'B':
			updateDepthBias(-1);
			break;
		case 'q':
			slopeScale += 0.1;
			needDepthMapUpdate = 1;
			updateDepthBias(0);
			break;
		case 'Q':
			slopeScale -= 0.1;
			if (slopeScale < 0.0) {
				slopeScale = 0.0;
			}
			needDepthMapUpdate = 1;
			updateDepthBias(0);
			break;
		case 'a':
			++areaLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		case 'S':
			solver->reportDirectIlluminationChange(true);
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
			break;
		case '*':
			if(uberProgramGlobalSetup.SHADOW_SAMPLES<8)
			{
				uberProgramGlobalSetup.SHADOW_SAMPLES *= 2;
				setupAreaLight();
			}
			//	else uberProgramGlobalSetup.SHADOW_SAMPLES=9;
			break;
		case '/':
			//if(uberProgramGlobalSetup.SHADOW_SAMPLES==9) uberProgramGlobalSetup.SHADOW_SAMPLES=8; else
			if(uberProgramGlobalSetup.SHADOW_SAMPLES>1) 
			{
				uberProgramGlobalSetup.SHADOW_SAMPLES /= 2;
				setupAreaLight();
			}
			break;
		case '+':
			{
				unsigned numInstances = areaLight->getNumInstances();
				if(numInstances+INSTANCES_PER_PASS<=MAX_INSTANCES) 
				{
					if(numInstances==1 && numInstances<INSTANCES_PER_PASS)
						numInstances = INSTANCES_PER_PASS;
					// vypnuty accum, protoze nedela dobrotu na radeonech
					//else
					//	numInstances += INSTANCES_PER_PASS;
					areaLight->setNumInstances(numInstances);
					setupAreaLight();
				}
			}
			break;
		case '-':
			{
				unsigned numInstances = areaLight->getNumInstances();
				if(numInstances>1) 
				{
					if(numInstances>INSTANCES_PER_PASS) 
						numInstances -= INSTANCES_PER_PASS;
					else
						numInstances = 1;
					areaLight->setNumInstances(numInstances);
					setupAreaLight();
				}
			}
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
	ME_TOGGLE_WATER,
	ME_TOGGLE_INFO,
	ME_TOGGLE_CAPTURE_MOVIE,
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
		case ME_TOGGLE_WATER:
			level->pilot.setup->renderWater = !level->pilot.setup->renderWater;
			break;

		case ME_TOGGLE_INFO:
			renderInfo = !renderInfo;
			break;

		case ME_TOGGLE_CAPTURE_MOVIE:
			captureMovie = !captureMovie;
			break;

		case ME_TOGGLE_HELP:
			showHelp = 1-showHelp;
			break;

#if SUPPORT_LIGHTMAPS
		case ME_UPDATE_LIGHTMAPS_0_ENV:
			{
				// set lights
				//rr::RRLights lights;
				//lights.push_back(rr::RRLight::createPointLight(rr::RRVec3(1,1,1),rr::RRColorRGBF(0.5f))); //!!! not freed
				//lights.push_back(rr::RRLight::createDirectionalLight(rr::RRVec3(2,-5,1),rr::RRColorRGBF(0.7f))); //!!! not freed
				//level->solver->setLights(lights);
				// updates maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect;
				paramsDirect.applyCurrentSolution = 0;
				paramsDirect.applyLights = 1;
				paramsDirect.applyEnvironment = 1;
				paramsDirect.quality = LIGHTMAP_QUALITY;
				rr::RRDynamicSolver::UpdateParameters paramsIndirect;
				paramsIndirect.applyCurrentSolution = 0;
				paramsIndirect.applyLights = 1;
				paramsIndirect.applyEnvironment = 1;
				paramsIndirect.quality = LIGHTMAP_QUALITY/4;

				// update all objects
				level->solver->updateLightmaps(0,-1,true,&paramsDirect,&paramsIndirect);

				// stop updating maps in realtime, stay with what we computed here
				modeMovingEye = true;
				renderConstantAmbient = false;
				renderVertexColors = false;
				renderLightmaps = true;
			}
			break;

		case ME_UPDATE_LIGHTMAPS_0:
			{
				// updates maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect;
				paramsDirect.applyCurrentSolution = 1;
				paramsDirect.applyLights = 0;
				paramsDirect.applyEnvironment = 0;
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
				renderConstantAmbient = false;
				renderVertexColors = false;
				renderLightmaps = true;
			}
			break;

		case ME_UPDATE_LIGHTMAPS_ALL:
			{
				// updates all maps in high quality
				rr::RRDynamicSolver::UpdateParameters paramsDirect;
				paramsDirect.applyCurrentSolution = 1;
				paramsDirect.applyLights = 0;
				paramsDirect.applyEnvironment = 0;
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
					level->solver->updateVertexBuffers(layerNumber,-1,true,NULL,NULL);
					// update 1 lmap
					static unsigned obj=12;
					if(!level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer)
						level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer = ((rr_gl::RRDynamicSolverGL*)(level->solver))->createIlluminationPixelBuffer(512,512);
					level->solver->updateLightmap(obj,level->solver->getIllumination(obj)->getLayer(layerNumber)->pixelBuffer,NULL,&paramsDirect);
				}

				// stop updating maps in realtime, stay with what we computed here
				modeMovingEye = true;
				renderConstantAmbient = false;
				renderVertexColors = false;
				renderLightmaps = true;
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
					rr::RRIlluminationPixelBuffer* map = level->solver->getIllumination(objectIndex)->getLayer(0)->pixelBuffer;
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
					rr::RRObjectIllumination::Layer* illum = level->solver->getIllumination(objectIndex)->getLayer(0);
					rr::RRIlluminationPixelBuffer* loaded = static_cast<rr_gl::RRDynamicSolverGL*>(level->solver)->loadIlluminationPixelBuffer(filename);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete illum->pixelBuffer;
						illum->pixelBuffer = loaded;
					}
				}
				/*/ load all environment maps (dynamic objects)
				for(unsigned objectIndex=0;objectIndex<DYNAOBJECTS;objectIndex++)
				{
					// diffuse
					sprintf(filename,"export/cap%02d_dynobj%d_diff_%cs.png",captureIndex,objectIndex,'%');
					rr::RRIlluminationEnvironmentMap* loaded = level->solver->loadIlluminationEnvironmentMap(filename,cubeSideNames);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete dynaobjects->getObject(objectIndex)->diffuseMap;
						dynaobjects->getObject(objectIndex)->diffuseMap = loaded;
					}
					// specular
					sprintf(filename,"export/cap%02d_dynobj%d_spec_%cs.png",captureIndex,objectIndex,'%');
					loaded = level->solver->loadIlluminationEnvironmentMap(filename,cubeSideNames);
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
				renderConstantAmbient = false;
				renderVertexColors = false;
				renderLightmaps = true;
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
	glutAddMenuEntry("Toggle water",ME_TOGGLE_WATER);
	glutAddMenuEntry("Toggle info",ME_TOGGLE_INFO);
	glutAddMenuEntry("Toggle capture movie",ME_TOGGLE_CAPTURE_MOVIE);
	glutAddMenuEntry("Lightmaps update(rt light)", ME_UPDATE_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps update(env+lights)", ME_UPDATE_LIGHTMAPS_0_ENV);
	glutAddMenuEntry("Lightmaps save current", ME_SAVE_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps load current", ME_LOAD_LIGHTMAPS_0);
	glutAddMenuEntry("Lightmaps update(rt light) all frames", ME_UPDATE_LIGHTMAPS_ALL);
	glutAddMenuEntry("Lightmaps save all frames", ME_SAVE_LIGHTMAPS_ALL);
	glutAddMenuEntry("Lightmaps load all frames", ME_LOAD_LIGHTMAPS_ALL);
	glutAddMenuEntry("Scene previous", ME_PREVIOUS_SCENE);
	glutAddMenuEntry("Scene next", ME_NEXT_SCENE);
	glutAddMenuEntry("Toggle help",ME_TOGGLE_HELP);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	needMatrixUpdate = 1;

	if(!demoPlayer)
	{
		demoPlayer = new DemoPlayer(cfgFile,supportEditor);
		demoPlayer->setPaused(supportEditor);
		demoPlayer->setBigscreen(bigscreenCompensation);
	}
}

void mouse(int button, int state, int x, int y)
{
	if(level && state==GLUT_DOWN) level->pilot.reportInteraction();
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		modeMovingEye = !modeMovingEye;
	}
	if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
		//showImage(loadingMap);
		if(supportEditor)
			level->pilot.setup->save();
		//delete level;
		level = NULL;
		seekInMusicAtSceneSwap = true;
	}
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
		if(modeMovingEye)
		{
			currentFrame.eye.angle -= 0.005*x;
			currentFrame.eye.angleX -= 0.005*y;
			CLAMP(currentFrame.eye.angleX,-M_PI*0.49f,M_PI*0.49f);
			reportEyeMovement();
		}
		else
		{
			currentFrame.light.angle -= 0.005*x;
			currentFrame.light.angleX -= 0.005*y;
			CLAMP(currentFrame.light.angleX,-M_PI*0.49f,M_PI*0.49f);
			reportLightMovement();
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	if(!winWidth) return; // can't work without window

	if(level && level->pilot.isTimeToChangeLevel())
	{
		//showImage(loadingMap);
		//delete level;
		level = NULL;
		seekInMusicAtSceneSwap = false;
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
//#ifdef THREE_ONE
		if(captureMovie)
		{
			// advance by 1 frame of 30fps movie
			shotRequested = true;
			demoPlayer->advance(1.f/30);
		}
		else
		{
			// advance according to real time
			demoPlayer->advance();
		}
		if(level)
		{
			if(!demoPlayer->getDynamicObjects()->setupSceneDynamicForPartTime(level->pilot.setup, demoPlayer->getPartPosition()))
			{
				if(level->animationEditor)
				{
					// play scene finished, jump to editor
					demoPlayer->setPaused(true);
					level->animationEditor->frameCursor = MAX(1,level->pilot.setup->frames.size())-1;
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
//#else
//		dynaobjects->updateSceneDynamic(level->pilot.setup,seconds);
//#endif
	}
	prev = now;

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
		needDepthMapUpdate = 1;
		needRedisplay = 1;
	}

//	LIMITED_TIMES(1,timer.Start());
	bool rrOn = renderVertexColors
#ifdef CALCULATE_WHEN_PLAYING_PRECALCULATED_MAPS
		|| renderLightmaps
#endif
		;
//	printf("[--- %d %d %d %d",rrOn?1:0,movingEye?1:0,updateDuringLightMovement?1:0,movingLight?1:0);
	// pri kalkulaci nevznikne improve -> neni read results -> aplikace neda display -> pristi calculate je dlouhy
	// pokud se ale hybe svetlem, aplikace da display -> pristi calculate je kratky
	if(!level || (rrOn && level->solver->calculate()==rr::RRStaticSolver::IMPROVED) || needRedisplay || gameOn)
	{
//		printf("---]");
		// pokud pouzivame rr renderer a zmenil se indirect, promaznout cache
		// nutne predtim nastavit params (renderedChannels apod)
		//!!! rendererCaching->setStatus(RRGLCachingRenderer::CS_READY_TO_COMPILE);
		glutPostRedisplay();
//printf("coll=%.1fM coll/sec=%fk\n",rr::RRIntersectStats::getInstance()->intersect_mesh/1000000.0f,rr::RRIntersectStats::getInstance()->intersect_mesh/1000.0f/timer.Watch());//!!!
	}
}

void init_gl_states()
{
	GLint depthBits;

	glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	//printf("depth buffer precision = %d\n", depthBits);

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

void parseOptions(int argc, char **argv)
{
	int i;

	for (i=1; i<argc; i++)
	{
		if (strstr(argv[i],".cfg")) {
			cfgFile = argv[i];
		}
		if (!strcmp("editor", argv[i])) {
			supportEditor = 1;
			fullscreen = 0;
			showTimingInfo = 1;
		}
		if (!strcmp("bigscreen", argv[i])) {
			bigscreenCompensation = 1;
		}
		if (!strcmp("rx", argv[i])) {
			resolutionx = atoi(argv[++i]);
		}
		if (!strcmp("ry", argv[i])) {
			resolutiony = atoi(argv[++i]);
		}
		if (!strcmp("-window", argv[i])) {
			fullscreen = 0;
		}
		if (!strcmp("-fullscreen", argv[i])) {
			fullscreen = 1;
		}
		if (!strcmp("-hard", argv[i])) {
			startWithSoftShadows = 0;
		}
		if (!strcmp("-twosided", argv[i])) {
			twosided = 1;
		}
	}
}


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

	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	//rr::RRReporter::setFilter(true,3,true);
	//rr_gl::Program::showLog = true;

	parseOptions(argc, argv);

	// init GLUT
	glutInitWindowSize(resolutionx,resolutiony);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_ACCUM | GLUT_ALPHA); //accum na high quality soft shadows, alpha na filtrovani ambient map
	if(fullscreen)
	{
		char buf[100];
		sprintf(buf,"%dx%d:32",resolutionx,resolutiony);
		glutGameModeString(buf);
		glutEnterGameMode();
	}
	else
	{
		glutCreateWindow("Realtime Radiosity");
		unsigned w = glutGet(GLUT_SCREEN_WIDTH);
		unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
		glutPositionWindow((w-resolutionx)/2,(h-resolutiony)/2);
	}
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);
	glutSpecialFunc(special);
	glutSpecialUpFunc(specialUp);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);
	initMenu();

	// init GLEW
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	init_gl_states();
	const char* vendor = (const char*)glGetString(GL_VENDOR);
	const char* renderer = (const char*)glGetString(GL_RENDERER);
	ati = !vendor || !renderer || strstr(vendor,"ATI") || strstr(vendor,"AMD") || strstr(renderer,"Radeon");


	updateMatrices(); // needed for startup without area lights (areaLight doesn't update matrices for 1 instance)

	uberProgramGlobalSetup.SHADOW_MAPS = 1;
	uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
	uberProgramGlobalSetup.LIGHT_DIRECT = true;
	uberProgramGlobalSetup.LIGHT_DIRECT_MAP = true;
	uberProgramGlobalSetup.LIGHT_INDIRECT_CONST = renderConstantAmbient;
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = renderVertexColors;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
	uberProgramGlobalSetup.LIGHT_INDIRECT_auto = renderLightmaps;
	uberProgramGlobalSetup.LIGHT_INDIRECT_ENV = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE = true;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_CONST = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_VCOLOR = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramGlobalSetup.MATERIAL_SPECULAR = false;
	uberProgramGlobalSetup.MATERIAL_SPECULAR_CONST = false;
	uberProgramGlobalSetup.MATERIAL_SPECULAR_MAP = false;
	uberProgramGlobalSetup.MATERIAL_NORMAL_MAP = false;
	uberProgramGlobalSetup.MATERIAL_EMISSIVE_MAP = false;
	uberProgramGlobalSetup.OBJECT_SPACE = false;
	uberProgramGlobalSetup.FORCE_2D_POSITION = false;

	// init shaders
	// init textures
	init_gl_resources();

	// adjust INSTANCES_PER_PASS to GPU
#if SUPPORT_LIGHTMAPS
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = 0;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = 1;
#endif
	INSTANCES_PER_PASS = uberProgramGlobalSetup.detectMaxShadowmaps(uberProgram,argc,argv);
#if SUPPORT_LIGHTMAPS
	uberProgramGlobalSetup.LIGHT_INDIRECT_VCOLOR = renderVertexColors;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
#endif
	if(!INSTANCES_PER_PASS) error("",true);
	areaLight->setNumInstances(startWithSoftShadows?INSTANCES_PER_PASS:1);

	if(rr::RRLicense::loadLicense("licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.",false);

	glutMainLoop();
	return 0;
}
