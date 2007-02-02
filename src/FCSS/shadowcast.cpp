//#define BUGS
#define DYNAOBJECTS                1   // 0..7
#define MAX_INSTANCES              50  // max number of light instances aproximating one area light
#define MAX_INSTANCES_PER_PASS     10
unsigned INSTANCES_PER_PASS = 6; // 5 je max pro X800pro, 6 je max pro 6150, 7 je max pro 6600
#define INITIAL_INSTANCES_PER_PASS INSTANCES_PER_PASS
#define INITIAL_PASSES             1
#define SHADOW_MAP_SIZE_SOFT       512
#define SHADOW_MAP_SIZE_HARD       2048
#define SUBDIVISION                0
#define LIGHTMAP_SIZE              1024
#define PRIMARY_SCAN_PRECISION     1 // 1nejrychlejsi/2/3nejpresnejsi, 3 s texturami nebude fungovat kvuli cachovani pokud se detekce vseho nevejde na jednu texturu - protoze displaylist myslim neuklada nastaveni textur
bool ati = 1;
int fullscreen = 0;
bool animated = 1;
bool renderer3ds = 1;
bool startWithSoftShadows = 1;
bool renderLightmaps = 0;
/*
crashne po esc v s_veza/gcc

-gamma korekce (do rrscaleru)
-kontrast korekce (pred rendrem)
-jas korekce (pred rendrem)

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

#include "Q3Loader.h" // asi musi byt prvni, kvuli pragma pack
#include <limits> // nutne aby uspel build v gcc4.3
#include "DemoEngine/Timer.h"
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
#include "RRGPUOpenGL.h"
#include "RRGPUOpenGL/RendererOfRRObject.h"
#include "DemoEngine/AreaLight.h"
#include "DemoEngine/Camera.h"
#include "DemoEngine/Texture.h"
#include "DemoEngine/UberProgram.h"
#include "DemoEngine/Model_3DS.h"
#include "DemoEngine/RendererWithCache.h"
#include "DemoEngine/UberProgramSetup.h"
#include "3ds2rr.h"
#include "DynamicObject.h"
#include "Bugs.h"
#include "LevelSequence.h"


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
	de::Model_3DS m3ds;
	//TMapQ3 bsp;
	class Solver* solver;
	class Bugs* bugs;
	rr_gl::RendererOfRRObject* rendererNonCaching;
	de::RendererWithCache* rendererCaching;

	Level(const char* filename_3ds);
	~Level();
};


/////////////////////////////////////////////////////////////////////////////
//
// globals

de::Camera eye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
de::Camera light = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,100.};
GLUquadricObj *quadric;
de::AreaLight* areaLight = NULL;
#define lightDirectMaps 3
de::Texture* lightDirectMap[lightDirectMaps];
unsigned lightDirectMapIdx = 0;
de::Texture* loadingMap = NULL;
de::Texture* hintMap = NULL;
de::Program *ambientProgram;
de::UberProgram* uberProgram;
de::UberProgramSetup uberProgramGlobalSetup;
int winWidth = 0;
int winHeight = 0;
int depthBias24 = 50;//23;//42;
int depthScale24;
GLfloat slopeScale = 5;//2.3;//4.0;
int needDepthMapUpdate = 1;
bool needLightmapCacheUpdate = false;
int wireFrame = 0;
int needMatrixUpdate = 1;
int drawMode = DM_EYE_VIEW_SOFTSHADOWED;
bool showHelp = 0;
bool showHint = 0;
int showLightViewFrustum = 0;
bool paused = 0;
int resolutionx = 640;
int resolutiony = 480;
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
LevelSequence levelSequence;
class DynamicObjects* dynaobjects;

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
	printf("%f %d %d\n",slopeScale,depthBias24,depthScale24);
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

	areaLight = new de::AreaLight(&light,MAX_INSTANCES,SHADOW_MAP_SIZE_SOFT);

	// update states, but must be done after initing shadowmaps (inside arealight)
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getTexelBits();
	depthScale24 = 1 << (shadowDepthBits-16);
	updateDepthBias(0);  /* Update with no offset change. */

	for(unsigned i=0;i<lightDirectMaps;i++)
	{
		char name[]="maps\\spot0.png";
		name[9] = '0'+i;
		lightDirectMap[i] = de::Texture::load(name, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
		if(!lightDirectMap[i])
		{
			printf("Texture %s not found or invalid.\n",name);
			error("",false);
		}
	}
	loadingMap = de::Texture::load("maps\\rrbugs_loading.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	hintMap = de::Texture::load("maps\\rrbugs_hint.tga", GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);

	uberProgram = new de::UberProgram("shaders\\ubershader.vp", "shaders\\ubershader.fp");
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
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
// Solver

void renderScene(de::UberProgramSetup uberProgramSetup, unsigned firstInstance);
void updateMatrices();
void updateDepthMap(unsigned mapIndex,unsigned mapIndices);

class Solver : public rr_gl::RRRealtimeRadiosityGL
{
public:
	virtual ~Solver()
	{
		// delete objects and illumination
		delete3dsFromRR(this);
	}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		needLightmapCacheUpdate = true; // pokazdy kdyz pridam/uberu jakoukoliv lightmapu, smaznout z cache
		return renderLightmaps ? rr_gl::RRRealtimeRadiosityGL::createIlluminationPixelBuffer(LIGHTMAP_SIZE,LIGHTMAP_SIZE) : NULL;
	}
	virtual void detectMaterials()
	{
/*#ifdef RR_DEVELOPMENT
		delete[] surfaces;
		numSurfaces = level->m3ds.numMaterials;
		surfaces = new rr::RRSurface[numSurfaces];
		for(unsigned i=0;i<numSurfaces;i++)
		{
			surfaces[i].reset(false);
			surfaces[i].diffuseReflectance = rr::RRColor(m3ds.Materials[i].color.r/255.0,m3ds.Materials[i].color.g/255.0,m3ds.Materials[i].color.b/255.0);
		}
#endif*/
	}
	virtual bool detectDirectIllumination()
	{
		// renderer not ready yet, fail
		if(!level || !level->rendererCaching) return false;

		// first time illumination is detected, no shadowmap has been created yet
		if(needDepthMapUpdate)
		{
			if(needMatrixUpdate) updateMatrices(); // probably not necessary
			// to avoid depth updates only for us, all depthmaps are updated and then used by following display()
			if(drawMode==DM_EYE_VIEW_SOFTSHADOWED)
			{
				unsigned numInstances = areaLight->getNumInstances();
				for(unsigned i=0;i<numInstances;i++)
				{
					updateDepthMap(i,numInstances);
				}
			}
			else
			{
				updateDepthMap(0,0);
			}
		}

		// setup shader for rendering direct illumination+shadows without materials
		return RRRealtimeRadiosityGL::detectDirectIllumination();
	}
	virtual void setupShader()
	{
		if(!level) return;

		de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = 1;
		uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
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
		uberProgramSetup.MATERIAL_SPECULAR = false;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		uberProgramSetup.MATERIAL_NORMAL_MAP = false;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = true;

		if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap[lightDirectMapIdx]))
			error("Failed to compile or link GLSL program.\n",true);
	}
	virtual void reportAction(const char* action) const
	{
		printf(action);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// Dynamic objects

class DynamicObjects
{
public:
	DynamicObjects()
	{
		de::UberProgramSetup material;

		// diffuse
		material.MATERIAL_DIFFUSE = 1;
		material.MATERIAL_DIFFUSE_COLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = 1;
		material.MATERIAL_SPECULAR = 0;
		material.MATERIAL_SPECULAR_MAP = 0;
		material.MATERIAL_NORMAL_MAP = 0;
		//dynaobject[3] = DynamicObject::create("3ds\\characters\\armyman2003.3ds",0.006f,material); // 14k
		if(DYNAOBJECTS>0) dynaobject[0] = DynamicObject::create("3ds\\characters\\blackman1\\blackman.3ds",0.95f,material,0); // 1k
		if(DYNAOBJECTS>1) dynaobject[1] = DynamicObject::create("3ds\\characters\\civil\\civil.3ds",0.01f,material,0); // 2k
		//dynaobject[4] = DynamicObject::create("3ds\\characters\\3dm-female3\\3dm-female3.3ds",0.008f,material); // strasny vlasy
		//dynaobject[5] = DynamicObject::create("3ds\\characters\\Tifa\\Tifa.3ds",0.028f,material); // prilis lowpoly oblicej
		//dynaobject[5] = DynamicObject::create("3ds\\characters\\icop\\icop.3DS",0.04f,material);
		//dynaobject[6] = DynamicObject::create("3ds\\characters\\ct\\crono.3ds",0.01f,material);
		//dynaobject[7] = DynamicObject::create("3ds\\characters\\ct\\lucca.3ds",0.01f,material);

		// diff+specular map+normalmap
		material.MATERIAL_DIFFUSE = 1;
		material.MATERIAL_DIFFUSE_COLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = 1;
		material.MATERIAL_SPECULAR = 1;
		material.MATERIAL_SPECULAR_MAP = 1;
		material.MATERIAL_NORMAL_MAP = 1;
		if(DYNAOBJECTS>2) dynaobject[2] = DynamicObject::create("3ds\\characters\\sven\\sven.3ds",0.011f,material,8); // 2k

		// diff+specular
		material.MATERIAL_DIFFUSE = 1;
		material.MATERIAL_DIFFUSE_COLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = 1;
		material.MATERIAL_SPECULAR = 1;
		material.MATERIAL_SPECULAR_MAP = 0;
		material.MATERIAL_NORMAL_MAP = 0;
		if(DYNAOBJECTS>3) dynaobject[3] = DynamicObject::create("3ds\\characters\\woman-statue9.3ds",0.004f,material,4); // 9k
		if(DYNAOBJECTS>5) dynaobject[5] = DynamicObject::create("3ds\\characters\\Jessie16.3DS",0.022f,material,16); // 16k

		// diff+specular map
		material.MATERIAL_DIFFUSE = 1;
		material.MATERIAL_DIFFUSE_COLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = 1;
		material.MATERIAL_SPECULAR = 1;
		material.MATERIAL_SPECULAR_MAP = 1;
		material.MATERIAL_NORMAL_MAP = 0;
		if(DYNAOBJECTS>4) dynaobject[4] = DynamicObject::create("3ds\\characters\\potato\\potato01.3ds",0.004f,material,16); // 13k

		// specular
		material.MATERIAL_DIFFUSE = 0;
		material.MATERIAL_DIFFUSE_COLOR = 0;
		material.MATERIAL_DIFFUSE_MAP = 0;
		material.MATERIAL_SPECULAR = 1;
		material.MATERIAL_SPECULAR_MAP = 0;
		material.MATERIAL_NORMAL_MAP = 0;
		if(DYNAOBJECTS>6) dynaobject[6] = DynamicObject::create("3ds\\characters\\I Robot female.3ds",0.24f,material,16); // 20k

		// static: quake = 28k

		// ok otexturovane
		//dynaobject = DynamicObject::create("3ds\\characters\\ct\\crono.3ds",0.01f); // ok
		//dynaobject = DynamicObject::create("3ds\\characters\\ct\\lucca.3ds",0.01f); // ok
		//dynaobject = DynamicObject::create("3ds\\characters\\sven\\sven.3ds",0.01f); // ok
		//dynaobject = DynamicObject::create("3ds\\characters\\potato\\potato01.3ds",0.004f); // ok
		//dynaobject = DynamicObject::create("3ds\\objects\\head\\head.3DS",0.004f); // ok. vytvari zeleny facy po koupelne
		//dynaobject = DynamicObject::create("3ds\\characters\\swatfemale\\female.3ds",0.02f); // spatne normaly na bocich
		//dynaobject = DynamicObject::create("3ds\\characters\\icop\\icop.3DS",0.04f); // spatne normaly na zadech a chodidlech
		//dynaobject = DynamicObject::create("3ds\\characters\\GokuGT_3DS\\Goku_GT.3DS",0.004f); // ok, ale jen nekomercne

		// ok neotexturovane
		//dynaobject = DynamicObject::create("3ds\\characters\\armyman2003.3ds",0.01f); // ok
		//dynaobject = DynamicObject::create("3ds\\characters\\i robot female.3ds",0.04f); // ok
		//dynaobject = DynamicObject::create("3ds\\objects\\knife.3ds",0.01f); // ok
		//dynaobject = DynamicObject::create("3ds\\characters\\snowman.3ds",1); // spatne normaly zezadu
		//dynaobject = DynamicObject::create("3ds\\objects\\gothchocker.3ds",0.2f); // spatne normaly zezadu
		//dynaobject = DynamicObject::create("3ds\\objects\\rubic_cube.3ds",0.01f); // spatne normaly, ale pouzitelne
		//dynaobject = DynamicObject::create("3ds\\objects\\polyhedrons_ts.3ds",0.1f); // sesmoothovane normaly, chybi hrany
	}
	const rr::RRVec3& getPos(unsigned objIndex)
	{
		return dynaobject[MIN(objIndex,DYNAOBJECTS)]->worldFoot;
	}
	void setPos(unsigned objIndex, rr::RRVec3 worldFoot)
	{
		if(objIndex<DYNAOBJECTS) dynaobject[objIndex]->worldFoot = worldFoot;
	}
	void renderSceneDynamic(de::UberProgramSetup uberProgramSetup, unsigned firstInstance) const
	{
		static float d = 0;
		// increment rotation when frame begins
		if(!uberProgramSetup.LIGHT_DIRECT && !firstInstance) d = (timeGetTime()%10000000)*0.07f;
		// use object space
		uberProgramSetup.OBJECT_SPACE = true;
		// use environment maps
		if(uberProgramSetup.LIGHT_INDIRECT_COLOR || uberProgramSetup.LIGHT_INDIRECT_MAP)
		{
			// indirect from envmap
			uberProgramSetup.SHADOW_MAPS = 1;
			//uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_INDIRECT_CONST = 0;
			uberProgramSetup.LIGHT_INDIRECT_COLOR = 0;
			uberProgramSetup.LIGHT_INDIRECT_MAP = 0;
			uberProgramSetup.LIGHT_INDIRECT_ENV = 1;
		}
		//dynaobject[4]->worldFoot = rr::RRVec3(2.2f*sin(d*0.005f),1.0f,2.2f);

		for(unsigned i=0;i<DYNAOBJECTS;i++)
		{
			if(dynaobject[i])
				dynaobject[i]->render(uberProgram,uberProgramSetup,areaLight,firstInstance,lightDirectMap[lightDirectMapIdx],level->solver,eye,d);
		}
	}
	~DynamicObjects()
	{
		for(unsigned i=0;i<DYNAOBJECTS;i++)
			delete dynaobject[i];
	}
private:
	DynamicObject* dynaobject[DYNAOBJECTS];
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

// callback that feeds 3ds renderer with our vertex illumination
const float* lockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRRealtimeRadiosity*)solver)->getIllumination(object)->getChannel(0)->vertexBuffer;
	return vertexBuffer ? &vertexBuffer->lock()->x : NULL;
}

