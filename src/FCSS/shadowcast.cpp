#define BUGS
#define MAX_INSTANCES              50  // max number of light instances aproximating one area light
#define MAX_INSTANCES_PER_PASS     10
unsigned INSTANCES_PER_PASS = 6; // 5 je max pro X800pro, 6 je max pro 6150, 7 je max pro 6600
#define INITIAL_INSTANCES_PER_PASS INSTANCES_PER_PASS
#define INITIAL_PASSES             1
#define PRIMARY_SCAN_PRECISION     1 // 1nejrychlejsi/2/3nejpresnejsi, 3 s texturami nebude fungovat kvuli cachovani pokud se detekce vseho nevejde na jednu texturu - protoze displaylist myslim neuklada nastaveni textur
int fullscreen = 1;
bool renderer3ds = true;
bool updateDuringLightMovement = 1;
bool startWithSoftShadows = 1;
bool singlecore = 0;
/*
do eg
-prodlouzit prvni vypocet ve sponze po resetu, na rychly karte je moc rychly
-zlepsit chovani bugu aby sly pustit ve sponze
-plynule rekace na sipky, ne pauza po prvnim stisku
-spatne vyrenderovany uvodni loading

vypisovat kolik % casu
 -detect&resetillum
 -improve
 -readresults
 -game render
 -idle

dalsi announcementy

mailnout do limy
napsat zadani na final gather

sehnat bankovni spojeni
zmerit memory leaky

co jeste pomuze:
30% za 3 dny: detect+reset po castech, kratsi improve
1% za 4h: presunout kanaly z objektu do meshe
20% za 8 dnu:
 thread0: renderovat prechod mezi kanalem 0 a 1 podle toho v jake fazi je thread1
 thread1: vlastni gl kontext a nekonecny cyklus: detekce, update, 0.2s vypoctu, read results do kanalu k, k=1-k

casy:
80 detect primary
110 reset energies .. spadne na 80 kdyz se neresetuje propagace, to ale jeste nefunguje
80 improve
60 read results
50 user - render

!kdyz nenactu textury a vse je bile, vypocet se velmi rychle zastavi, mozna distribuuje ale nerefreshuje

! sponza v rr renderu je spatne vysmoothovana
  - spis je to vlastnost stavajiciho smoothovani, ne chyba
  - smoothovat podle normal
  - vertex blur

rr renderer: pridat indirect mapu

ovladani jasu (global, indirect)
nacitat jpg

pri 2 instancich je levy okraj spotmapy oriznuty
 pri arealight by spotmapa potrebovala malinko mensi fov nez shadowmapy aby nezabirala mista kde konci stin

autodeteknout zda mam metry nebo centimetry
dodelat podporu pro matice do 3ds2rr importeru
kdyz uz by byl korektni model s gammou, pridat ovladani gammy

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce (komplikovane pri multipass renderu)
scita se primary a zkorigovany indirect, vysledkem je ze primo osvicena mista jsou svetlejsi nez maji byt
*/

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <list>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/glut.h>
#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Timer.h"
#include "DemoEngine/Camera.h"
#include "DemoEngine/Texture.h"
#include "DemoEngine/UberProgram.h"
#include "DemoEngine/MultiLight.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/3ds2rr.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/RendererOfRRObject.h"
#include "DemoEngine/UberProgramSetup.h"
#include "Bugs.h"
#include "LevelSequence.h"

using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
#define CLAMP(a,min,max) (a)=(((a)<(min))?min:(((a)>(max)?(max):(a))))


/////////////////////////////////////////////////////////////////////////////

/* Draw modes. */
enum {
	DM_EYE_VIEW_SHADOWED,
	DM_EYE_VIEW_SOFTSHADOWED,
};



/////////////////////////////////////////////////////////////////////////////
//
// Level

class Level
{
public:
	Model_3DS m3ds;
	class MyApp* app;
	class Bugs* bugs;
	RendererOfRRObject* rendererNonCaching;
	RendererWithCache* rendererCaching;

	Level(const char* filename_3ds);
	~Level();
};


/////////////////////////////////////////////////////////////////////////////
//
// globals

Camera eye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
Camera light = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,20.};
GLUquadricObj *quadric;
AreaLight* areaLight = NULL;
#define lightDirectMaps 3
Texture* lightDirectMap[lightDirectMaps];
unsigned lightDirectMapIdx = 0;
Texture* loadingMap = NULL;
Texture* hintMap = NULL;
Program *ambientProgram;
UberProgram* uberProgram;
UberProgramSetup uberProgramGlobalSetup;
int winWidth, winHeight;
int depthBias24 = 42;
int depthScale24;
GLfloat slopeScale = 4.0;
int needDepthMapUpdate = 1;
int wireFrame = 0;
int needMatrixUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
bool showHelp = 0;
bool showHint = 0;
int showLightViewFrustum = 0;
bool paused = 0;
int resolutionx = 640;
int resolutiony = 480;
bool modeMovingEye = true;
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
LevelSequence levelSequence;


