#define MAX_INSTANCES              50  // max number of light instances aproximating one area light
#define MAX_INSTANCES_PER_PASS     10
unsigned INSTANCES_PER_PASS = 10; // 5 je max pro X800pro, 7 je max pro 6600
#define INITIAL_INSTANCES_PER_PASS INSTANCES_PER_PASS
#define INITIAL_PASSES             1
#define PRIMARY_SCAN_PRECISION     1 // 1nejrychlejsi/2/3nejpresnejsi, 3 s texturami nebude fungovat kvuli cachovani pokud se detekce vseho nevejde na jednu texturu - protoze displaylist myslim neuklada nastaveni textur
int fullscreen = 1;
bool renderer3ds = true;
bool updateDuringLightMovement = 1;
bool startWithSoftShadows = 1;
/*
zjistit co jeste chybi v distru
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

!vadna zed v koupelne4, zajizdi do podlahy

pri 2 instancich je levy okraj spotmapy oriznuty
 pri arealight by spotmapa potrebovala malinko mensi fov nez shadowmapy aby nezabirala mista kde konci stin

autodeteknout zda mam metry nebo centimetry
dodelat podporu pro matice do 3ds2rr importeru
kdyz uz by byl korektni model s gammou, pridat ovladani gammy

POZOR
neni tu korektni skladani primary+indirect a az nasledna gamma korekce (komplikovane pri multipass renderu)
scita se primary a zkorigovany indirect, vysledkem je ze primo osvicena mista jsou svetlejsi nez maji byt
*/

#include <assert.h>
#include <float.h>
#include <math.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/glut.h>
#include "RRIllumCalculator.h"
#include "DemoEngine/Camera.h"
#include "DemoEngine/Texture.h"
#include "DemoEngine/UberProgram.h"
#include "DemoEngine/MultiLight.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/3ds2rr.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/RendererOfRRObject.h"
#include "DemoEngine/UberProgramSetup.h"

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

/* Menu items. */
enum {
	ME_TOGGLE_GLOBAL_ILLUMINATION,
	ME_CHANGE_SPOTLIGHT,
	ME_TOGGLE_WIRE_FRAME,
	ME_TOGGLE_LIGHT_FRUSTUM,
	ME_SWITCH_MOUSE_CONTROL,
	ME_EXIT,
};


/////////////////////////////////////////////////////////////////////////////
//
// globals

Model_3DS m3ds;
char* filename_3ds="3ds\\koupelna\\koupelna4.3ds";
float scale_3ds = 0.03f;
RendererOfRRObject* rendererNonCaching = NULL;
RendererWithCache* rendererCaching = NULL;
Camera eye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
Camera light = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,20.};
GLUquadricObj *quadric;
AreaLight* areaLight = NULL;
#define lightDirectMaps 3
Texture *lightDirectMap[lightDirectMaps];
unsigned lightDirectMapIdx = 0;
Program *ambientProgram;
UberProgram* uberProgram;
UberProgramSetup uberProgramGlobalSetup;
int winWidth, winHeight;
int depthBias24 = 42;
int depthScale24;
GLfloat slopeScale = 4.0;
int needDepthMapUpdate = 1;
int xEyeBegin, yEyeBegin, movingEye = 0;
int xLightBegin, yLightBegin, movingLight = 0;
int wireFrame = 0;
int needMatrixUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
bool showHelp = 0;
int showLightViewFrustum = 1;
int eyeButton = GLUT_LEFT_BUTTON;
int lightButton = GLUT_MIDDLE_BUTTON;
int useDepth24 = 0;