// callback that cleans vertex illumination
void unlockVertexIllum(void* solver,unsigned object)
{
	rr::RRIlluminationVertexBuffer* vertexBuffer = ((rr::RRRealtimeRadiosity*)solver)->getIllumination(object)->getChannel(0)->vertexBuffer;
	if(vertexBuffer) vertexBuffer->unlock();
}

void renderSceneStatic(de::UberProgramSetup uberProgramSetup, unsigned firstInstance)
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
	if(uberProgramSetup.MATERIAL_DIFFUSE && uberProgramSetup.MATERIAL_DIFFUSE_MAP && !uberProgramSetup.FORCE_2D_POSITION && renderer3ds && !renderLightmaps)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		level->m3ds.Draw(level->solver,uberProgramSetup.LIGHT_INDIRECT_COLOR?lockVertexIllum:NULL,unlockVertexIllum);
		return;
	}
	rr_gl::RendererOfRRObject::RenderedChannels renderedChannels;
	renderedChannels.LIGHT_DIRECT = uberProgramSetup.LIGHT_DIRECT;
	renderedChannels.LIGHT_INDIRECT_COLOR = uberProgramSetup.LIGHT_INDIRECT_COLOR;
	renderedChannels.LIGHT_INDIRECT_MAP = uberProgramSetup.LIGHT_INDIRECT_MAP;
	renderedChannels.LIGHT_INDIRECT_ENV = uberProgramSetup.LIGHT_INDIRECT_ENV;
	renderedChannels.MATERIAL_DIFFUSE_COLOR = uberProgramSetup.MATERIAL_DIFFUSE_COLOR;
	renderedChannels.MATERIAL_DIFFUSE_MAP = uberProgramSetup.MATERIAL_DIFFUSE_MAP;
	renderedChannels.FORCE_2D_POSITION = uberProgramSetup.FORCE_2D_POSITION;
	level->rendererNonCaching->setRenderedChannels(renderedChannels);
	if(renderedChannels.LIGHT_INDIRECT_COLOR)
	{
		// turn off caching for renders with indirect color, because it changes often
		level->rendererCaching->setStatus(de::RendererWithCache::CS_NEVER_COMPILE);
	}
	if(renderedChannels.LIGHT_INDIRECT_MAP && needLightmapCacheUpdate)
	{
		// refresh cache for renders with ambient map after any ambient map reallocation
		needLightmapCacheUpdate = false;
		level->rendererCaching->setStatus(de::RendererWithCache::CS_READY_TO_COMPILE);
	}
	if(renderedChannels.LIGHT_INDIRECT_MAP && !level->solver->getIllumination(0)->getChannel(0)->pixelBuffer)
	{
		// create lightmaps if they are needed for render
		level->solver->calculate(rr::RRRealtimeRadiosity::UPDATE_PIXEL_BUFFERS);
	}
	level->rendererCaching->render();
}