/////////////////////////////////////////////////////////////////////////////

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nTry upgrading drivers for your graphics card.\nIf it doesn't help, your graphics card may be too old.\nCards known to work: GEFORCE 6xxx/7xxx, RADEON 95xx+/Xxxx/X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

void updateDepthBias(int delta)
{
	depthBias24 += delta;
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needDepthMapUpdate = 1;
}

void updateDepthScale(void)
{
	GLint shadowDepthBits = TextureShadowMap::getDepthBits();
	if (shadowDepthBits < 24) {
		depthScale24 = 1;
	} else {
		depthScale24 = 1 << (shadowDepthBits - 24+8); // "+8" hack: on gf6600, 24bit depth seems to need +8 scale
	}
	needDepthMapUpdate = 1;
}

void init_gl_resources()
{
	quadric = gluNewQuadric();

	areaLight = new AreaLight(MAX_INSTANCES);

	// update states, but must be done after initing shadowmaps (inside arealight)
	updateDepthScale();
	updateDepthBias(0);  /* Update with no offset change. */

	for(unsigned i=0;i<lightDirectMaps;i++)
	{
		char name[]="maps\\spot0.tga";
		name[9] = '0'+i;
		lightDirectMap[i] = Texture::load(name, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		if(!lightDirectMap[i])
		{
			printf("Texture %s not found or not supported (supported = truecolor .tga).\n",name);
			error("",false);
		}
	}
	loadingMap = Texture::load("maps\\rrbugs_loading.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	hintMap = Texture::load("maps\\rrbugs_hint.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);

	uberProgram = new UberProgram("shaders\\ubershader.vp", "shaders\\ubershader.fp");
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
	ambientProgram = uberProgram->getProgram(uberProgramSetup.getSetupString());

	if(!ambientProgram)
		error("\nFailed to compile or link GLSL program.\n",true);
}

void done_gl_resources()
{
	//delete ambientProgram;
	delete uberProgram;
	delete loadingMap;
	delete hintMap;
	for(unsigned i=0;i<lightDirectMaps;i++) delete lightDirectMap[i];
	delete areaLight;
	gluDeleteQuadric(quadric);
}

/////////////////////////////////////////////////////////////////////////////
//
// MyApp

void renderScene(UberProgramSetup uberProgramSetup, unsigned firstInstance);
void updateMatrices();
void updateDepthMap(unsigned mapIndex,unsigned mapIndices);

// generuje uv coords pro capture
class CaptureUv : public VertexDataGenerator
{
public:
	virtual ~CaptureUv() {};
	virtual void generateData(unsigned triangleIndex, unsigned vertexIndex, void* vertexData, unsigned size) // vertexIndex=0..2
	{
		if(!xmax || !ymax)
		{
			error("No window, internal error.",false);
		}
		((GLfloat*)vertexData)[0] = ((GLfloat)((triangleIndex-firstCapturedTriangle)/ymax)+((vertexIndex<2)?0:1)-xmax/2)/(xmax/2);
		((GLfloat*)vertexData)[1] = ((GLfloat)((triangleIndex-firstCapturedTriangle)%ymax)+1-(vertexIndex%2)-ymax/2)/(ymax/2);
	}
	unsigned firstCapturedTriangle;
	unsigned xmax, ymax;
};

// external dependencies of MyApp:
// z m3ds detekuje materialy
// renderer je pouzit k captureDirect
//#include "Timer.h"
class MyApp : public rr::RRRealtimeRadiosity
{
protected:
	virtual void detectMaterials()
	{
	}
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!level || !level->rendererCaching) return false;

		// first time illumination is detected, no shadowmap has been created yet
		if(needDepthMapUpdate)
		{
			updateDepthMap(0,0);
			needDepthMapUpdate = 1; // aby si pote soft pregeneroval svych 7 map a nespolehal na nasi jednu
		}
		
		//Timer w;w.Start();

		rr::RRMesh* mesh = multiObject->getCollider()->getMesh();
		unsigned numTriangles = mesh->getNumTriangles();

		// adjust captured texture size so we don't waste pixels
		static CaptureUv captureUv;
		unsigned width1 = 4;
		unsigned height1 = 4;
		captureUv.firstCapturedTriangle = 0;
		captureUv.xmax = winWidth/width1;
		captureUv.ymax = winHeight/height1;
		while(captureUv.xmax && numTriangles/(captureUv.xmax*captureUv.ymax)==numTriangles/((captureUv.xmax-1)*captureUv.ymax)) captureUv.xmax--;
		unsigned width = captureUv.xmax*width1;
		unsigned height = captureUv.ymax*height1;

		// setup render states
		glClearColor(0,0,0,1);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);
		glViewport(0, 0, width, height);

		// Allocate the index buffer memory as necessary.
		GLuint* pixelBuffer = new GLuint[width * height];

		//printf("%d %d\n",numTriangles,captureUv.xmax*captureUv.ymax);
		for(captureUv.firstCapturedTriangle=0;captureUv.firstCapturedTriangle<numTriangles;captureUv.firstCapturedTriangle+=captureUv.xmax*captureUv.ymax)
		{
			// clear
			glClear(GL_COLOR_BUFFER_BIT);

			// render scene
			UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = true;
			//uberProgramSetup.LIGHT_DIRECT_MAP = ;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
			uberProgramSetup.LIGHT_INDIRECT_MAP = false;
#if PRIMARY_SCAN_PRECISION==1 // 110ms
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
#elif PRIMARY_SCAN_PRECISION==2 // 150ms
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = true;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
#else // PRIMARY_SCAN_PRECISION==3 // 220ms
			uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
#endif
			uberProgramSetup.FORCE_2D_POSITION = true;
			level->rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup,0);
			level->rendererNonCaching->setCapture(NULL,0);

			// Read back the index buffer to memory.
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixelBuffer);

			// dbg print
			//rr::RRColor suma = rr::RRColor(0);

			// accumulate triangle powers
			for(unsigned triangleIndex=captureUv.firstCapturedTriangle;triangleIndex<MIN(numTriangles,captureUv.firstCapturedTriangle+captureUv.xmax*captureUv.ymax);triangleIndex++)
			{
				// accumulate 1 triangle power
				unsigned sum[3] = {0,0,0};
				unsigned i = (triangleIndex-captureUv.firstCapturedTriangle)/(height/height1);
				unsigned j = triangleIndex%(height/height1);
				for(unsigned n=0;n<height1;n++)
					for(unsigned m=0;m<width1;m++)
					{
						unsigned pixel = width*(j*height1+n) + (i*width1+m);
						unsigned color = pixelBuffer[pixel] >> 8; // alpha was lost
						sum[0] += color>>16;
						sum[1] += (color>>8)&255;
						sum[2] += color&255;
					}
				// pass power to rrobject
				rr::RRColor avg = rr::RRColor(sum[0],sum[1],sum[2]) / (255*width1*height1/2);
#if PRIMARY_SCAN_PRECISION==1
				multiObject->setTriangleAdditionalMeasure(triangleIndex,rr::RM_IRRADIANCE,avg);
#else
				multiObject->setTriangleAdditionalMeasure(triangleIndex,rr::RM_EXITANCE,avg);
#endif

				// debug print
				//rr::RRColor tmp = rr::RRColor(0);
				//multiObject->getTriangleAdditionalMeasure(triangleIndex,rr::RM_EXITING_FLUX,tmp);
				//suma+=tmp;
			}
			//printf("sum = %f/%f/%f\n",suma[0],suma[1],suma[2]);
		}

		delete[] pixelBuffer;

		// restore render states
		glViewport(0, 0, winWidth, winHeight);
		glDepthMask(1);
		glEnable(GL_DEPTH_TEST);
		//printf("primary scan (%d-pass (%f)) took............ %d ms\n",numTriangles/(captureUv.xmax*captureUv.ymax)+1,(float)numTriangles/(captureUv.xmax*captureUv.ymax),(int)(1000*w.Watch()));
		return true;
	}
	virtual void reportAction(const char* action) const
	{
		printf(action);
	}