/////////////////////////////////////////////////////////////////////////////

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nTry upgrading drivers for your graphics card.\nIf it doesn't help, your graphics card may be too old.\nCards that should work: NVIDIA 6xxx/7xxx, ATI 95xx+/Xxxx/X1xxx");
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
		try
		{
			lightDirectMap[i] = new Texture(name, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		}
		catch (...)
		{
			printf("Texture %s not found or not supported (supported = truecolor .tga).\n",name);
			error("",false);
		}

	}

	uberProgram = new UberProgram("shaders\\ubershader.vp", "shaders\\ubershader.fp");
	UberProgramSetup uberProgramSetup;
	uberProgramSetup.LIGHT_INDIRECT_COLOR = true;
	ambientProgram = uberProgram->getProgram(uberProgramSetup.getSetupString());

	if(!ambientProgram)
		error("\nFailed to compile or link GLSL program.\n",true);
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
class MyApp : public rr::RRVisionApp
{
protected:
	virtual void detectMaterials()
	{
		delete[] surfaces;
		numSurfaces = m3ds.numMaterials;
		surfaces = new rr::RRSurface[numSurfaces];
		for(unsigned i=0;i<numSurfaces;i++)
		{
			surfaces[i].reset(false);
			surfaces[i].diffuseReflectance = rr::RRColor(m3ds.Materials[i].color.r/255.0,m3ds.Materials[i].color.g/255.0,m3ds.Materials[i].color.b/255.0);
		}
	}
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!rendererCaching) return false;

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
			rendererNonCaching->setCapture(&captureUv,captureUv.firstCapturedTriangle); // set param for cache so it creates different displaylists
			renderScene(uberProgramSetup,0);
			rendererNonCaching->setCapture(NULL,0);

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
				//rrobject->getTriangleAdditionalMeasure(triangleIndex,rr::RM_EXITING_FLUX,tmp);
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
};

MyApp* app = NULL;


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
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,firstInstance,lightDirectMap[lightDirectMapIdx]))
		error("Failed to compile or link GLSL program.\n",true);

	// lze smazat, stejnou praci dokaze i rrrenderer
	// nicmene m3ds.Draw stale jeste
	// 1) lip smoothuje (pouziva min vertexu)
	// 2) slouzi jako test ze RRIllumCalculator spravne generuje vertex buffer s indirectem
	// 3) nezpusobuje 0.1sec zasek pri kazdem pregenerovani displaylistu
	// 4) muze byt v malym rozliseni nepatrne rychlejsi (pouziva min vertexu)
	if(uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		m3ds.Draw(uberProgramSetup.LIGHT_INDIRECT_COLOR?app:NULL,uberProgramSetup.LIGHT_INDIRECT_MAP);
		return;
	}
	RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
	rendererNonCaching->setRenderedChannels(renderedChannels);
	rendererCaching->render();
}