void renderScene(de::UberProgramSetup uberProgramSetup, unsigned firstInstance)
{
	// render static scene
	assert(!uberProgramSetup.OBJECT_SPACE); 
	renderSceneStatic(uberProgramSetup,firstInstance);
	if(uberProgramSetup.FORCE_2D_POSITION) return;
	dynaobjects->renderSceneDynamic(uberProgramSetup,firstInstance);
}

void updateDepthMap(unsigned mapIndex,unsigned mapIndices)
{
	if(!needDepthMapUpdate) return;
	assert(mapIndex>=0);
	de::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;

	glColorMask(0,0,0,0);
	de::Texture* shadowmap = areaLight->getShadowMap((mapIndex>=0)?mapIndex:0);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClearDepth(0.9999); // prevents backprojection
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);

	de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
	uberProgramSetup.SHADOW_MAPS = 0;
	uberProgramSetup.SHADOW_SAMPLES = 0;
	uberProgramSetup.LIGHT_DIRECT = false;
	uberProgramSetup.LIGHT_DIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_CONST = false;
	uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
	uberProgramSetup.LIGHT_INDIRECT_MAP = false;
	uberProgramSetup.LIGHT_INDIRECT_ENV = false;
	uberProgramSetup.MATERIAL_DIFFUSE = false;
	uberProgramSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
	uberProgramSetup.MATERIAL_SPECULAR = false;
	uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
	uberProgramSetup.MATERIAL_NORMAL_MAP = false;
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