public:
	virtual ~MyApp()
	{
		// delete objects and illumination
		for(unsigned i=0;i<getNumObjects();i++)
		{
			delete getIllumination(i);
			delete getObject(i);
		}
	}
};


/////////////////////////////////////////////////////////////////////////////

/* drawLight - draw a yellow sphere (disabling lighting) to represent
   the current position of the local light source. */
void drawLight(void)
{
	ambientProgram->useIt();
	glPushMatrix();
	glTranslatef(light.pos[0]-0.3*light.dir[0], light.pos[1]-0.3*light.dir[1], light.pos[2]-0.3*light.dir[2]);
	glColor3f(1,1,0);
	gluSphere(quadric, 0.05, 10, 10);
	glPopMatrix();
}

void updateMatrices(void)
{
	eye.aspect = winHeight ? (float) winWidth / (float) winHeight : 1;
	eye.update(0);
	light.update(0.3f);
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
	glMultMatrixd(light.inverseViewMatrix);
	glMultMatrixd(light.inverseFrustumMatrix);
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

void renderScene(UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	if(!level) return;
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap[lightDirectMapIdx]))
		error("Failed to compile or link GLSL program.\n",true);

	// lze smazat, stejnou praci dokaze i rrrenderer
	// nicmene m3ds.Draw stale jeste
	// 1) lip smoothuje (pouziva min vertexu)
	// 2) slouzi jako test ze RRRealtimeRadiosity spravne generuje vertex buffer s indirectem
	// 3) nezpusobuje 0.1sec zasek pri kazdem pregenerovani displaylistu
	// 4) muze byt v malym rozliseni nepatrne rychlejsi (pouziva min vertexu)
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		level->m3ds.Draw(uberProgramSetup.LIGHT_INDIRECT_COLOR?level->app:NULL,uberProgramSetup.LIGHT_INDIRECT_MAP);
		return;
	}
	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
	level->rendererNonCaching->setRenderedChannels(renderedChannels);
	level->rendererCaching->render();
}