void updateDepthMap(unsigned mapIndex,unsigned mapIndices)
{
	if(!needDepthMapUpdate) return;

	assert(mapIndex>=0);
	areaLight->getInstance(mapIndex)->setupForRender();


	glColorMask(0,0,0,0);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	areaLight->getShadowMap((mapIndex>=0)?mapIndex:0)->renderingToInit();
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
	glClear(GL_COLOR_BUFFER_BIT);
	if(firstInstance==0) glClear(GL_DEPTH_BUFFER_BIT);

	if (wireFrame) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	eye.setupForRender();

	renderScene(uberProgramSetup,firstInstance);

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
	static char *message[] = {
		"Realtime Radiosity Viewer",
		" Stepan Hrbek, http://dee.cz",
		" radiosity engine, http://lightsprint.com",
		"",
		"Purpose:",
		" Show radiosity integration into arbitrary interactive 3d app",
		" using NO PRECALCULATIONS.",
		" Demos using precalculated data are coming.",
		"",
		"Controls:",
		" mouse+left button - manipulate camera",
		" mouse+mid button  - manipulate light",
		" right button      - menu",
		" arrows            - move camera or light (with middle mouse pressed)",
		"",
		" space - toggle global illumination",
		" 's'   - change spotlight",
		" '+ -' - increase/decrease penumbra (soft shadow) precision",
		" '* /' - increase/decrease penumbra (soft shadow) smoothness",
		" 'a'   - cycle through linear, rectangular and circular area light",
		" 'z/Z' - zoom in/out",
		" 'w'   - toggle wire frame",
		" 'f'   - toggle showing spotlight frustum",
		" 'm'   - toggle whether the left or middle mouse buttons control the eye and",
		"         view positions (helpful for systems with only a two-button mouse)",
/*		"",
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
		"h - help",
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

void display(void)
{
//	printf("Display.\n");//!!!
	if(!winWidth) return; // can't work without window

	app->reportIlluminationUse();

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

static void benchmark(int perFrameDepthMapUpdate)
{
	const int numFrames = 150;
	int start, stop;
	float time;
	int i;

	needDepthMapUpdate = 1;
	display();

	printf("starting benchmark...\n");
	glFinish();

	start = glutGet(GLUT_ELAPSED_TIME);
	for (i=0; i<numFrames; i++) {
		if (perFrameDepthMapUpdate) {
			needDepthMapUpdate = 1;
		}
		display();
	}
	glFinish();
	stop = glutGet(GLUT_ELAPSED_TIME);

	time = (stop - start)/1000.0;

	printf("  perFrameDepthMapUpdate=%d, time = %f secs, fps = %f\n",
		perFrameDepthMapUpdate, time, numFrames/time);
	needDepthMapUpdate = 1;
}

void switchMouseControl(void)
{
	if (eyeButton == GLUT_LEFT_BUTTON) {
		eyeButton = GLUT_MIDDLE_BUTTON;
		lightButton = GLUT_LEFT_BUTTON;
	} else {
		lightButton = GLUT_MIDDLE_BUTTON;
		eyeButton = GLUT_LEFT_BUTTON;
	}
	movingEye = 0;
	movingLight = 0;
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
	app->reportLightChange(true);
	app->reportEndOfInteractions(); // force update even in movingEye mode
}

void selectMenu(int item)
{
	app->reportCriticalInteraction();
	switch (item) 
	{
		case ME_SWITCH_MOUSE_CONTROL:
			switchMouseControl();
			return;  /* No redisplay needed. */

		case ME_TOGGLE_GLOBAL_ILLUMINATION:
			toggleGlobalIllumination();
			break;
		case ME_CHANGE_SPOTLIGHT:
			changeSpotlight();
			break;
		case ME_TOGGLE_WIRE_FRAME:
			toggleWireFrame();
			break;
		case ME_TOGGLE_LIGHT_FRUSTUM:
			showLightViewFrustum = !showLightViewFrustum;
			if (showLightViewFrustum) {
				needMatrixUpdate = 1;
			}
			break;
		case ME_EXIT:
			exit(0);
			break;
		default:
			assert(0);
			break;
	}
	glutPostRedisplay();
}