void drawEyeViewShadowed(de::UberProgramSetup uberProgramSetup, unsigned firstInstance)
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
	if(numInstances<=INSTANCES_PER_PASS && uberProgramGlobalSetup.MATERIAL_DIFFUSE && uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP)
	{
		de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = numInstances;
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = !renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
		//uberProgramSetup.MATERIAL_SPECULAR = ;
		//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
		//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
		//uberProgramSetup.OBJECT_SPACE = false;
		uberProgramSetup.FORCE_2D_POSITION = false;
		drawEyeViewShadowed(uberProgramSetup,0);
		return;
	}

	glClear(GL_ACCUM_BUFFER_BIT);

	// add direct
	for(unsigned i=0;i<numInstances;i+=INSTANCES_PER_PASS)
	{
		de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = MIN(INSTANCES_PER_PASS,numInstances);
		//uberProgramSetup.SHADOW_SAMPLES = ;
		uberProgramSetup.LIGHT_DIRECT = true;
		//uberProgramSetup.LIGHT_DIRECT_MAP = ;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
		uberProgramSetup.LIGHT_INDIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
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
		de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
		uberProgramSetup.SHADOW_MAPS = 0;
		uberProgramSetup.SHADOW_SAMPLES = 0;
		uberProgramSetup.LIGHT_DIRECT = false;
		uberProgramSetup.LIGHT_DIRECT_MAP = false;
		uberProgramSetup.LIGHT_INDIRECT_CONST = false;
		uberProgramSetup.LIGHT_INDIRECT_COLOR = !renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
		uberProgramSetup.LIGHT_INDIRECT_ENV = false;
		//uberProgramSetup.MATERIAL_DIFFUSE = ;
		//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
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

static void output(int x, int y, const char *string)
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
//	if(!big && gameOn) return;

	static const char *message[] = {
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
		" enter - change spotlight texture",
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

void showImageBegin()
{
	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
}

void showImageEnd()
{
	glDisable(GL_CULL_FACE);
	glColor3f(1,1,1);
	glDisable(GL_DEPTH_TEST);

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

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glutSwapBuffers();
}

void showImage(const de::Texture* tex)
{
	showImageBegin();
	tex->bindTexture();
	showImageEnd();
}


/////////////////////////////////////////////////////////////////////////////
//
// Level body

Level::Level(const char* filename_3ds)
{
	solver = NULL;
	bugs = NULL;
	rendererNonCaching = NULL;
	rendererCaching = NULL;

	float scale_3ds = 1;
	{
		de::Camera tmpeye = {{0.000000,1.000000,4.000000},2.935000,-0.7500, 1.,100.,0.3,60.};
		de::Camera tmplight = {{-1.233688,3.022499,-0.542255},1.239998,6.649996, 1.,70.,1.,20.};
		eye = tmpeye;
		light = tmplight;
	}
	if(strstr(filename_3ds, "quake") || strstr(filename_3ds, "QUAKE")) {
		scale_3ds = 0.05f;
		//Camera tmpeye = {{4.533,0.732,3.848},2.150,-2.400,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{3.713,1.013,-3.391},1.045,-1.450,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{3.378,1.925,-3.447},13.590,-0.050,1.3,75.0,0.3,60.0};
		//Camera tmplight = {{8.193,5.060,5.751},8.985,2.150,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{13.739,1.732,-1.572},23.650,0.100,1.3,75.0,0.3,60.0};
		//Camera tmplight = {{8.193,5.060,5.751},9.170,2.900,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{13.739,1.732,-1.560},23.660,0.350,1.3,75.0,0.3,60.0};
		//Camera tmplight = {{13.369,2.732,3.065},16.515,0.850,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{13.739,1.732,-1.572},23.470,-0.350,1.3,75.0,0.3,60.0};
		//Camera tmplight = {{8.193,5.060,5.751},9.295,2.350,1.0,70.0,1.0,20.0};
		de::Camera tmpeye = {{13.749,1.743,-0.891},23.390,0.100,1.3,75.0,0.3,60.0};
		de::Camera tmplight = {{8.411,5.053,2.312},9.040,7.900,1.0,70.0,1.0,20.0};
		dynaobjects->setPos(0,rr::RRVec3(11.55f,0.355f,-2.93f));
		dynaobjects->setPos(1,rr::RRVec3(8.41f,3.555f,0.17f));
		dynaobjects->setPos(2,rr::RRVec3(12.57f,0,-1.45f));
		dynaobjects->setPos(3,rr::RRVec3(10.71f,0.711f,0.38f));
		dynaobjects->setPos(4,rr::RRVec3(12.71f,1.711f,0.38f));
		dynaobjects->setPos(5,rr::RRVec3(8.71f,0.711f,0.38f));

		eye = tmpeye;
		light = tmplight;
	}
	if(strstr(filename_3ds, "koupelna4")) {
		scale_3ds = 0.03f;
		// dobry zacatek
		de::Camera tmpeye = {{-3.448,1.953,1.299},8.825,0.100,1.3,95.0,0.3,60.0};
		de::Camera tmplight = {{-1.802,0.715,0.850},3.600,-1.450,1.0,70.0,1.0,20.0};
		// bad lmap
//		Camera tmpeye = {{1.910,1.298,1.580},6.650,2.350,1.3,75.0,0.3,60.0};
//		Camera tmplight = {{2.950,0.899,3.149},7.405,13.000,1.0,70.0,1.0,20.0};
//		Camera tmpeye = {{-3.118,1.626,-1.334},11.025,-0.650,1.3,75.0,0.3,60.0};
//		Camera tmplight = {{-1.802,0.715,0.850},3.495,0.650,1.0,70.0,1.0,20.0};
		// shoty do doxy
		//Camera tmpeye = {{-3.842,1.429,-0.703},8.165,-0.050,1.3,100.0,0.3,60.0}; // red
		//Camera tmplight = {{-1.802,0.715,0.850},1.250,0.050,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{-3.842,1.429,-0.703},8.165,-0.050,1.3,100.0,0.3,60.0}; // blue
		//Camera tmplight = {{-0.626,0.927,-4.018},2.010,-3.400,1.0,70.0,1.0,20.0};
		//Camera tmpeye = {{-3.842,1.429,-0.703},8.165,-0.050,1.3,100.0,0.3,60.0}; // white
		//Camera tmplight = {{-0.759,3.520,-0.736},-1.950,8.800,1.0,70.0,1.0,20.0};
		// backlight bug
		//Camera tmpeye = {{-0.273,4.571,3.438},10.085,9.250,1.3,75.0,0.3,60.0};
		//Camera tmplight = {{-2.379,2.074,3.170},0.495,-0.100,1.0,70.0,1.0,20.0};
		//Camera koupelna4_eye = {{0.032202,1.659255,1.598609},10.010005,-0.150000};
		//Camera koupelna4_light = {{-1.309976,0.709500,0.498725},3.544996,-10.000000};
//		Camera koupelna4_eye = {{-3.742134,1.983256,0.575757},9.080003,0.000003, 1.,50.,0.3,60.};
//		Camera koupelna4_light = {{-1.801678,0.715500,0.849606},3.254993,-3.549996, 1.,70.,1.,20.};
		//Camera koupelna4_eye = {{0.823,1.500,-0.672},11.055,-0.050,1.3,100.0,0.3,60.0};//wrong backprojection
		//Camera koupelna4_light = {{-1.996,0.257,-2.205},0.265,-1.000,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
//		if(areaLight) areaLight->setNumInstances(INSTANCES_PER_PASS);
	}
	if(strstr(filename_3ds, "koupelna3")) {
		scale_3ds = 0.01f;
		//lightDirectMapIdx = 1;
		// dobry zacatek
		de::Camera tmpeye = {{0.822,1.862,6.941},9.400,-0.450,1.3,50.0,0.3,60.0};
		de::Camera tmplight = {{1.906,1.349,1.838},3.930,0.100,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
//		if(areaLight) areaLight->setNumInstances(INSTANCES_PER_PASS);
	}
	if(strstr(filename_3ds, "koupelna5")) {
		scale_3ds = 0.03f;
		//Camera tmpeye = {{6.172,3.741,1.522},4.340,1.600,1.3,100.0,0.3,60.0};
		//Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		de::Camera tmpeye = {{5.735,2.396,1.479},4.405,0.100,1.3,100.0,0.3,60.0};
		de::Camera tmplight = {{-2.825,4.336,3.259},1.160,9.100,1.0,70.0,1.0,20.0};
		eye = tmpeye;
		light = tmplight;
//		if(areaLight) areaLight->setNumInstances(1);
	}
	if(strstr(filename_3ds, "sponza"))
	{
		de::Camera tmpeye = {{-15.619742,7.192011,-0.808423},7.020000,1.349999, 1.,100.,0.3,60.};
		de::Camera tmplight = {{-8.042444,7.689753,-0.953889},-1.030000,0.200001, 1.,70.,1.,30.};
		//Camera sponza_eye = {{-10.407576,1.605258,4.050256},7.859994,-0.050000};
		//Camera sponza_light = {{-7.109047,5.130751,-2.025017},0.404998,2.950001};
		//Camera sponza_eye = {{13.924,7.606,1.007},7.920,-0.150,1.3,100.0,0.3,60.0};// shows face with bad indirect
		//Camera sponza_light = {{-8.042,7.690,-0.954},1.990,0.800,1.0,70.0,1.0,30.0};
		eye = tmpeye;
		light = tmplight;
//		if(areaLight) areaLight->setNumInstances(1);
	}
	if(strstr(filename_3ds, "sibenik"))
	{
		// zacatek nevhodny pouze kvuli spatnym normalam
		//		Camera tmpeye = {{-8.777,3.117,0.492},1.145,-0.400,1.3,50.0,0.3,80.0};
		//		Camera tmplight = {{-0.310,2.952,-0.532},5.550,3.200,1.0,70.0,1.0,40.0};
		// dalsi zacatek kde je videt zmrsena normala
		de::Camera tmpeye = {{-3.483,5.736,2.755},7.215,2.050,1.3,50.0,0.3,80.0};
		de::Camera tmplight = {{-1.872,5.494,0.481},0.575,0.950,1.0,70.0,1.6,40.0};
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
//		if(areaLight) areaLight->setNumInstances(1);
	}

	printf("Loading %s...",filename_3ds);

	// init .3ds scene
	if(!m3ds.Load(filename_3ds,scale_3ds))
		error("",false);

	//	printf(solver->getObject(0)->getCollider()->getMesh()->save("c:\\a")?"saved":"not saved");
	//	printf(solver->getObject(0)->getCollider()->getMesh()->load("c:\\a")?" / loaded":" / not loaded");

	printf("\n");

	// init radiosity solver
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	rr::RRScene::SmoothingParameters sp;
	sp.subdivisionSpeed = SUBDIVISION;
	insert3dsToRR(&m3ds,solver,&sp);
	solver->calculate(); // creates radiosity solver with multiobject. without renderer, no primary light is detected
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init renderer
	rendererNonCaching = new rr_gl::RendererOfRRObject(solver->getMultiObjectCustom(),solver->getScene(),solver->getScaler());
	rendererCaching = new de::RendererWithCache(rendererNonCaching);
	// next calculate will use renderer to detect primary illum. must be called from mainloop, we don't know winWidth/winHeight yet

	// init bugs
#ifdef BUGS
	bugs = Bugs::create(solver->getScene(),solver->getMultiObjectCustom(),100);
#endif

	updateMatrices();
	needDepthMapUpdate = true;
	needRedisplay = true;
	//readMap("bsp\\trajectory\\maps\\trajectory.bsp",bsp);
}

Level::~Level()
{
	//freeMap(bsp);
	delete bugs;
	delete rendererCaching;
	delete rendererNonCaching;
	delete solver->getScaler();
	delete solver;
}


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display()
{
	if(!winWidth) return; // can't work without window
	//printf("<Display.>");
	if(!level)
	{
		showImage(loadingMap);
		showImage(loadingMap); // neznamo proc jeden show nekdy nestaci na spravny uvodni obrazek
		level = new Level(levelSequence.getNextLevel());
#ifdef BUGS
		for(unsigned i=0;i<6;i++)
#else
		for(unsigned i=0;i<12;i++)
#endif
			level->solver->calculate();
	}
	if(showHint)
	{
		showImage(hintMap);
		return;
	}

	// pro jednoduchost je to tady
	// kdyby to bylo u vsech stisku/pusteni/pohybu klaves/mysi a animaci,
	//  nevolalo by se to zbytecne v situaci kdy redisplay vyvola calculate() hlaskou ze zlepsil osvetleni
	// zisk by ale byl miniaturni
	level->solver->reportInteraction();

	if(needMatrixUpdate)
		updateMatrices();

	needRedisplay = 0;

	switch(drawMode)
	{
		case DM_EYE_VIEW_SHADOWED:
			{
				updateDepthMap(0,0);
				de::UberProgramSetup uberProgramSetup = uberProgramGlobalSetup;
				uberProgramSetup.SHADOW_MAPS = 1;
				uberProgramSetup.SHADOW_SAMPLES = 1;
				uberProgramSetup.LIGHT_DIRECT = true;
				uberProgramSetup.LIGHT_DIRECT_MAP = true;
				uberProgramSetup.LIGHT_INDIRECT_CONST = true;
				uberProgramSetup.LIGHT_INDIRECT_COLOR = false;
				uberProgramSetup.LIGHT_INDIRECT_MAP = false;
				uberProgramSetup.LIGHT_INDIRECT_ENV = false;
				//uberProgramSetup.MATERIAL_DIFFUSE = ;
				//uberProgramSetup.MATERIAL_DIFFUSE_COLOR = ;
				//uberProgramSetup.MATERIAL_DIFFUSE_MAP = ;
				//uberProgramSetup.MATERIAL_SPECULAR = ;
				//uberProgramSetup.MATERIAL_SPECULAR_MAP = ;
				//uberProgramSetup.MATERIAL_NORMAL_MAP = ;
				//uberProgramSetup.OBJECT_SPACE = false;
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
	//printf("cache: hits=%d misses=%d",rr::RRScene::getSceneStatistics()->numIrradianceCacheHits,rr::RRScene::getSceneStatistics()->numIrradianceCacheMisses);
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
	level->solver->reportLightChange(true);
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
	level->solver->reportLightChange(level->solver->getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles()>10000?true:false);
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

void special(int c, int x, int y)
{
	if(showHint)
	{
		showHint = false;
		return;
	}

	int modif = glutGetModifiers();
	float scale = 1;
	if(modif&GLUT_ACTIVE_SHIFT) scale=10;
	if(modif&GLUT_ACTIVE_CTRL) scale=3;
	if(modif&GLUT_ACTIVE_ALT) scale=0.1f;

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
			{printf("\nde::Camera tmpeye = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",eye.pos[0],eye.pos[1],eye.pos[2],eye.angle,eye.height,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			printf("de::Camera tmplight = {{%.3f,%.3f,%.3f},%.3f,%.3f,%.1f,%.1f,%.1f,%.1f};\n",light.pos[0],light.pos[1],light.pos[2],light.angle,light.height,light.aspect,light.fieldOfView,light.anear,light.afar);
			for(unsigned i=0;i<DYNAOBJECTS;i++)
				printf("dynaobjects->setPos(%d,rr::RRVec3(%ff,%ff,%ff));\n",i,dynaobjects->getPos(i)[0],dynaobjects->getPos(i)[1],dynaobjects->getPos(i)[2]);
			return;}

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
	}
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
			delete level;
			delete dynaobjects;
			done_gl_resources();
			exit(0);
			break;
		case 9:
			modeMovingEye = !modeMovingEye;
			break;
		case 'p':
			paused = !paused;
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
				rr::RRVec3 dir = rr::RRVec3(eye.dir[0],eye.dir[1],eye.dir[2]).normalized();
				ray->rayOrigin = rr::RRVec3(eye.pos[0],eye.pos[1],eye.pos[2]);
				ray->rayDirInv[0] = 1/dir[0];
				ray->rayDirInv[1] = 1/dir[1];
				ray->rayDirInv[2] = 1/dir[2];
				ray->rayLengthMin = 0;
				ray->rayLengthMax = 1000;
				ray->rayFlags = rr::RRRay::FILL_POINT3D;
				if(level->solver->getMultiObjectCustom()->getCollider()->intersect(ray))
				{
					dynaobjects->setPos(c-'1',ray->hitPoint3d);
				}
			}
			break;

		case 'v':
			renderLightmaps = !renderLightmaps;
			if(!renderLightmaps)
			{
				needLightmapCacheUpdate = true;
				for(unsigned i=0;i<level->solver->getNumObjects();i++)
				{
					//!!! doresit obecne, ne jen pro channel 0
					delete level->solver->getIllumination(i)->getChannel(0)->pixelBuffer;
					level->solver->getIllumination(i)->getChannel(0)->pixelBuffer = NULL;
				}
			}
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
			needMatrixUpdate = 1;
			break;
		case 'z':
			if(eye.fieldOfView>25) eye.fieldOfView -= 25;
			needMatrixUpdate = 1;
			break;
		case 13:
			changeSpotlight();
			break;
			/*
		case 'a':
			++areaLight->areaType%=3;
			needDepthMapUpdate = 1;
			break;
		case 'S':
			solver->reportLightChange(true);
			break;
		case 'w':
		case 'W':
			toggleWireFrame();
			return;*/
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
			break;/*
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
		case '1':
		case 'a':
		case 'A':
			specialUp(GLUT_KEY_LEFT,0,0);
			break;
		case '2':
		case 's':
		case 'S':
			specialUp(GLUT_KEY_DOWN,0,0);
			break;
		case '3':
		case 'd':
		case 'D':
			specialUp(GLUT_KEY_RIGHT,0,0);
			break;
		case '5':
		case 'w':
		case 'W':
			specialUp(GLUT_KEY_UP,0,0);
			break;
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	needMatrixUpdate = 1;
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
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;//timer.Watch();
		CLAMP(seconds,0.001f,0.3f);
		de::Camera* cam = modeMovingEye?&eye:&light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			//printf(" %f ",seconds);
			if(cam==&light) reportLightMovement(); else reportEyeMovement();
		}
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
	if(animated)
	{
		needDepthMapUpdate = 1;
		needRedisplay = 1;
	}

//	LIMITED_TIMES(1,timer.Start());
	bool rrOn = drawMode == DM_EYE_VIEW_SOFTSHADOWED;
//	printf("[--- %d %d %d %d",rrOn?1:0,movingEye?1:0,updateDuringLightMovement?1:0,movingLight?1:0);
	// pri kalkulaci nevznikne improve -> neni read results -> aplikace neda display -> pristi calculate je dlouhy
	// pokud se ale hybe svetlem, aplikace da display -> pristi calculate je kratky
	if(!level || (rrOn && level->solver->calculate()==rr::RRScene::IMPROVED) || needRedisplay || gameOn)
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
		if (!strcmp("-ATI", argv[i])) {
			ati = 1;
		}
		if (!strcmp("-window", argv[i])) {
			fullscreen = 0;
		}
		if (!strcmp("-noAreaLight", argv[i])) {
			startWithSoftShadows = 0;
		}
		if (strstr(argv[i], ".3ds") || strstr(argv[i], ".3DS")) {
			levelSequence.insertLevelFront(argv[i]);
		}
	}
}

#include "../DemoEngine/TextureShadowMap.h"

int main(int argc, char **argv)
{
	//_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 33935;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}

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
		glutSetCursor(GLUT_CURSOR_NONE);
	}
	else
	{
		glutCreateWindow("Realtime Radiosity");
		glutFullScreen();
	}
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

	// init GLEW
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	init_gl_states();

	updateMatrices(); // needed for startup without area lights (areaLight doesn't update matrices for 1 instance)

	dynaobjects = new DynamicObjects();

	uberProgramGlobalSetup.SHADOW_MAPS = 1;
	uberProgramGlobalSetup.SHADOW_SAMPLES = 4;
	uberProgramGlobalSetup.LIGHT_DIRECT = true;
	uberProgramGlobalSetup.LIGHT_DIRECT_MAP = true;
	uberProgramGlobalSetup.LIGHT_INDIRECT_CONST = false;
	uberProgramGlobalSetup.LIGHT_INDIRECT_COLOR = !renderLightmaps;
	uberProgramGlobalSetup.LIGHT_INDIRECT_MAP = renderLightmaps;
	uberProgramGlobalSetup.LIGHT_INDIRECT_ENV = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE = true;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_COLOR = false;
	uberProgramGlobalSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramGlobalSetup.MATERIAL_SPECULAR = false;
	uberProgramGlobalSetup.MATERIAL_SPECULAR_MAP = false;
	uberProgramGlobalSetup.MATERIAL_NORMAL_MAP = false;
	uberProgramGlobalSetup.OBJECT_SPACE = false;
	uberProgramGlobalSetup.FORCE_2D_POSITION = false;

	// init shaders
	// init textures
	init_gl_resources();

	// adjust INSTANCES_PER_PASS to GPU
	INSTANCES_PER_PASS = de::UberProgramSetup::detectMaxShadowmaps(uberProgram,INSTANCES_PER_PASS);
	if(ati && INSTANCES_PER_PASS>1) INSTANCES_PER_PASS--;
	if(!INSTANCES_PER_PASS) error("",true);
	areaLight->setNumInstances(startWithSoftShadows?INITIAL_PASSES*INITIAL_INSTANCES_PER_PASS:1);

	if(rr::RRLicense::loadLicense("licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.",false);

	glutMainLoop();
	return 0;
}