void updateDepthMap(unsigned mapIndex,unsigned mapIndices)
{
	if(!needDepthMapUpdate) return;

	assert(mapIndex>=0);
	Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;

	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	areaLight->getShadowMap((mapIndex>=0)?mapIndex:0)->renderingToInit();
	glClearDepth(0.999999); // prevents backprojection, tested on nvidia geforce 6600
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);

	UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
	uberProgramSetup.SHADOW_MAPS = 0;
	uberProgramSetup.SHADOW_SAMPLES = 0;
	uberProgramSetup.LIGHT_DIRECT = false;
	uberProgramSetup.LIGHT_DIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
	uberProgramSetup.FORCE_2D_POSITION = false;
	renderScene(uberProgramSetup,0);
	areaLight->getShadowMap((mapIndex>=0)?mapIndex:0)->renderingToDone();

	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);


	if(mapIndex==mapIndices-1) 
	{
		needDepthMapUpdate = 0;
	}
}

void drawEyeViewShadowed(UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	if(!level) return;
	glClear(GL_COLOR_BUFFER_BIT);
	if(firstInstance==0) glClear(GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	eye.setupForRender();

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
		if(!paused) level->bugs->tick(seconds);
		level->bugs->render();
	}
#endif

	drawLight();
	if (showLightViewFrustum) drawShadowMapFrustum();
}

void drawEyeViewSoftShadowed(void)
{
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++)
	{
		updateDepthMap(i,numInstances);
	}
	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// optimized path without accum, only for m3ds, rrrenderer can't render both materialColor and indirectColor
	if(numInstances<=INSTANCES_PER_PASS && uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP)
	{
		UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = numInstances;
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,0);
		return;
	}

	glClear(GL_ACCUM_BUFFER_BIT);

	// add direct
	for(unsigned i=0;i<numInstances;i+=INSTANCES_PER_PASS)
	{
		UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = MIN(INSTANCES_PER_PASS,numInstances);
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,i);
		glAccum(GL_ACCUM,1./(numInstances/MIN(INSTANCES_PER_PASS,numInstances)));
	}
	// add indirect
	{
		UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.LIGHT_DIRECT = false;
		uberProgramSetup.LIGHT_DIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		uberProgramSetup.FORCE_2D_POSITION = false;
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderScene(uberProgramSetup,0);
		glAccum(GL_ACCUM,1);
	}

	glAccum(GL_RETURN,1);
}

static void output(int x, int y, char *string)
{
	glRasterPos2i(x, y);
	int len = (int) strlen(string);
	for(int i = 0; i < len; i++)
	{
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, string[i]);
	}
}