void reportLightMovement()
{
	if(updateDuringLightMovement)
	{
		// Behem pohybu svetla v male scene dava lepsi vysledky update (false)
		//  scena neni behem pohybu tmavsi, setrvacnost je neznatelna.
		// Ve velke scene dava lepsi vysledky reset (true),
		//  scena sice behem pohybu ztmavne,
		//  pri false je ale velka setrvacnost, nekdy dokonce stary indirect vubec nezmizi.
		app->reportLightChange(app->multiObject->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
	}
	else
	{
		app->reportCriticalInteraction();
	}
}

void special(int c, int x, int y)
{
	if(!movingLight) app->reportCriticalInteraction();
	Camera* cam = movingLight?&light:&eye;
	switch (c) 
	{
		case GLUT_KEY_F7:
			benchmark(1);
			return;
		case GLUT_KEY_F8:
			benchmark(0);
			return;
		case GLUT_KEY_F9:
			printf("\nCamera tmpeye = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",eye.pos[0],eye.pos[1],eye.pos[2],eye.angle,eye.height,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			printf("Camera tmplight = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",light.pos[0],light.pos[1],light.pos[2],light.angle,light.height,light.aspect,light.fieldOfView,light.anear,light.afar);
			return;

		case GLUT_KEY_UP:
			for(int i=0;i<3;i++) cam->pos[i]+=cam->dir[i]/20;
			if(movingLight) reportLightMovement();
			break;
		case GLUT_KEY_DOWN:
			for(int i=0;i<3;i++) cam->pos[i]-=cam->dir[i]/20;
			if(movingLight) reportLightMovement();
			break;
		case GLUT_KEY_LEFT:
			cam->pos[0]+=cam->dir[2]/20;
			cam->pos[2]-=cam->dir[0]/20;
			if(movingLight) reportLightMovement();
			break;
		case GLUT_KEY_RIGHT:
			cam->pos[0]-=cam->dir[2]/20;
			cam->pos[2]+=cam->dir[0]/20;
			if(movingLight) reportLightMovement();
			break;

		default:
			return;
	}
	/*if (glutGetModifiers() & GLUT_ACTIVE_CTRL) {
	switch (c) {
	case GLUT_KEY_UP:
	break;
	case GLUT_KEY_DOWN:
	break;
	case GLUT_KEY_LEFT:
	break;
	case GLUT_KEY_RIGHT:
	break;
	}
	return;
	}*/
	needMatrixUpdate = 1;
	if(movingLight) 
	{
		needDepthMapUpdate = 1;
	}
	glutPostRedisplay();
}

void keyboard(unsigned char c, int x, int y)
{
	switch (c)
	{
		case 27:
			//delete app; throws asser in freeing node from ivertex
			exit(0);
			break;
		case 'b':
			updateDepthBias(+1);
			break;
		case 'B':
			updateDepthBias(-1);
			break;
		case 'm':
			switchMouseControl();
			break;
		case 'h':
		case 'H':
			showHelp = !showHelp;
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
			break;
		case ' ':
			toggleGlobalIllumination();
			break;
		case 's':
			changeSpotlight();
			break;
		case 'S':
			app->reportLightChange(true);
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
		case 'a':
			++areaLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		default:
			return;
	}
	glutPostRedisplay();
}

void reshape(int w, int h)
{
	if(w<512 || h<512)
		LIMITED_TIMES(5,printf("Window size<512 not supported in this demo, expect incorrect render!\n"));
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = (double) winWidth / (double) winHeight;
	needMatrixUpdate = true;

	/* Perhaps there might have been a mode change so at window
	reshape time, redetermine the depth scale. */
	updateDepthScale();
}

void mouse(int button, int state, int x, int y)
{
	if (button == eyeButton && state == GLUT_DOWN) {
		movingEye = 1;
		xEyeBegin = x;
		yEyeBegin = y;
	}
	if (button == eyeButton && state == GLUT_UP) {
		app->reportEndOfInteractions();
		movingEye = 0;
	}
	if (button == lightButton && state == GLUT_DOWN) {
		movingLight = 1;
		xLightBegin = x;
		yLightBegin = y;
	}
	if (button == lightButton && state == GLUT_UP) {
		app->reportEndOfInteractions();
		if(!updateDuringLightMovement) app->reportLightChange(true);
		movingLight = 0;
		needDepthMapUpdate = 1;
		glutPostRedisplay();
	}
}

void motion(int x, int y)
{
	if (movingEye) {
		app->reportCriticalInteraction();
		eye.angle = eye.angle - 0.005*(x - xEyeBegin);
		eye.height = eye.height + 0.15*(y - yEyeBegin);
		CLAMP(eye.height,-13,13);
		xEyeBegin = x;
		yEyeBegin = y;
		needMatrixUpdate = 1;
	}
	if (movingLight) {
		light.angle = light.angle - 0.005*(x - xLightBegin);
		light.height = light.height + 0.15*(y - yLightBegin);
		CLAMP(light.height,-13,13);
		xLightBegin = x;
		yLightBegin = y;
		needMatrixUpdate = 1;
		needDepthMapUpdate = 1;
		reportLightMovement();
	}
}

//#include "Timer.h"
//rr::Timer timer;

void idle()
{
	if(!winWidth) return; // can't work without window
//	LIMITED_TIMES(1,timer.Start());
	bool rrOn = drawMode == DM_EYE_VIEW_SOFTSHADOWED;
//	printf("[--- %d %d %d %d",rrOn?1:0,movingEye?1:0,updateDuringLightMovement?1:0,movingLight?1:0);
	// pri kalkulaci nevznikne improve -> neni read results -> aplikace neda display -> pristi calculate je dlouhy
	// pokud se ale hybe svetlem, aplikace da display -> pristi calculate je kratky
	if((rrOn && app->calculate()==rr::RRScene::IMPROVED) || movingEye || movingLight)
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
	wglSwapIntervalEXT(0);
#endif
}

void initMenus(void)
{
	glutCreateMenu(selectMenu);
	glutAddMenuEntry("[ ] Toggle global illumination", ME_TOGGLE_GLOBAL_ILLUMINATION);
	glutAddMenuEntry("[s] Change spotlight", ME_CHANGE_SPOTLIGHT);
	glutAddMenuEntry("[f] Toggle light frustum", ME_TOGGLE_LIGHT_FRUSTUM);
	glutAddMenuEntry("[w] Toggle wire frame", ME_TOGGLE_WIRE_FRAME);
	glutAddMenuEntry("[m] Switch mouse buttons", ME_SWITCH_MOUSE_CONTROL);
	glutAddMenuEntry("[ESC] Quit", ME_EXIT);

	glutAttachMenu(GLUT_RIGHT_BUTTON);
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
			filename_3ds = argv[i];
			scale_3ds = 1;
		}
		if (!strcmp("-depth24", argv[i])) {
			useDepth24 = 1;
		}
	}
}