static void drawHelpMessage(bool big)
{
	if(!big && gameOn) return;

	static char *message[] = {
#ifdef BUGS
		"Realtime Radiosity Bugs",
#else
		"Realtime Radiosity Viewer",
#endif
		" Stepan Hrbek, http://dee.cz",
		" radiosity engine, http://lightsprint.com",
		"",
		"Purpose:",
		" Show radiosity integration into arbitrary interactive 3d app",
		" using NO PRECALCULATIONS.",
#ifndef BUGS
		" Demos using precalculated data are coming.",
#endif
		"",
		"Controls:",
		" mouse            - look",
		" arrows/wsad/1235 - move",
		" left button      - switch between camera/light",
		" right button     - next scene",
		" F5               - hint",
		"",
		"Extras for experts:",
		" space - toggle global illumination",
		" 'z/Z' - zoom in/out",
		" '+ -' - increase/decrease penumbra (soft shadow) precision",
		" '* /' - increase/decrease penumbra (soft shadow) smoothness",
		" 'l'   - toggle lazy updates",
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
		"'C'   - expand shadow frustum far clip plane",
		"'b'   - increment the depth bias for 1st pass glPolygonOffset",
		"'B'   - decrement the depth bias for 1st pass glPolygonOffset",
		"'q'   - increment depth slope for 1st pass glPolygonOffset",
		"'Q'   - increment depth slope for 1st pass glPolygonOffset",*/
		NULL,
		"F1 - help",
		NULL
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
	if(big) glRecti(winWidth - 30, 30, 30, winHeight - 30);

	glDisable(GL_BLEND);

	glColor3f(1,1,1);
	for(i=0; message[i] != NULL; i++) 
	{
		if(!big) i=sizeof(message)/sizeof(char*)-2;
		if (message[i][0] != '\0')
		{
			output(x, y, message[i]);
		}
		y += 18;
	}

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
}

void showImage(const Texture* tex)
{
	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	tex->bindTexture();
	glDisable(GL_CULL_FACE);
	glColor3f(1,1,1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glBegin(GL_POLYGON);
	glTexCoord2f(0,0);
	glVertex2f(-1,-1);
	glTexCoord2f(1,0);
	glVertex2f(1,-1);
	glTexCoord2f(1,1);
	glVertex2f(1,1);
	glTexCoord2f(0,1);
	glVertex2f(-1,1);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glutSwapBuffers();
}


/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(const char* filename_3ds)
{
	app = NULL;
	bugs = NULL;
	rendererNonCaching = NULL;
	rendererCaching = NULL;

	float scale_3ds = 1;
	{
		Camera tmpeye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
		Camera tmplight = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,20.};
		eye = tmpeye;
		light = tmplight;
	}
	if(strstr(filename_3ds, "koupelna4")) {
		scale_3ds = 0.03f;
		//Camera koupelna4_eye = {{0.032202,1.659255,1.598609},10.010005,-0.150000};
		//Camera koupelna4_light = {{-1.309976,0.709500,0.498725},3.544996,-10.000000};
		Camera koupelna4_eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
		Camera koupelna4_light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
		//Camera koupelna4_eye = {{0.823,1.500,-0.672},11.055,-0.050,1.3,100.0,0.3,60.0};//wrong backprojection
		//Camera koupelna4_light = {{-1.996,0.257,-2.205},0.265,-1.000,1.0,70.0,1.0,20.0};
		eye = koupelna4_eye;
		light = koupelna4_light;
		updateDuringLightMovement = 1;
		if(areaLight) areaLight->setNumInstances(INSTANCES_PER_PASS);
	}
	if(strstr(filename_3ds, "koupelna3")) {
		scale_3ds = 0.01f;
		//lightDirectMapIdx = 1;
		//Camera tmpeye = {{0.822,1.862,6.941},9.400,-0.450,1.3,50.0,0.3,60.0};
		//Camera tmplight = {{1.906,1.349,1.838},4.085,0.700,1.0,70.0,1.0,20.0};
		Camera tmpeye = {{0.822,1.862,6.941},9.400,-0.450,1.3,50.0,0.3,60.0};
		Camera tmplight = {{1.906,1.349,1.838},3.930,0.100,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{6.172,3.741,1.522},4.370,1.150,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{-2.825,4.336,3.259},-4.315,3.850,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{6.031,3.724,1.471},4.350,1.450,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{-0.718,4.366,-0.145},-12.175,6.550,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
		updateDuringLightMovement = 1;
		if(areaLight) areaLight->setNumInstances(INSTANCES_PER_PASS);
	}
	if(strstr(filename_3ds, "koupelna5")) {
		scale_3ds = 0.03f;
		//Camera tmpeye = {{6.172,3.741,1.522},4.340,1.600,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		Camera tmpeye = {{5.735,2.396,1.479},4.405,0.100,1.3,100.0,0.3,60.0};
		Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
		updateDuringLightMovement = 1;
		if(areaLight) areaLight->setNumInstances(1);
	}
	if(strstr(filename_3ds, "sponza"))
	{
		Camera sponza_eye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999, 1.,100.,0.3,60.};
		Camera sponza_light = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001, 1.,70.,1.,30.};
		//Camera sponza_eye = {{-10.407576,1.605258,4.050256},7.859994,-0.050000};
		//Camera sponza_light = {{-7.109047,5.130751,-2.025017},0.404998,2.950001};
		//Camera sponza_eye = {{13.924,7.606,1.007},7.920,-0.150,1.3,100.0,0.3,60.0};// shows face with bad indirect
		//Camera sponza_light = {{-8.042,7.690,-0.954},1.990,0.800,1.0,70.0,1.0,30.0};
		eye = sponza_eye;
		light = sponza_light;
		updateDuringLightMovement = 0;
		if(areaLight) areaLight->setNumInstances(1);
	}
	if(strstr(filename_3ds, "sibenik"))
	{
		// zacatek nevhodny pouze kvuli spatnym normalam
		//		Camera tmpeye = {{-8.777,3.117,0.492},1.145,-0.400,1.3,50.0,0.3,80.0};
		//		Camera tmplight = {{-0.310,2.952,-0.532},5.550,3.200,1.0,70.0,1.0,40.0};
		// dalsi zacatek kde je videt zmrsena normala
		Camera tmpeye = {{-3.483,5.736,2.755},7.215,2.050,1.3,50.0,0.3,80.0};
		Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
		// detail vadne normaly
		//Camera tmpeye = {{3.078,5.093,4.675},7.995,-1.700,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
		// na screenshoty
		//Camera tmpeye = {{-8.777,3.117,0.492},1.630,-13.000,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-0.310,2.952,-0.532},4.670,-13.000,1.0,70.0,1.0,40.0};
		// kandidati na init
		//Camera tmpeye = {{-3.768,5.543,2.697},7.380,-0.050,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
		//Camera tmpeye = {{-5.876,10.169,6.378},8.110,7.150,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-1.872,5.494,0.481},0.550,0.500,1.0,70.0,1.6,40.0};
		// shot
		//Camera tmpeye = {{12.158,-0.433,-3.413},11.835,-9.850,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{21.907,0.387,1.443},-7.395,-4.450,1.0,70.0,1.0,40.0};
		//Camera tmpeye = {{-10.518,8.224,-0.298},7.820,-0.100,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-1.455,12.912,-2.926},13.130,13.000,1.0,70.0,1.6,40.0};
		eye = tmpeye;
		light = tmplight;
		updateDuringLightMovement = 0;
		if(areaLight) areaLight->setNumInstances(1);
	}

	printf("Loading %s...",filename_3ds);

	// init .3ds scene
	if(!m3ds.Load(filename_3ds,scale_3ds))
		error("",false);

	//	printf(app->getObject(0)->getCollider()->getMesh()->save("c:\\a")?"saved":"not saved");
	//	printf(app->getObject(0)->getCollider()->getMesh()->load("c:\\a")?" / loaded":" / not loaded");

	printf("\n");

	// init radiosity solver
	app = new MyApp();
	new_3ds_importer(&m3ds,app,0.01f);
	app->calculate(); // creates radiosity solver with multiobject. without renderer, no primary light is detected
	if(!app->getMultiObject())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new RendererOfRRObject(app->getMultiObject(),app->getScene());
	rendererCaching = new RendererWithCache(rendererNonCaching);
	// next calculate will use renderer to detect primary illum. must be called from mainloop, we don't know winWidth/winHeight yet

	// init bugs
#ifdef BUGS
	bugs = Bugs::create(app->getScene(),app->getMultiObject(),100);
#endif

	needMatrixUpdate = true;
	needDepthMapUpdate = true;
	needRedisplay = true;
}