int main(int argc, char **argv)
{
	float stitchDistance = 0.01f;

	if(rr::RRLicense::loadLicense("license_number")!=rr::RRLicense::VALID)
		error("Problem with license number.",false);

	glutInitWindowSize(800, 600);
	glutInit(&argc, argv);
	parseOptions(argc, argv);

	if (useDepth24) {
		glutInitDisplayString("depth~24 rgb double accum");
	} else {
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ACCUM);
	}
	glutCreateWindow("shadowcast");
	if(fullscreen)
	{
		glutFullScreen();
	}

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		// Problem: glewInit failed, something is seriously wrong.
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		return 1;
	}
	//fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutIdleFunc(idle);

	/* Menu initialization depends on knowing what extensions are
	supported. */
	initMenus();

	if (strstr(filename_3ds, "koupelna4")) {
		scale_3ds = 0.03f;
		//Camera koupelna4_eye = {{0.032202,1.659255,1.598609},10.010005,-0.150000};
		//Camera koupelna4_light = {{-1.309976,0.709500,0.498725},3.544996,-10.000000};
		Camera koupelna4_eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
		Camera koupelna4_light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
		eye = koupelna4_eye;
		light = koupelna4_light;
	}
	if (strstr(filename_3ds, "koupelna3")) {
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
	}
	if (strstr(filename_3ds, "koupelna5")) {
		scale_3ds = 0.03f;
		//Camera tmpeye = {{6.172,3.741,1.522},4.340,1.600,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		Camera tmpeye = {{5.735,2.396,1.479},4.405,0.100,1.3,100.0,0.3,60.0};
		Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
	}
	if (strstr(filename_3ds, "sponza"))
	{
		Camera sponza_eye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999, 1.,100.,0.3,60.};
		Camera sponza_light = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001, 1.,70.,1.,30.};
		//Camera sponza_eye = {{-10.407576,1.605258,4.050256},7.859994,-0.050000};
		//Camera sponza_light = {{-7.109047,5.130751,-2.025017},0.404998,2.950001};
		//lightFieldOfView += 10.0;
		//eyeFieldOfView = 50.0;
		eye = sponza_eye;
		light = sponza_light;
	}

	if (strstr(filename_3ds, "sibenik"))
	{
		stitchDistance = 0.01f;
		// zacatek nevhodny pouze kvuli spatnym normalam
//		Camera tmpeye = {{-8.777,3.117,0.492},1.145,-0.400,1.3,50.0,0.3,80.0};
//		Camera tmplight = {{-0.310,2.952,-0.532},5.550,3.200,1.0,70.0,1.0,40.0};
		// dalsi zacatek kde je videt zmrsena normala
		Camera tmpeye = {{-3.483,5.736,2.755},7.215,2.050,1.3,50.0,0.3,80.0};
		Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
		// detail vadne normaly
		//Camera tmpeye = {{3.078,5.093,4.675},7.995,-1.700,1.3,50.0,0.3,80.0};
		//Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
		/* na screenshoty
		Camera tmpeye = {{-8.777,3.117,0.492},1.630,-13.000,1.3,50.0,0.3,80.0};
		Camera tmplight = {{-0.310,2.952,-0.532},4.670,-13.000,1.0,70.0,1.0,40.0};
		*/
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
	}

	updateMatrices(); // needed for startup without area lights (areaLight doesn't update matrices for 1 instance)

	//if(rrobject) printf("vertices=%d triangles=%d\n",rrobject->getCollider()->getMesh()->getNumVertices(),rrobject->getCollider()->getMesh()->getNumTriangles());
	rr::RRScene::setStateF(rr::RRScene::SUBDIVISION_SPEED,0);
	rr::RRScene::setState(rr::RRScene::GET_SOURCE,0);
	rr::RRScene::setState(rr::RRScene::GET_REFLECTED,1);
	//rr::RRScene::setState(rr::GET_SMOOTH,0);
	rr::RRScene::setStateF(rr::RRScene::MIN_FEATURE_SIZE,0.15f);
	//rr::RRScene::setStateF(rr::MAX_SMOOTH_ANGLE,0.4f);

	init_gl_states();
	init_gl_resources();

	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);

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
	INSTANCES_PER_PASS = UberProgramSetup::detectMaxShadowmaps(uberProgram);
	if(!INSTANCES_PER_PASS) error("",true);

	areaLight->attachTo(&light);
	areaLight->setNumInstances(startWithSoftShadows?INITIAL_PASSES*INITIAL_INSTANCES_PER_PASS:1);

	printf("Loading scene...");

	// load 3ds
	if(!m3ds.Load(filename_3ds,scale_3ds))
		error("",false);
	//m3ds.shownormals=1;
	//m3ds.numObjects=2;//!!!
	app = new MyApp();
	new_3ds_importer(&m3ds,app,stitchDistance);

//	printf(app->getObject(0)->getCollider()->getMesh()->save("c:\\a")?"saved":"not saved");
//	printf(app->getObject(0)->getCollider()->getMesh()->load("c:\\a")?" / loaded":" / not loaded");

	printf("\n");
	char title[256];
	sprintf(title,"Realtime Radiosity Viewer - %s",filename_3ds);
	glutSetWindowTitle(title);

	// creates radiosity solver with multiobject
	// without renderer, no primary light is detected
	app->calculate();

//printf(" %d triangles\n",app->multiObject?app->multiObject->getCollider()->getMesh()->getNumTriangles():0);

	if(!app->multiObject)
		error("No objects in scene.",false);

	// creates renderer
	rendererNonCaching = new RendererOfRRObject(app->multiObject,app->scene);
	rendererCaching = new RendererWithCache(rendererNonCaching);
	// next calculate will use renderer to detect primary illum
	// must be called from mainloop, we don't know winWidth/winHeight yet

	glutMainLoop();
	return 0;
}