Level::~Level()
{
	delete bugs;
	delete rendererCaching;
	delete rendererNonCaching;
	delete app;
}


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if(!winWidth) return; // can't work without window
	if(!level)
	{
		showImage(loadingMap);
		level = new Level(levelSequence.getNextLevel());
	}
	if(showHint)
	{
		showImage(hintMap);
		return;
	}

	level->app->reportIlluminationUse();

	if(needMatrixUpdate)
		updateMatrices();

	switch(drawMode)
	{
		case DM_EYE_VIEW_SHADOWED:
			{
				updateDepthMap(0,0);
				UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
				uberProgramSetup.SHADOW_MAPS = 1;
				uberProgramSetup.SHADOW_SAMPLES = 1;
				uberProgramSetup.LIGHT_DIRECT = true;
				uberProgramSetup.LIGHT_DIRECT_MAP = true;
				uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
				uberProgramSetup.LIGHT_INDIRECT_MAP = false;
				//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
				//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
				uberProgramSetup.FORCE_2D_POSITION = false;
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				drawEyeViewShadowed(uberProgramSetup,0);
				break;
			}
		case DM_EYE_VIEW_SOFTSHADOWED:
			drawEyeViewSoftShadowed();
			break;
		default:
			assert(0);
			break;
	}

	if(wireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	drawHelpMessage(showHelp);

	glutSwapBuffers();
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

void toggleGlobalIllumination()
{
	if(drawMode == DM_EYE_VIEW_SHADOWED)
		drawMode = DM_EYE_VIEW_SOFTSHADOWED;
	else
		drawMode = DM_EYE_VIEW_SHADOWED;
	needDepthMapUpdate = 1;
}

void changeSpotlight()
{
	lightDirectMapIdx = (lightDirectMapIdx+1)%lightDirectMaps;
	//light.fieldOfView = 50+40.0*rand()/RAND_MAX;
	needDepthMapUpdate = 1;
	if(!level) return;
	level->app->reportLightChange(true);
	level->app->reportEndOfInteractions(); // force update even in movingEye mode
}

void reportEyeMovement()
{
	if(!level) return;
	if(singlecore) level->app->reportCriticalInteraction();
	needMatrixUpdate = 1;
	needRedisplay = 1;
	movingEye = 4;
}

void reportEyeMovementEnd()
{
	if(!level) return;
	if(singlecore) level->app->reportEndOfInteractions();
	movingEye = 0;
}

void reportLightMovement()
{
	if(!level) return;
	if(updateDuringLightMovement)
	{
		// Behem pohybu svetla v male scene dava lepsi vysledky update (false)
		//  scena neni behem pohybu tmavsi, setrvacnost je neznatelna.
		// Ve velke scene dava lepsi vysledky reset (true),
		//  scena sice behem pohybu ztmavne,
		//  pri false je ale velka setrvacnost, nekdy dokonce stary indirect vubec nezmizi.
		level->app->reportLightChange(level->app->getMultiObject()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
	}
	else
	{
		if(singlecore) level->app->reportCriticalInteraction();
	}
	needDepthMapUpdate = 1;
	needMatrixUpdate = 1;
	needRedisplay = 1;
	movingLight = 3;
}

void reportLightMovementEnd()
{
	if(!level) return;
	if(singlecore) level->app->reportEndOfInteractions();
	if(!updateDuringLightMovement)
	{
		level->app->reportLightChange(true);
		needDepthMapUpdate = 1;
		needMatrixUpdate = 1;
		needRedisplay = 1;
	}
	movingLight = 0;
}

void special(int c, int x, int y)
{
	if(showHint)
	{
		showHint = false;
		return;
	}
	Camera* cam = modeMovingEye?&eye:&light;
	Camera::Move move = NULL;
	switch (c) 
	{
		case GLUT_KEY_F1:
			showHelp = !showHelp;
			break;
		case GLUT_KEY_F5:
			showHint = 1;
			break;
		case GLUT_KEY_F10:
			exit(0);
			break;
		case GLUT_KEY_F9:
			printf("\nCamera tmpeye = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",eye.pos[0],eye.pos[1],eye.pos[2],eye.angle,eye.height,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			printf("Camera tmplight = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",light.pos[0],light.pos[1],light.pos[2],light.angle,light.height,light.aspect,light.fieldOfView,light.anear,light.afar);
			return;

		case GLUT_KEY_UP:
			move = &Camera::moveForward;
			break;
		case GLUT_KEY_DOWN:
			move = &Camera::moveBack;
			break;
		case GLUT_KEY_LEFT:
			move = &Camera::moveLeft;
			break;
		case GLUT_KEY_RIGHT:
			move = &Camera::moveRight;
			break;

		default:
			return;
	}
	if(move)
	{
		int modif = glutGetModifiers();
		float scale = 1;
		if(modif&GLUT_ACTIVE_SHIFT) scale=10;
		if(modif&GLUT_ACTIVE_CTRL) scale=3;
		if(modif&GLUT_ACTIVE_ALT) scale=0.1f;
		#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
		CALL_MEMBER_FN(*cam,move)(0.05f*scale);
		if(cam==&light) reportLightMovement(); else reportEyeMovement();
	}
	glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
	if(showHint)
	{
		showHint = false;
		return;
	}
	switch (c)
	{
		case 27:
			delete level; // throws assert in freeing node from ivertex
			done_gl_resources();
			exit(0);
			break;
		case 9:
			modeMovingEye = !modeMovingEye;
			break;
		case 'p':
			paused = !paused;
			break;
		case '1':
		case 'a':
		case 'A':
			special(GLUT_KEY_LEFT,0,0);
			break;
		case '2':
		case 's':
		case 'S':
			special(GLUT_KEY_DOWN,0,0);
			break;
		case '3':
		case 'd':
		case 'D':
			special(GLUT_KEY_RIGHT,0,0);
			break;
		case '5':
		case 'w':
		case 'W':
			special(GLUT_KEY_UP,0,0);
			break;

		case 'l':
			updateDuringLightMovement = !updateDuringLightMovement;
			break;
		case 'f':
		case 'F':
			showLightViewFrustum = !showLightViewFrustum;
			if (showLightViewFrustum) {
				needMatrixUpdate = 1;
			}
			break;
		case 'Z':
			if(eye.fieldOfView<100) eye.fieldOfView+=25;
			needMatrixUpdate = true;
			break;
		case 'z':
			if(eye.fieldOfView>25) eye.fieldOfView -= 25;
			needMatrixUpdate = true;
			break;
			/*
		case 'a':
			++areaLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		case 's':
			changeSpotlight();
			break;
		case 'S':
			app->reportLightChange(true);
			break;
		case 'w':
		case 'W':
			toggleWireFrame();
			return;
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
		case 'p':
			light.fieldOfView -= 5.0;
			if (light.fieldOfView < 5.0) {
				light.fieldOfView = 5.0;
			}
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;
		case 'P':
			light.fieldOfView += 5.0;
			if (light.fieldOfView > 160.0) {
				light.fieldOfView = 160.0;
			}
			needMatrixUpdate = 1;
			needDepthMapUpdate = 1;
			break;*/
		case ' ':
			toggleGlobalIllumination();
			break;
		case 't':
			uberProgramGlobalSetup.MATERIAL_DIFFUSE_COLOR = !uberProgramGlobalSetup.MATERIAL_DIFFUSE_COLOR;
			uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = !uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP;
			break;
		case 'r':
			renderer3ds = !renderer3ds;
			break;
		case '*':
			if(uberProgramGlobalSetup.SHADOW_SAMPLES<8) uberProgramGlobalSetup.SHADOW_SAMPLES *= 2;
			//	else uberProgramGlobalSetup.SHADOW_SAMPLES=9;
			break;
		case '/':
			//if(uberProgramGlobalSetup.SHADOW_SAMPLES==9) uberProgramGlobalSetup.SHADOW_SAMPLES=8; else
			if(uberProgramGlobalSetup.SHADOW_SAMPLES>1) uberProgramGlobalSetup.SHADOW_SAMPLES /= 2;
			break;
		case '+':
			{
				unsigned numInstances = areaLight->getNumInstances();
				if(numInstances+INSTANCES_PER_PASS<=MAX_INSTANCES) 
				{
					if(numInstances==1 && numInstances<INSTANCES_PER_PASS)
						numInstances = INSTANCES_PER_PASS;
					else
						numInstances += INSTANCES_PER_PASS;
					needDepthMapUpdate = 1;
					areaLight->setNumInstances(numInstances);
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
					needDepthMapUpdate = 1;
					areaLight->setNumInstances(numInstances);
				}
			}
			break;
		default:
			return;
	}
	glutPostRedisplay();
}

void reshape(int w, int h)
{
	// nevim proc ale funguje i s mensim oknem
	//if(w<512 || h<512) LIMITED_TIMES(5,printf("Window size<512 not supported in this demo, expect incorrect render!\n"));

	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	needMatrixUpdate = true;

	/* Perhaps there might have been a mode change so at window
	reshape time, redetermine the depth scale. */
	updateDepthScale();
}

void mouse(int button, int state, int x, int y)
{
	if(showHint)
	{
		showHint = false;
		return;
	}
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		modeMovingEye = !modeMovingEye;
	}
	if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
		showImage(loadingMap);
		delete level;
		level = NULL;
	}
}

void passive(int x, int y)
{
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		if(modeMovingEye)
		{
			eye.angle = eye.angle - 0.005*x;
			eye.height = eye.height + 0.15*y;
			CLAMP(eye.height,-13,13);
			reportEyeMovement();
		}
		else
		{
			light.angle = light.angle - 0.005*x;
			light.height = light.height + 0.15*y;
			CLAMP(light.height,-13,13);
			reportLightMovement();
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	if(!winWidth) return; // can't work without window
	if(movingEye && !--movingEye)
	{
		reportEyeMovementEnd();
	}
	if(movingLight && !--movingLight)
	{
		reportLightMovementEnd();
	}
//	LIMITED_TIMES(1,timer.Start());
	bool rrOn = drawMode == DM_EYE_VIEW_SOFTSHADOWED;
//	printf("[--- %d %d %d %d",rrOn?1:0,movingEye?1:0,updateDuringLightMovement?1:0,movingLight?1:0);
	// pri kalkulaci nevznikne improve -> neni read results -> aplikace neda display -> pristi calculate je dlouhy
	// pokud se ale hybe svetlem, aplikace da display -> pristi calculate je kratky
	if(!level || (rrOn && level->app->calculate()==rr::RRScene::IMPROVED) || needRedisplay || gameOn)
	{
//		printf("---]");
		// pokud pouzivame rr renderer a zmenil se indirect, promaznout cache
		// nutne predtim nastavit params (renderedChannels apod)
		//!!! rendererCaching->setStatus(RRGLCachingRenderer::CS_READY_TO_COMPILE);
		glutPostRedisplay();
//printf("coll=%.1fM coll/sec=%fk\n",rr::RRIntersectStats::getInstance()->intersect_mesh/1000000.0f,rr::RRIntersectStats::getInstance()->intersect_mesh/1000.0f/timer.Watch());//!!!
	}
}

void depthBiasSelect(int depthBiasOption)
{
	depthBias24 = depthBiasOption;
	glPolygonOffset(slopeScale, depthBias24 * depthScale24);
	needDepthMapUpdate = 1;
	glutPostRedisplay();
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
		{
			int tmp;
			if(sscanf(argv[i],"%d",&tmp)==1)
			{
				if(tmp>=1 && tmp<=MAX_INSTANCES_PER_PASS) 
				{
					INSTANCES_PER_PASS = tmp;
				}
				else
					printf("Out of range, 1 to 10 allowed.\n");
			}
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
		if (!strcmp("-noAreaLight", argv[i])) {
			startWithSoftShadows = 0;
		}
		if (!strcmp("-lazyUpdates", argv[i])) {
			updateDuringLightMovement = 0;
		}
		if (strstr(argv[i], ".3ds") || strstr(argv[i], ".3DS")) {
			levelSequence.insertLevelFront(argv[i]);
		}
	}
}

int main(int argc, char **argv)
{
	/*
	// Get current flag
	int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );
	// Turn on leak-checking bit
	tmpFlag |= _CRTDBG_LEAK_CHECK_DF;
	// Turn off CRT block checking bit
	tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;
	// Set flag to the new value
	_CrtSetDbgFlag( tmpFlag );
	//_crtBreakAlloc = 31299;
	*/


	parseOptions(argc, argv);

	// init GLUT
	glutInitWindowSize(resolutionx,resolutiony);
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
	if(fullscreen)
	{
		char buf[100];
		sprintf(buf,"%dx%d:32",resolutionx,resolutiony);
		glutGameModeString(buf);
		glutEnterGameMode();
		glutSetCursor(GLUT_CURSOR_NONE);
	}
	else
	{
		glutCreateWindow("Realtime Radiosity");
		//glutFullScreen();
	}
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);

	// init GLEW
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	init_gl_states();

	updateMatrices(); // needed for startup without area lights (areaLight doesn't update matrices for 1 instance)


	// init shaders
	// init textures
	init_gl_resources();

	uberProgramGlobalSetup.SHADOW_MAPS = 1;
	uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
	uberProgramGlobalSetup.LIGHT_DIRECT = true;
	uberProgramGlobalSetup.LIGHT_DIRECT_MAP = true;
	uberProgramGlobalSetup.LIGHT_INDIRECT_COLOR = true;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramGlobalSetup.FORCE_2D_POSITION = false;

	// adjust INSTANCES_PER_PASS to GPU
	INSTANCES_PER_PASS = UberProgramSetup::detectMaxShadowmaps(uberProgram,INSTANCES_PER_PASS);
	if(!INSTANCES_PER_PASS) error("",true);

	areaLight->attachTo(&light);
	areaLight->setNumInstances(startWithSoftShadows?INITIAL_PASSES*INITIAL_INSTANCES_PER_PASS:1);

	if(rr::RRLicense::loadLicense("license_number")!=rr::RRLicense::VALID)
		error("Problem with license number.",false);

	glutMainLoop();
	return 0;
}
