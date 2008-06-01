// --------------------------------------------------------------------------
// Viewer of scene.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/Timer.h"
#include "LightmapViewer.h"
#include <cstdio>
#include <GL/glew.h>
#include <GL/glut.h>

#define DEBUG_TEXEL

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

static void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if(gfxRelated)
		rr::RRReporter::report(rr::INF1,"Please update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families\n");
	if(glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
		glutLeaveGameMode();
	else
		glutDestroyWindow(glutGetWindow());
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

static class Solver*              solver = NULL;
static Camera                     eye(-1.856f,1.440f,2.097f, 2.404f,0,-0.3f, 1.3f, 90, 0.1f,1000);
enum SelectionType {ST_CAMERA, ST_LIGHT, ST_OBJECT};
static SelectionType              selectedType = ST_CAMERA;
static unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
static unsigned                   selectedObjectIndex = 0; // index into static objects
static int                        winWidth = 0; // current size
static int                        winHeight = 0; // current size
static bool                       fullscreen = 0; // current mode
static int                        windowCoord[4] = {0,0,800,600}; // x,y,w,h of window when user switched to fullscreen
static bool                       renderRealtime = 1;
static bool                       render2d = 0;
static bool                       renderAmbient = 0;
static bool                       renderDiffuse = 1;
static bool                       renderEmission = 1;
static bool                       renderTransparent = 1;
static bool                       renderTextures = 1;
static bool                       renderTonemapping = 1;
static ToneMapping*               toneMapping = NULL;
static bool                       renderWireframe = 0;
static bool                       renderHelpers = 0;
static bool                       fireballLoadAttempted = 1;
static float                      speedGlobal = 2; // speed of movement controlled by user
static float                      speedForward = 0;
static float                      speedBack = 0;
static float                      speedRight = 0;
static float                      speedLeft = 0;
static float                      speedUp = 0;
static float                      speedDown = 0;
static float                      speedLean = 0;
static rr::RRVec4                 brightness(1);
static float                      gamma = 1;
static bool                       exitRequested = 0;
static int                        menuHandle = 0;
static bool                       bilinear = 1;
static bool                       ourEnv = 0; // whether environment is owned by us
static rr::RRLights               lightsToBeDeletedOnExit; // list of lights owned by us
static LightmapViewer*            lv = NULL; // 2d lightmap viewer
static unsigned                   layerNumber = 0; // layer used for all static lighting operations
static unsigned                   centerObject = UINT_MAX; // object in the middle of screen
static unsigned                   centerTexel = UINT_MAX; // texel in the middle of screen
static unsigned                   centerTriangle = UINT_MAX; // triangle in the middle of screen, multiObjPostImport
static int                        menuInUse = GLUT_MENU_NOT_IN_USE; // GLUT_MENU_IN_USE or GLUT_MENU_NOT_IN_USE

// all we need for testing lightfield
static const rr::RRLightField*    lightField = NULL;
static GLUquadricObj*             lightFieldQuadric = NULL;
static rr::RRObjectIllumination*  lightFieldObjectIllumination = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// log of rays

namespace ray_log
{
	enum {MAX_RAYS=10000};
	struct Ray
	{
		rr::RRVec3 begin;
		rr::RRVec3 end;
		bool infinite;
		bool unreliable;
	};
	static Ray log[MAX_RAYS];
	static unsigned size = 0;
	static void push_back(const rr::RRRay* ray, bool hit)
	{
		if(size<MAX_RAYS)
		{
			log[size].begin = ray->rayOrigin;
			log[size].end = hit ? ray->hitPoint3d : ray->rayOrigin+rr::RRVec3(1/ray->rayDirInv[0],1/ray->rayDirInv[1],1/ray->rayDirInv[2])*ray->rayLengthMax;
			log[size].infinite = !hit;
			log[size].unreliable = false;
			size++;
		}
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// GI solver and renderer

class Solver : public RRDynamicSolverGL
{
public:
	RendererOfScene* rendererOfScene;

	Solver(const char* _pathToShaders) : RRDynamicSolverGL(_pathToShaders)
	{
		pathToShaders = _strdup(_pathToShaders);
		rendererOfScene = new RendererOfScene(this,pathToShaders);
	}
	~Solver()
	{
		free(pathToShaders);
		delete rendererOfScene;
	}
	void resetRenderCache()
	{
		delete rendererOfScene;
		rendererOfScene = new RendererOfScene(this,pathToShaders);
	}
	virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
	{
		const rr::RRVector<RealtimeLight*>* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

		// render static scene
		rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight,honourExpensiveLightingShadowingFlags);

		if(renderRealtime)
			rendererOfScene->useOptimizedScene();
		else
			rendererOfScene->useOriginalScene(layerNumber);

		rendererOfScene->setBrightnessGamma(&brightness,gamma);
		rendererOfScene->render();
	}
	void dirtyLights()
	{
		for(unsigned i=0;i<realtimeLights.size();i++)
		{
			solver->reportDirectIlluminationChange(i,true,true);
		}
	}

protected:
	char* pathToShaders;
};


/////////////////////////////////////////////////////////////////////////////
//
// menu

class Menu
{
public:
	Menu(Solver* solver)
	{
		create();
		menuInUse = GLUT_MENU_NOT_IN_USE;
	}
	static void create()
	{
		// Select...
		int selectHandle = glutCreateMenu(selectCallback);
		char buf[100];
		glutAddMenuEntry("camera", -1);
		for(unsigned i=0;i<solver->getLights().size();i++)
		{
			sprintf(buf,"light %d",i);
			glutAddMenuEntry(buf, i);
		}
		for(unsigned i=0;i<solver->getStaticObjects().size();i++)
		{
			sprintf(buf,"object %d",i);
			glutAddMenuEntry(buf, 1000+i);
		}

		// Lights...
		int lightsHandle = glutCreateMenu(lightsCallback);
		glutAddMenuEntry("Insert dir light", ME_LIGHT_DIR);
		glutAddMenuEntry("Insert spot light", ME_LIGHT_SPOT);
		glutAddMenuEntry("Insert point light", ME_LIGHT_POINT);
		glutAddMenuEntry("Delete selected light", ME_LIGHT_DELETE);

		// Static lighting...
		int staticHandle = glutCreateMenu(staticCallback);
		glutAddMenuEntry("Render static lighting", ME_STATIC_3D);
		glutAddMenuEntry("       static lighting in 2D", ME_STATIC_2D);
		glutAddMenuEntry("Toggle bilinear interpolation", ME_STATIC_BILINEAR);
		glutAddMenuEntry("Assign empty vertex buffers",0);
		glutAddMenuEntry("       empty lightmaps 16x16",-16);
		glutAddMenuEntry("       empty lightmaps 64x64",-64);
		glutAddMenuEntry("       empty lightmaps 256x256",-256);
		glutAddMenuEntry("       empty lightmaps 1024x1024",-1024);
		glutAddMenuEntry("Build all, quality 1",1);
		glutAddMenuEntry("           quality 10",10);
		glutAddMenuEntry("           quality 100",100);
		glutAddMenuEntry("           quality 1000",1000);
		glutAddMenuEntry("Build selected obj, only direct",ME_STATIC_BUILD1);
#ifdef DEBUG_TEXEL
		glutAddMenuEntry("Diagnose texel, quality 1", ME_STATIC_DIAGNOSE1);
		glutAddMenuEntry("                quality 10", ME_STATIC_DIAGNOSE10);
		glutAddMenuEntry("                quality 100", ME_STATIC_DIAGNOSE100);
		glutAddMenuEntry("                quality 1000", ME_STATIC_DIAGNOSE1000);
#endif
		glutAddMenuEntry("Build lightfield",ME_STATIC_BUILD_LIGHTFIELD);
		glutAddMenuEntry("Save",ME_STATIC_SAVE);
		glutAddMenuEntry("Load",ME_STATIC_LOAD);

		// Fireball
		int fireballHandle = glutCreateMenu(fireballCallback);
		glutAddMenuEntry("Render realtime GI: architect", ME_REALTIME_ARCHITECT);
		glutAddMenuEntry("Render realtime GI: fireball", ME_REALTIME_FIREBALL);
		glutAddMenuEntry("Build fireball, quality 10", 10);
		glutAddMenuEntry("                quality 100", 100);
		glutAddMenuEntry("                quality 1000", 1000);
		glutAddMenuEntry("                quality 10000", 10000);

		// Movement speed...
		int speedHandle = glutCreateMenu(speedCallback);
		glutAddMenuEntry("0.001 m/s", 1);
		glutAddMenuEntry("0.01 m/s", 10);
		glutAddMenuEntry("0.1 m/s", 100);
		glutAddMenuEntry("0.5 m/s", 500);
		glutAddMenuEntry("2 m/s", 2000);
		glutAddMenuEntry("10 m/s", 10000);
		glutAddMenuEntry("100 m/s", 100000);
		glutAddMenuEntry("1000 m/s", 1000000);

		// Environment...
		int envHandle = glutCreateMenu(envCallback);
		glutAddMenuEntry("Set white", ME_ENV_WHITE);
		glutAddMenuEntry("Set black", ME_ENV_BLACK);
		glutAddMenuEntry("Set white top", ME_ENV_WHITE_TOP);

		// main menu
		menuHandle = glutCreateMenu(mainCallback);
		glutAddSubMenu("Select...", selectHandle);
		glutAddSubMenu("Lights...", lightsHandle);
		glutAddSubMenu("Realtime lighting...", fireballHandle);
		glutAddSubMenu("Static lighting...", staticHandle);
		glutAddSubMenu("Movement speed...", speedHandle);
		glutAddSubMenu("Environment...", envHandle);
		glutAddMenuEntry(renderAmbient?"Disable const ambient":"Enable const ambient", ME_RENDER_AMBIENT);
		glutAddMenuEntry(renderDiffuse?"Disable diffuse":"Enable diffuse", ME_RENDER_DIFFUSE);
		glutAddMenuEntry(renderEmission?"Disable emissivity":"Enable emissivity", ME_RENDER_EMISSION);
		glutAddMenuEntry(renderTransparent?"Disable transparency":"Enable transparency", ME_RENDER_TRANSPARENT);
		glutAddMenuEntry(renderTextures?"Disable textures":"Enable textures", ME_RENDER_TEXTURES);
		glutAddMenuEntry(renderTonemapping?"Disable tone mapping":"Enable tone mapping", ME_RENDER_TONEMAPPING);
		glutAddMenuEntry(renderWireframe?"Disable wireframe":"Wireframe", ME_RENDER_WIREFRAME);
		glutAddMenuEntry(renderHelpers?"Hide helpers":"Show helpers", ME_RENDER_HELPERS);
		glutAddMenuEntry(solver->honourExpensiveLightingShadowingFlags?"Ignore expensive flags":"Honour expensive flags", ME_HONOUR_FLAGS);
		glutAddMenuEntry(fullscreen?"Windowed":"Fullscreen", ME_MAXIMIZE);
		glutAddMenuEntry("Set random camera",ME_RANDOM_CAMERA);
		glutAddMenuEntry("Log solver status",ME_VERIFY);
		glutAddMenuEntry("Quit", ME_CLOSE);
		glutAttachMenu(GLUT_RIGHT_BUTTON);
	}
	static void destroy()
	{
		glutDetachMenu(GLUT_RIGHT_BUTTON);
		glutDestroyMenu(menuHandle);
	}
	~Menu()
	{
		destroy();
	}
	static void mainCallback(int item)
	{
		switch(item)
		{
			case ME_RENDER_AMBIENT: renderAmbient = !renderAmbient; break;
			case ME_RENDER_DIFFUSE: renderDiffuse = !renderDiffuse; break;
			case ME_RENDER_EMISSION: renderEmission = !renderEmission; break;
			case ME_RENDER_TRANSPARENT: renderTransparent = !renderTransparent; break;
			case ME_RENDER_TEXTURES: renderTextures = !renderTextures; break;
			case ME_RENDER_TONEMAPPING: renderTonemapping = !renderTonemapping; break;
			case ME_RENDER_WIREFRAME: renderWireframe = !renderWireframe; break;
			case ME_RENDER_HELPERS: renderHelpers = !renderHelpers; break;
			case ME_HONOUR_FLAGS: solver->honourExpensiveLightingShadowingFlags = !solver->honourExpensiveLightingShadowingFlags; solver->dirtyLights(); break;
			case ME_MAXIMIZE:
				if(!glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
				{
					fullscreen = !fullscreen;
					if(fullscreen)
					{
						windowCoord[0] = glutGet(GLUT_WINDOW_X);
						windowCoord[1] = glutGet(GLUT_WINDOW_Y);
						windowCoord[2] = glutGet(GLUT_WINDOW_WIDTH);
						windowCoord[3] = glutGet(GLUT_WINDOW_HEIGHT);
						glutFullScreen();
					}
					else
					{
						glutReshapeWindow(windowCoord[2],windowCoord[3]);
						glutPositionWindow(windowCoord[0],windowCoord[1]);
					}
				}
				break;
			case ME_RANDOM_CAMERA:
				if(solver->getMultiObjectCustom())
				{
					const rr::RRMesh* multiMesh = solver->getMultiObjectCustom()->getCollider()->getMesh();
					rr::RRVec3 mini,maxi;
					multiMesh->getAABB(&mini,&maxi,NULL);
					unsigned numVertices = multiMesh->getNumVertices();
					if(numVertices)
					{
						multiMesh->getVertex(numVertices*rand()/RAND_MAX,eye.pos);
						eye.pos -= eye.dir*(maxi-mini).sum()*0.22f;
						eye.afar = MAX(eye.anear+1,(maxi-mini).sum()*50);
						speedGlobal = (maxi-mini).sum()*0.1f;
					}
				}
				break;
			case ME_VERIFY: solver->verify(); break;
			case ME_CLOSE: exitRequested = 1; break;
		}
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
		destroy();
		create();
	}
	static void selectCallback(int item)
	{
		if(item<0) selectedType = ST_CAMERA;
		if(item>=0 && item<1000) {selectedType = ST_LIGHT; selectedLightIndex = item;}
		if(item>=1000) {selectedType = ST_OBJECT; selectedObjectIndex = item-1000;}
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void staticCallback(int item)
	{
		switch(item)
		{
			case ME_STATIC_3D:
				renderRealtime = 0;
				render2d = 0;
				break;
			case ME_STATIC_2D:
				renderRealtime = 0;
				render2d = 1;
				break;
			case ME_STATIC_BILINEAR:
				bilinear = !bilinear;
				renderRealtime = false;
				for(unsigned i=0;i<solver->getNumObjects();i++)
				{	
					if(solver->getIllumination(i) && solver->getIllumination(i)->getLayer(layerNumber) && solver->getIllumination(i)->getLayer(layerNumber)->getType()==rr::BT_2D_TEXTURE)
					{
						glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_2D_LIGHT_INDIRECT);
						rr_gl::getTexture(solver->getIllumination(i)->getLayer(layerNumber))->bindTexture();
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear?GL_LINEAR:GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear?GL_LINEAR:GL_NEAREST);
					}
				}
				break;
			case ME_STATIC_LOAD:
				solver->getStaticObjects().loadIllumination("",layerNumber);
				renderRealtime = false;
				break;
			case ME_STATIC_SAVE:
				solver->getStaticObjects().saveIllumination("",layerNumber);
				break;
			case ME_STATIC_BUILD_LIGHTFIELD:
				{
					rr::RRVec3 aabbMin,aabbMax;
					solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
					delete lightField;
					lightField = solver->buildLightField(aabbMin,aabbMax-aabbMin,1);
				}
				break;
			case ME_STATIC_BUILD1:
				{
					// calculate 1 object, direct lighting
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(1000);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmap(selectedObjectIndex,solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber),NULL,NULL,&params,&filtering);
					renderRealtime = false;
					// propagate computed data from buffers to textures
					if(solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber) && solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber)->getType()==rr::BT_2D_TEXTURE)
						getTexture(solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber))->reset(true,false);
					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					solver->resetRenderCache();
				}
				break;
#ifdef DEBUG_TEXEL
			case ME_STATIC_DIAGNOSE1:
			case ME_STATIC_DIAGNOSE10:
			case ME_STATIC_DIAGNOSE100:
			case ME_STATIC_DIAGNOSE1000:
				{
					if(centerObject!=UINT_MAX)
					{
						solver->leaveFireball();
						fireballLoadAttempted = false;
						rr::RRDynamicSolver::UpdateParameters params(item+1-ME_STATIC_DIAGNOSE1);
						params.debugObject = centerObject;
						params.debugTexel = centerTexel;
						params.debugTriangle = UINT_MAX;//centerTriangle;
						params.debugRay = ray_log::push_back;
						ray_log::size = 0;
						solver->updateLightmaps(layerNumber,-1,-1,&params,&params,NULL);
					}
				}
				break;
#endif
			default:
				if(item>0)
				{
					// calculate all
					solver->leaveFireball();
					fireballLoadAttempted = false;
					rr::RRDynamicSolver::UpdateParameters params(item);
					rr::RRDynamicSolver::FilteringParameters filtering;
					filtering.wrap = false;
					solver->updateLightmaps(layerNumber,-1,-1,&params,&params,&filtering);
					renderRealtime = false;
					// propagate computed data from buffers to textures
					for(unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if(solver->getIllumination(i) && solver->getIllumination(i)->getLayer(layerNumber) && solver->getIllumination(i)->getLayer(layerNumber)->getType()==rr::BT_2D_TEXTURE)
							getTexture(solver->getIllumination(i)->getLayer(layerNumber))->reset(true,false);
					}
					// reset cache, GL texture ids constant, but somehow rendered maps are not updated without display list rebuild
					solver->resetRenderCache();
				}
				else
				{
					// allocate buffers
					for(unsigned i=0;i<solver->getStaticObjects().size();i++)
					{
						if(solver->getIllumination(i) && solver->getObject(i)->getCollider()->getMesh()->getNumVertices())
						{
							delete solver->getIllumination(i)->getLayer(layerNumber);
							solver->getIllumination(i)->getLayer(layerNumber) = item
								? rr::RRBuffer::create(rr::BT_2D_TEXTURE,-item,-item,1,rr::BF_RGB,true,NULL)
								: rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
						}
					}
					// reset cache, GL texture ids changed
					if(item)
						solver->resetRenderCache();
				}
				break;
		}
		// leaving menu, mouse is not in the screen center -> center it
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void fireballCallback(int item)
	{
		switch(item)
		{
			case ME_REALTIME_FIREBALL:
				renderRealtime = 1;
				render2d = 0;
				solver->dirtyLights();
				if(!fireballLoadAttempted) 
				{
					fireballLoadAttempted = true;
					solver->loadFireball(NULL);
				}
				break;
			case ME_REALTIME_ARCHITECT:
				renderRealtime = 1;
				render2d = 0;
				solver->dirtyLights();
				fireballLoadAttempted = false;
				solver->leaveFireball();
				break;
			default:
				renderRealtime = 1;
				render2d = 0;
				solver->buildFireball(item,NULL);
				solver->dirtyLights();
				fireballLoadAttempted = true;
				break;
		}
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void speedCallback(int item)
	{
		speedGlobal = item/1000.f;
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void envCallback(int item)
	{
		if(ourEnv)
			delete solver->getEnvironment();
		ourEnv = true;
		switch(item)
		{
			case ME_ENV_WHITE: solver->setEnvironment(rr::RRBuffer::createSky()); break;
			case ME_ENV_BLACK: solver->setEnvironment(NULL); break;
			case ME_ENV_WHITE_TOP: solver->setEnvironment(rr::RRBuffer::createSky(rr::RRVec4(1),rr::RRVec4(0))); break;
		}
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void lightsCallback(int item)
	{
		rr::RRLights newList = solver->getLights();
		rr::RRLight* newLight = NULL;
		switch(item)
		{
			case ME_LIGHT_DIR: newLight = rr::RRLight::createDirectionalLight(rr::RRVec3(-1),rr::RRVec3(1),true); break;
			case ME_LIGHT_SPOT: newLight = rr::RRLight::createSpotLight(eye.pos,rr::RRVec3(1),eye.dir,eye.fieldOfView*(3.14f/180/2),eye.fieldOfView*(3.14f/180/4)); break;
			case ME_LIGHT_POINT: newLight = rr::RRLight::createPointLight(eye.pos,rr::RRVec3(1)); break;
			case ME_LIGHT_DELETE:
				if(selectedLightIndex<solver->realtimeLights.size())
				{
					newList.erase(selectedLightIndex);
					if(selectedLightIndex && selectedLightIndex==newList.size())
						selectedLightIndex--;
					if(!newList.size())
						selectedType = ST_CAMERA;
				}
				break;
		}
		if(newLight)
		{
			lightsToBeDeletedOnExit.push_back(newLight);
			newList.push_back(newLight);
		}
		solver->setLights(newList);
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
		destroy();
		create();
	}
	enum
	{
		ME_RENDER_AMBIENT,
		ME_RENDER_DIFFUSE,
		ME_RENDER_EMISSION,
		ME_RENDER_TRANSPARENT,
		ME_RENDER_TEXTURES,
		ME_RENDER_TONEMAPPING,
		ME_RENDER_WIREFRAME,
		ME_RENDER_HELPERS,
		ME_HONOUR_FLAGS,
		ME_MAXIMIZE,
		ME_CLOSE,
		ME_ENV_WHITE,
		ME_ENV_BLACK,
		ME_ENV_WHITE_TOP,
		ME_LIGHT_DIR,
		ME_LIGHT_SPOT,
		ME_LIGHT_POINT,
		ME_LIGHT_DELETE,
		ME_RANDOM_CAMERA,
		ME_VERIFY,
		// ME_REALTIME/STATIC must not collide with 1,10,100,1000,10000
		ME_REALTIME_FIREBALL = 1234,
		ME_REALTIME_ARCHITECT,
		ME_STATIC_3D,
		ME_STATIC_2D,
		ME_STATIC_BILINEAR,
		ME_STATIC_BUILD1,
#ifdef DEBUG_TEXEL
		ME_STATIC_DIAGNOSE1    = 10001,
		ME_STATIC_DIAGNOSE10   = 10010,
		ME_STATIC_DIAGNOSE100  = 10100,
		ME_STATIC_DIAGNOSE1000 = 11000,
#endif
		ME_STATIC_LOAD,
		ME_STATIC_SAVE,
		ME_STATIC_BUILD_LIGHTFIELD,
	};
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

static void special(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 1; break;
		case GLUT_KEY_DOWN: speedBack = 1; break;
		case GLUT_KEY_LEFT: speedLeft = 1; break;
		case GLUT_KEY_RIGHT: speedRight = 1; break;
		case GLUT_KEY_PAGE_UP: speedUp = 1; break;
		case GLUT_KEY_PAGE_DOWN: speedDown = 1; break;
	}
	glutPostRedisplay();
}

static void specialUp(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 0; break;
		case GLUT_KEY_DOWN: speedBack = 0; break;
		case GLUT_KEY_LEFT: speedLeft = 0; break;
		case GLUT_KEY_RIGHT: speedRight = 0; break;
		case GLUT_KEY_PAGE_UP: speedUp = 0; break;
		case GLUT_KEY_PAGE_DOWN: speedDown = 0; break;
	}
}

static void keyboard(unsigned char c, int x, int y)
{
	switch (c)
	{
		case '+': brightness *= 1.2f; break;
		case '-': brightness /= 1.2f; break;
		case '*': gamma *= 1.2f; break;
		case '/': gamma /= 1.2f; break;

		case '[': if(solver->getNumObjects()) selectedObjectIndex = (selectedObjectIndex+solver->getNumObjects()-1)%solver->getNumObjects(); break;
		case ']': if(solver->getNumObjects()) selectedObjectIndex = (selectedObjectIndex+1)%solver->getNumObjects(); break;

		case 'a':
		case 'A': speedLeft = 1; break;
		case 's':
		case 'S': speedBack = 1; break;
		case 'd':
		case 'D': speedRight = 1; break;
		case 'w':
		case 'W': speedForward = 1; break;
		case 'q':
		case 'Q': speedUp = 1; break;
		case 'z':
		case 'Z': speedDown = 1; break;
		case 'x':
		case 'X': speedLean = -1; break;
		case 'c':
		case 'C': speedLean = +1; break;

		case 27: if(render2d) render2d = 0;
			//else exitRequested = 1;
			// Exit by esc is disabled because of GLUT error:
			//  If you create menu by right click and then press esc, menu disappears, mouse is locked.
			//  The only way to fix it is to press any mouse button.
			//  However, if you exit viewer (by hotkey) when mouse is locked, GLUT crashes or calls exit().
			//  So it's safer to don't offer any 'quit' hotkey, use menu.
			break;
	}
	solver->reportInteraction();
	glutPostRedisplay();
}

static void keyboardUp(unsigned char c, int x, int y)
{
	switch (c)
	{
		case 'a':
		case 'A': speedLeft = 0; break;
		case 's':
		case 'S': speedBack = 0; break;
		case 'd':
		case 'D': speedRight = 0; break;
		case 'w':
		case 'W': speedForward = 0; break;
		case 'q':
		case 'Q': speedUp = 0; break;
		case 'z':
		case 'Z': speedDown = 0; break;
		case 'x':
		case 'X': speedLean = 0; break;
		case 'c':
		case 'C': speedLean = 0; break;
	}
}

static void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = winWidth/(float)winHeight;
	Texture* texture = Texture::createShadowmap(64,64);
	GLint shadowDepthBits = texture->getTexelBits();
	delete texture;
	glPolygonOffset(1.2f,(GLfloat)(20<<(shadowDepthBits-16)));
}

static void mouse(int button, int state, int x, int y)
{
	if(render2d && lv)
	{
		LightmapViewer::mouse(button,state,x,y);
		return;
	}
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && solver->realtimeLights.size())
	{
		if(selectedType!=ST_CAMERA) selectedType = ST_CAMERA;
		else selectedType = ST_LIGHT;
	}
#ifdef GLUT_WITH_WHEEL_AND_LOOP
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
		else eye.fieldOfView /= 1.4f;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView*1.4f<=3) eye.fieldOfView *= 1.4f;
		else if(eye.fieldOfView<130) eye.fieldOfView+=10;
	}
#endif
	solver->reportInteraction();
}

static void passive(int x, int y)
{
	if(menuInUse==GLUT_MENU_IN_USE) return;
	if(render2d && lv)
	{
		LightmapViewer::passive(x,y);
		return;
	}
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		if(selectedType==ST_CAMERA || selectedType==ST_OBJECT)
		{
			eye.angle -= 0.005f*x*(eye.fieldOfView/90);
			eye.angleX -= 0.005f*y*(eye.fieldOfView/90);
			CLAMP(eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
		}
		else
		if(selectedType==ST_LIGHT)
		{
			if(selectedLightIndex<solver->getLights().size())
			{
				solver->reportDirectIlluminationChange(selectedLightIndex,true,true);
				Camera* light = solver->realtimeLights[selectedLightIndex]->getParent();
				light->angle -= 0.005f*x;
				light->angleX -= 0.005f*y;
				CLAMP(light->angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
				// changes position a bit, together with rotation
				// if we don't call it, solver updates light in a standard way, without position change
				light->pos += light->dir*0.3f;
				light->update();
				light->pos -= light->dir*0.3f;
			}
		}
		if(winWidth) glutWarpPointer(winWidth/2,winHeight/2);
		solver->reportInteraction();
	}
}

static void textOutput(int x, int y, const char *format, ...)
{
	if(y>=winHeight) return; // if we render text below screen, GLUT stops rendering all texts including visible 
	char text[1000];
	va_list argptr;
	va_start (argptr,format);
	vsprintf (text,format,argptr);
	va_end (argptr);
	glRasterPos2i(x,y);
	int len = (int)strlen(text);
	for(int i=0;i<len;i++) glutBitmapCharacter(GLUT_BITMAP_9_BY_15,text[i]);
}

static void display(void)
{
	rr::RRReportInterval report(rr::INF3,"display...\n");
	if(render2d && lv)
	{
		LightmapViewer::setObject(solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber),solver->getObject(selectedObjectIndex)->getCollider()->getMesh(),bilinear);
		LightmapViewer::display();
	}
	else
	{
		if(exitRequested || !winWidth || !winHeight) return; // can't display without window

		eye.update();

		{
			rr::RRReportInterval report(rr::INF3,"calculate...\n");
			rr::RRDynamicSolver::CalculateParameters params;
			//params.qualityIndirectDynamic = 6;
			params.qualityIndirectStatic = 10000;
			solver->calculate(&params);
		}

		{
			rr::RRReportInterval report(rr::INF3,"render scene...\n");
			glClear(GL_DEPTH_BUFFER_BIT);
			eye.setupForRender();
			UberProgramSetup uberProgramSetup;
			uberProgramSetup.SHADOW_MAPS = 1;
			uberProgramSetup.SHADOW_SAMPLES = 1;
			uberProgramSetup.LIGHT_DIRECT = renderRealtime;
			uberProgramSetup.LIGHT_DIRECT_COLOR = renderRealtime;
			uberProgramSetup.LIGHT_DIRECT_ATT_SPOT = renderRealtime;
			uberProgramSetup.LIGHT_INDIRECT_CONST = renderAmbient;
			uberProgramSetup.LIGHT_INDIRECT_auto = true;
			uberProgramSetup.MATERIAL_DIFFUSE = true;
			uberProgramSetup.MATERIAL_DIFFUSE_CONST = renderDiffuse && !renderTextures;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = renderDiffuse && renderTextures;
			uberProgramSetup.MATERIAL_EMISSIVE_CONST = renderEmission;// && !renderTextures;
			uberProgramSetup.MATERIAL_EMISSIVE_MAP = 0;//renderEmission && renderTextures; ... we don't yet need emissive _maps_
			uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = renderTransparent && !renderTextures;
			uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = renderTransparent && renderTextures;
			uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = solver->numTransparencyChannels!=3; // autodetected for whole static scene
			uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
			uberProgramSetup.POSTPROCESS_GAMMA = true;
			if(renderWireframe) {glClear(GL_COLOR_BUFFER_BIT); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);}
			solver->renderScene(uberProgramSetup,NULL);
			if(renderWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			if(renderTonemapping && !renderWireframe && (solver->getLights().size() || uberProgramSetup.LIGHT_INDIRECT_CONST)) // disable adjustment in completely dark scene
			{
				static TIME oldTime = 0;
				TIME newTime = GETTIME;
				float secondsSinceLastFrame = (newTime-oldTime)/float(PER_SEC);
				if(secondsSinceLastFrame>0 && secondsSinceLastFrame<10 && oldTime)
					toneMapping->adjustOperator(secondsSinceLastFrame,brightness,gamma);
				oldTime = newTime;
			}
		}

		if(renderHelpers)
		{
			rr::RRReportInterval report(rr::INF3,"render helpers 1...\n");
			// render light field
			if(lightField)
			{
				// update cube
				lightFieldObjectIllumination->envMapWorldCenter = rr::RRVec3(eye.pos[0]+eye.dir[0],eye.pos[1]+eye.dir[1],eye.pos[2]+eye.dir[2]);
				rr::RRVec2 sphereShift = rr::RRVec2(eye.dir[2],-eye.dir[0]).normalized()*0.05f;
				lightField->updateEnvironmentMap(lightFieldObjectIllumination);

				// diffuse
				// set shader (no direct light)
				UberProgramSetup uberProgramSetup;
				uberProgramSetup.LIGHT_INDIRECT_ENV = true;
				uberProgramSetup.POSTPROCESS_BRIGHTNESS = brightness!=rr::RRVec4(1);
				uberProgramSetup.POSTPROCESS_GAMMA = gamma!=1;
				uberProgramSetup.MATERIAL_DIFFUSE = true;
				uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&brightness,gamma);
				glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_DIFFUSE);
				getTexture(lightFieldObjectIllumination->diffuseEnvMap,false,false)->reset(false,false);
				getTexture(lightFieldObjectIllumination->diffuseEnvMap,false,false)->bindTexture();
				// render
				glPushMatrix();
				glTranslatef(lightFieldObjectIllumination->envMapWorldCenter[0]-sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]-sphereShift[1]);
				gluSphere(lightFieldQuadric, 0.05f, 16, 16);
				glPopMatrix();

				// specular
				// set shader (no direct light)
				uberProgramSetup.MATERIAL_DIFFUSE = false;
				uberProgramSetup.MATERIAL_SPECULAR = true;
				uberProgramSetup.OBJECT_SPACE = true;
				Program* program = uberProgramSetup.useProgram(solver->getUberProgram(),NULL,0,&brightness,gamma);
				program->sendUniform("worldEyePos",eye.pos[0],eye.pos[1],eye.pos[2]);
				glActiveTexture(GL_TEXTURE0+rr_gl::TEXTURE_CUBE_LIGHT_INDIRECT_SPECULAR);
				getTexture(lightFieldObjectIllumination->specularEnvMap,false,false)->reset(false,false);
				getTexture(lightFieldObjectIllumination->specularEnvMap,false,false)->bindTexture();
				// render
				float worldMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, lightFieldObjectIllumination->envMapWorldCenter[0]+sphereShift[0],lightFieldObjectIllumination->envMapWorldCenter[1],lightFieldObjectIllumination->envMapWorldCenter[2]+sphereShift[1],1};
				program->sendUniform("worldMatrix",worldMatrix,false,4);
				gluSphere(lightFieldQuadric, 0.05f, 16, 16);
			}

			// render light frames
			solver->renderLights();

			// render lines
			glBegin(GL_LINES);
			enum {LINES=100, SIZE=100};
			for(unsigned i=0;i<LINES+1;i++)
			{
				if(i==LINES/2)
				{
					glColor3f(0,0,1);
					glVertex3f(0,-0.5*SIZE,0);
					glVertex3f(0,+0.5*SIZE,0);
				}
				else
					glColor3f(0,0,0.3f);
				float q = (i/float(LINES)-0.5f)*SIZE;
				glVertex3f(q,0,-0.5*SIZE);
				glVertex3f(q,0,+0.5*SIZE);
				glVertex3f(-0.5*SIZE,0,q);
				glVertex3f(+0.5*SIZE,0,q);
			}
			glEnd();
		}
	}

	if(renderHelpers)
	{
		rr::RRReportInterval report(rr::INF3,"render helpers 2...\n");
		// render properties
		centerObject = UINT_MAX; // reset pointer to texel in the center of screen, it will be set again ~100 lines below
		centerTexel = UINT_MAX;
		centerTriangle = UINT_MAX;
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, winWidth, winHeight, 0);
		glColor3f(1,1,1);
		glRasterPos2i(winWidth/2-4,winHeight/2+1);
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15,'.');
		int x = 10;
		int y = 10;
		textOutput(x,y+=18,"controls: right mouse button, wasdqzxc, mouse, +-*/");
		unsigned numObjects = solver->getNumObjects();
		{
			if(renderRealtime)
			{
				const char* solverType = "";
				switch(solver->getInternalSolverType())
				{
					case rr::RRDynamicSolver::NONE: solverType = "no solver"; break;
					case rr::RRDynamicSolver::ARCHITECT: solverType = "Architect solver"; break;
					case rr::RRDynamicSolver::FIREBALL: solverType = "Fireball solver"; break;
					case rr::RRDynamicSolver::BOTH: solverType = "both solvers"; break;
				}
				textOutput(x,y+=18,"realtime GI lighting, %s",solverType);
			}
			else
			{
				unsigned vbuf = 0;
				unsigned lmap = 0;
				for(unsigned i=0;i<numObjects;i++)
				{
					if(solver->getIllumination(i) && solver->getIllumination(i)->getLayer(layerNumber))
					{
						if(solver->getIllumination(i)->getLayer(layerNumber)->getType()==rr::BT_VERTEX_BUFFER) vbuf++; else
						if(solver->getIllumination(i)->getLayer(layerNumber)->getType()==rr::BT_2D_TEXTURE) lmap++;
					}
				}
				textOutput(x,y+=18,"static lighting (%dx vbuf, %dx lmap, %dx none)",vbuf,lmap,numObjects-vbuf-lmap);
			}
		}
		if(!render2d || !lv)
		{
			textOutput(x,y+=18*2,"[camera]");
			textOutput(x,y+=18,"pos: %f %f %f",eye.pos[0],eye.pos[1],eye.pos[2]);
			textOutput(x,y+=18,"dir: %f %f %f",eye.dir[0],eye.dir[1],eye.dir[2]);
		}
		unsigned numLights = solver->getLights().size();
		const rr::RRObject* multiObject = solver->getMultiObjectCustom();
		rr::RRObject* singleObject = solver->getObject(selectedObjectIndex);
		const rr::RRMesh* multiMesh = multiObject ? multiObject->getCollider()->getMesh() : NULL;
		const rr::RRMesh* singleMesh = singleObject ? singleObject->getCollider()->getMesh() : NULL;
		unsigned numTrianglesMulti = multiMesh ? multiMesh->getNumTriangles() : 0;
		unsigned numTrianglesSingle = singleMesh ? singleMesh->getNumTriangles() : 0;
		if(!render2d || !lv) if(selectedLightIndex<solver->realtimeLights.size())
		{
			RealtimeLight* rtlight = solver->realtimeLights[selectedLightIndex];
			const rr::RRLight* rrlight = rtlight->origin;
			Camera* light = rtlight->getParent();
			textOutput(x,y+=18*2,"[light %d/%d]",selectedLightIndex,solver->realtimeLights.size());
			textOutput(x,y+=18,"type: %s",(rrlight->type==rr::RRLight::POINT)?"point":((rrlight->type==rr::RRLight::SPOT)?"spot":"dir"));
			textOutput(x,y+=18,"pos: %f %f %f",light->pos[0],light->pos[1],light->pos[2]);
			textOutput(x,y+=18,"dir: %f %f %f",light->dir[0],light->dir[1],light->dir[2]);
			textOutput(x,y+=18,"color: %f %f %f",rrlight->color[0],rrlight->color[1],rrlight->color[2]);
			switch(rrlight->distanceAttenuationType)			
			{
				case rr::RRLight::NONE:        textOutput(x,y+=18,"dist att: none"); break;
				case rr::RRLight::PHYSICAL:    textOutput(x,y+=18,"dist att: physically correct"); break;
				case rr::RRLight::POLYNOMIAL:  textOutput(x,y+=18,"dist att: 1/(%f+%f*d+%f*d^2)",rrlight->polynom[0],rrlight->polynom[1],rrlight->polynom[2]); break;
				case rr::RRLight::EXPONENTIAL: textOutput(x,y+=18,"dist att: max(0,1-(distance/%f)^2)^%f",rrlight->radius,rrlight->fallOffExponent); break;
			}
			if(numTrianglesMulti<100000) // skip this expensive step for big scenes
			{
				static RealtimeLight* lastLight = NULL;
				static unsigned numLightReceivers = 0;
				static unsigned numShadowCasters = 0;
				if(rtlight!=lastLight)
				{
					lastLight = rtlight;
					numLightReceivers = 0;
					numShadowCasters = 0;
					for(unsigned t=0;t<numTrianglesMulti;t++)
					{
						if(multiObject->getTriangleMaterial(t,rrlight,NULL)) numLightReceivers++;
						for(unsigned j=0;j<numObjects;j++)
						{
							if(multiObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowCasters++;
						}
					}
				}
				textOutput(x,y+=18,"triangles lit: %d/%d",numLightReceivers,numTrianglesMulti);
				textOutput(x,y+=18,"triangles casting shadow: %f/%d",numShadowCasters/float(numObjects),numTrianglesMulti);
			}
		}
		if(singleMesh && selectedObjectIndex<solver->getNumObjects())
		{
			textOutput(x,y+=18*2,"[static object %d/%d]",selectedObjectIndex,numObjects);
			textOutput(x,y+=18,"triangles: %d/%d",numTrianglesSingle,numTrianglesMulti);
			textOutput(x,y+=18,"vertices: %d/%d",singleMesh->getNumVertices(),multiMesh?multiMesh->getNumVertices():0);
			static const rr::RRObject* lastObject = NULL;
			static rr::RRVec3 bboxMinL;
			static rr::RRVec3 bboxMaxL;
			static rr::RRVec3 centerL;
			static rr::RRVec3 bboxMinW;
			static rr::RRVec3 bboxMaxW;
			static rr::RRVec3 centerW;
			static unsigned numReceivedLights = 0;
			static unsigned numShadowsCast = 0;
			if(singleObject!=lastObject)
			{
				lastObject = singleObject;
				singleObject->getCollider()->getMesh()->getAABB(&bboxMinL,&bboxMaxL,&centerL);
				rr::RRMesh* singleWorldMesh = singleObject->createWorldSpaceMesh();
				singleWorldMesh->getAABB(&bboxMinW,&bboxMaxW,&centerW);
				delete singleWorldMesh;
				numReceivedLights = 0;
				numShadowsCast = 0;
				for(unsigned i=0;i<numLights;i++)
				{
					rr::RRLight* rrlight = solver->getLights()[i];
					for(unsigned t=0;t<numTrianglesSingle;t++)
					{
						if(singleObject->getTriangleMaterial(t,rrlight,NULL)) numReceivedLights++;
						for(unsigned j=0;j<numObjects;j++)
						{
							if(singleObject->getTriangleMaterial(t,rrlight,solver->getObject(j))) numShadowsCast++;
						}
					}
				}
			}

			if(numTrianglesSingle)
			{
				textOutput(x,y+=18,"world AABB: %f %f %f .. %f %f %f",bboxMinW[0],bboxMinW[1],bboxMinW[2],bboxMaxW[0],bboxMaxW[1],bboxMaxW[2]);
				textOutput(x,y+=18,"world center: %f %f %f",centerW[0],centerW[1],centerW[2]);
				textOutput(x,y+=18,"local AABB: %f %f %f .. %f %f %f",bboxMinL[0],bboxMinL[1],bboxMinL[2],bboxMaxL[0],bboxMaxL[1],bboxMaxL[2]);
				textOutput(x,y+=18,"local center: %f %f %f",centerL[0],centerL[1],centerL[2]);
				textOutput(x,y+=18,"received lights: %f/%d",numReceivedLights/float(numTrianglesSingle),numLights);
				textOutput(x,y+=18,"shadows cast: %f/%d",numShadowsCast/float(numTrianglesSingle),numLights*numObjects);
			}
			textOutput(x,y+=18,"lit: %s",renderRealtime?"realtime":"static");
			rr::RRBuffer* bufferSelectedObj = solver->getIllumination(selectedObjectIndex) ? solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber) : NULL;
			if(bufferSelectedObj)
			{
				if(renderRealtime) glColor3f(0.5f,0.5f,0.5f);
				textOutput(x,y+=18,"[lightmap]");
				textOutput(x,y+=18,"type: %s",(bufferSelectedObj->getType()==rr::BT_VERTEX_BUFFER)?"PER VERTEX":((bufferSelectedObj->getType()==rr::BT_2D_TEXTURE)?"PER PIXEL":"INVALID!"));
				textOutput(x,y+=18,"size: %d*%d*%d",bufferSelectedObj->getWidth(),bufferSelectedObj->getHeight(),bufferSelectedObj->getDepth());
				textOutput(x,y+=18,"format: %s",(bufferSelectedObj->getFormat()==rr::BF_RGB)?"RGB":((bufferSelectedObj->getFormat()==rr::BF_RGBA)?"RGBA":((bufferSelectedObj->getFormat()==rr::BF_RGBF)?"RGBF":((bufferSelectedObj->getFormat()==rr::BF_RGBAF)?"RGBAF":"INVALID!"))));
				textOutput(x,y+=18,"scale: %s",bufferSelectedObj->getScaled()?"custom(usually sRGB)":"physical(linear)");
				glColor3f(1,1,1);
			}
		}
		if(multiMesh && (!render2d || !lv))
		{
			rr::RRRay* ray = rr::RRRay::create();
			rr::RRVec3 dir = eye.dir.RRVec3::normalized();
			ray->rayOrigin = eye.pos;
			ray->rayDirInv[0] = 1/dir[0];
			ray->rayDirInv[1] = 1/dir[1];
			ray->rayDirInv[2] = 1/dir[2];
			ray->rayLengthMin = 0;
			ray->rayLengthMax = 10000;
			ray->rayFlags = rr::RRRay::FILL_DISTANCE|rr::RRRay::FILL_PLANE|rr::RRRay::FILL_POINT2D|rr::RRRay::FILL_POINT3D|rr::RRRay::FILL_SIDE|rr::RRRay::FILL_TRIANGLE;
			if(solver->getMultiObjectCustom()->getCollider()->intersect(ray))
			{
				rr::RRMesh::PreImportNumber preTriangle = multiMesh->getPreImportTriangle(ray->hitTriangle);
				const rr::RRMaterial* material = multiObject->getTriangleMaterial(ray->hitTriangle,NULL,NULL);
				rr::RRMaterial pointMaterial;
				if(material && material->sideBits[ray->hitFrontSide?0:1].pointDetails)
				{
					multiObject->getPointMaterial(ray->hitTriangle,ray->hitPoint2d,pointMaterial);
					material = &pointMaterial;
				}
				rr::RRMesh::TriangleMapping triangleMapping;
				multiMesh->getTriangleMapping(ray->hitTriangle,triangleMapping);
				rr::RRVec2 uvInLightmap = triangleMapping.uv[0] + (triangleMapping.uv[1]-triangleMapping.uv[0])*ray->hitPoint2d[0] + (triangleMapping.uv[2]-triangleMapping.uv[0])*ray->hitPoint2d[1];
				rr::RRMesh::TriangleNormals triangleNormals;
				multiMesh->getTriangleNormals(ray->hitTriangle,triangleNormals);
				rr::RRVec3 normal = triangleNormals.vertex[0].normal + (triangleNormals.vertex[1].normal-triangleNormals.vertex[0].normal)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].normal-triangleNormals.vertex[0].normal)*ray->hitPoint2d[1];
				rr::RRVec3 tangent = triangleNormals.vertex[0].tangent + (triangleNormals.vertex[1].tangent-triangleNormals.vertex[0].tangent)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].tangent-triangleNormals.vertex[0].tangent)*ray->hitPoint2d[1];
				rr::RRVec3 bitangent = triangleNormals.vertex[0].bitangent + (triangleNormals.vertex[1].bitangent-triangleNormals.vertex[0].bitangent)*ray->hitPoint2d[0] + (triangleNormals.vertex[2].bitangent-triangleNormals.vertex[0].bitangent)*ray->hitPoint2d[1];
				textOutput(x,y+=18*2,"[point in the middle of viewport]");
				textOutput(x,y+=18,"object: %d/%d",preTriangle.object,numObjects);
				textOutput(x,y+=18,"object lit: %s",renderRealtime?"reatime GI":(solver->getIllumination(preTriangle.object)->getLayer(layerNumber)?(solver->getIllumination(preTriangle.object)->getLayer(layerNumber)->getType()==rr::BT_2D_TEXTURE?"static lightmap":"static per-vertex"):"not"));
				textOutput(x,y+=18,"triangle in object: %d/%d",preTriangle.index,solver->getObject(preTriangle.object)->getCollider()->getMesh()->getNumTriangles());
				textOutput(x,y+=18,"triangle in scene: %d/%d",ray->hitTriangle,numTrianglesMulti);
				textOutput(x,y+=18,"uv in triangle: %f %f",ray->hitPoint2d[0],ray->hitPoint2d[1]);
				textOutput(x,y+=18,"uv in lightmap: %f %f",uvInLightmap[0],uvInLightmap[1]);
				rr::RRBuffer* bufferCenter = solver->getIllumination(preTriangle.object) ? solver->getIllumination(preTriangle.object)->getLayer(layerNumber) : NULL;
				if(bufferCenter)
				{
					int i = int(uvInLightmap[0]*bufferCenter->getWidth());
					int j = int(uvInLightmap[1]*bufferCenter->getHeight());
					textOutput(x,y+=18,"ij in lightmap: %d %d",i,j);
					if(i>=0 && i<(int)bufferCenter->getWidth() && j>=0 && j<(int)bufferCenter->getHeight())
					{
						centerObject = preTriangle.object;
						centerTexel = i + j*bufferCenter->getWidth();
						centerTriangle = ray->hitTriangle;
					}
				}
				textOutput(x,y+=18,"distance: %f",ray->hitDistance);
				textOutput(x,y+=18,"pos: %f %f %f",ray->hitPoint3d[0],ray->hitPoint3d[1],ray->hitPoint3d[2]);
				textOutput(x,y+=18,"plane:  %f %f %f %f",ray->hitPlane[0],ray->hitPlane[1],ray->hitPlane[2],ray->hitPlane[3]);
				textOutput(x,y+=18,"normal: %f %f %f",normal[0],normal[1],normal[2]);
				textOutput(x,y+=18,"tangent: %f %f %f",tangent[0],tangent[1],tangent[2]);
				textOutput(x,y+=18,"bitangent: %f %f %f",bitangent[0],bitangent[1],bitangent[2]);
				textOutput(x,y+=18,"side: %s",ray->hitFrontSide?"front":"back");
				textOutput(x,y+=18,"material: %s",material?((material!=&pointMaterial)?"per-triangle":"per-vertex"):"NULL!!!");
				if(material)
				{
					if(material->name)
						textOutput(x,y+=18,"name: %s",material->name);
					textOutput(x,y+=18,"diffuse refl: %f %f %f",material->diffuseReflectance.color[0],material->diffuseReflectance.color[1],material->diffuseReflectance.color[2]);
					textOutput(x,y+=18,"specular refl: %f",material->specularReflectance);
					textOutput(x,y+=18,"transmittance: %f %f %f",material->specularTransmittance.color[0],material->specularTransmittance.color[1],material->specularTransmittance.color[2]);
					textOutput(x,y+=18,"refraction index: %f",material->refractionIndex);
					textOutput(x,y+=18,"dif.emittance: %f %f %f",material->diffuseEmittance.color[0],material->diffuseEmittance.color[1],material->diffuseEmittance.color[2]);
				}
				unsigned numReceivedLights = 0;
				unsigned numShadowsCast = 0;
				for(unsigned i=0;i<numLights;i++)
				{
					rr::RRLight* rrlight = solver->getLights()[i];
					if(multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,NULL)) numReceivedLights++;
					for(unsigned j=0;j<numObjects;j++)
					{
						if(multiObject->getTriangleMaterial(ray->hitTriangle,rrlight,solver->getObject(j))) numShadowsCast++;
					}
				}
				textOutput(x,y+=18,"received lights: %d/%d",numReceivedLights,numLights);
				textOutput(x,y+=18,"shadows cast: %d/%d",numShadowsCast,numLights*numObjects);
			}
			textOutput(x,y+=18*2,"numbers of casters/lights show potential, what is allowed");
			delete ray;
		}
		if(multiMesh && render2d && lv)
		{
			rr::RRVec2 uv = LightmapViewer::getCenterUv();
			textOutput(x,y+=18*2,"[point in the middle of viewport]");
			textOutput(x,y+=18,"uv: %f %f",uv[0],uv[1]);
			rr::RRBuffer* buffer = solver->getIllumination(selectedObjectIndex) ? solver->getIllumination(selectedObjectIndex)->getLayer(layerNumber) : NULL;
			if(buffer && buffer->getType()==rr::BT_2D_TEXTURE)
			{
				int i = int(uv[0]*buffer->getWidth());
				int j = int(uv[1]*buffer->getHeight());
				textOutput(x,y+=18,"ij: %d %d",i,j);
				if(i>=0 && i<(int)buffer->getWidth() && j>=0 && j<(int)buffer->getHeight())
				{
					centerObject = selectedObjectIndex;
					centerTexel = i + j*buffer->getWidth();
					//!!!centerTriangle = ?;
					rr::RRVec4 color = buffer->getElement(i+j*buffer->getWidth());
					textOutput(x,y+=18,"color: %f %f %f %f",color[0],color[1],color[2],color[3]);
				}
			}
		}
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glEnable(GL_DEPTH_TEST);
		if((!render2d || !lv) && ray_log::size)
		{
			glBegin(GL_LINES);
			for(unsigned i=0;i<ray_log::size;i++)
			{
				if(ray_log::log[i].unreliable)
					glColor3ub(255,0,0);
				else
				if(ray_log::log[i].infinite)
					glColor3ub(0,0,255);
				else
					glColor3ub(0,255,0);
				glVertex3fv(&ray_log::log[i].begin[0]);
				glColor3ub(0,0,0);
				//glVertex3fv(&ray_log::log[i].end[0]);
				glVertex3f(ray_log::log[i].end[0]+rand()/(100.0f*RAND_MAX),ray_log::log[i].end[1]+rand()/(100.0f*RAND_MAX),ray_log::log[i].end[2]+rand()/(100.0f*RAND_MAX));
			}
			glEnd();
		}
	}

	glutSwapBuffers();
}

static void idle()
{
	if(!winWidth) return; // can't work without window

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		CLAMP(seconds,0.001f,0.3f);
		seconds *= speedGlobal;
		Camera* cam = (selectedType!=ST_LIGHT)?&eye:solver->realtimeLights[selectedLightIndex]->getParent();
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedUp) cam->moveUp(speedUp*seconds);
		if(speedDown) cam->moveDown(speedDown*seconds);
		if(speedLean) cam->lean(speedLean*seconds);
		if(speedForward || speedBack || speedRight || speedLeft || speedUp || speedDown || speedLean)
		{
			if(cam!=&eye) 
			{
				solver->reportDirectIlluminationChange(selectedLightIndex,true,true);
				if(speedForward) cam->moveForward(speedForward*seconds);
			}
		}
	}
	prev = now;

	glutPostRedisplay();
}

static void menuStatus(int status, int x, int y)
{
	menuInUse = status;
}

void sceneViewer(rr::RRDynamicSolver* _solver, bool _createWindow, const char* _pathToShaders, int _layerNumber, bool _honourExpensiveLightingShadowingFlags)
{
	// init GLUT
	int window;
	if(_createWindow)
	{
		int argc=1;
		char argv0[2] = "a";
		char* argv[] = {argv0,NULL};
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
		unsigned w = glutGet(GLUT_SCREEN_WIDTH);
		unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
		unsigned resolutionx = w-128;
		unsigned resolutiony = h-64;
		glutInitWindowSize(resolutionx,resolutiony);
		glutInitWindowPosition((w-resolutionx)/2,(h-resolutiony)/2);
		window = glutCreateWindow("Lightsprint Debug Console");
		glutPopWindow();

		// init GLEW
		if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

		// init GL
		int major, minor;
		if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
			error("OpenGL 2.0 capable graphics card is required.\n",true);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_DEPTH_TEST);
	}
	else
	{
		int viewport[4];
		glGetIntegerv(GL_VIEWPORT,viewport);
		winWidth = viewport[2];
		winHeight = viewport[3];
		//winWidth = glutGet(GLUT_WINDOW_WIDTH);
		//winHeight = glutGet(GLUT_WINDOW_HEIGHT);
	}

	// init solver
	solver = new Solver(_pathToShaders);
	solver->honourExpensiveLightingShadowingFlags = _honourExpensiveLightingShadowingFlags;
	solver->setScaler(_solver->getScaler());
	solver->setEnvironment(_solver->getEnvironment());
	solver->setStaticObjects(_solver->getStaticObjects(),NULL,NULL,rr::RRCollider::IT_BSP_FASTER,_solver->getMultiObjectCustom());
	solver->setLights(_solver->getLights());
	solver->observer = &eye; // solver automatically updates lights that depend on camera
	solver->loadFireball(NULL); // if fireball file already exists in temp, use it
	fireballLoadAttempted = 1;

	// init rest
	lv = LightmapViewer::create(_pathToShaders);
	layerNumber = (_layerNumber<0)?-1-_layerNumber:_layerNumber;
	renderRealtime = _layerNumber<0;
	renderAmbient = _solver->getLights().size()==0;
	ourEnv = 0;
	if(selectedLightIndex>_solver->getLights().size()) selectedLightIndex = 0;
	if(selectedObjectIndex>=solver->getNumObjects()) selectedObjectIndex = 0;
	lightFieldQuadric = gluNewQuadric();
	lightFieldObjectIllumination = new rr::RRObjectIllumination(0);
	lightFieldObjectIllumination->diffuseEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,4,4,6,rr::BF_RGB,true,NULL);
	lightFieldObjectIllumination->specularEnvMap = rr::RRBuffer::create(rr::BT_CUBE_TEXTURE,16,16,6,rr::BF_RGB,true,NULL);

	// run
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);
	glutSpecialFunc(special);
	glutSpecialUpFunc(specialUp);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);
	glutMenuStatusFunc(menuStatus);
	Menu* menu = new Menu(solver);
	menu->mainCallback(Menu::ME_RANDOM_CAMERA);
	toneMapping = new ToneMapping(_pathToShaders);
	
	exitRequested = false;
#ifdef GLUT_WITH_WHEEL_AND_LOOP
	while(!exitRequested && !_solver->aborting)
		glutMainLoopUpdate();
#else
	glutMainLoop();
#endif

	delete toneMapping;
	delete menu;
//	glutDisplayFunc(NULL); forbidden by GLUT
	glutKeyboardFunc(NULL);
	glutKeyboardUpFunc(NULL);
	glutSpecialFunc(NULL);
	glutSpecialUpFunc(NULL);
	glutReshapeFunc(NULL);
	glutMouseFunc(NULL);
	glutPassiveMotionFunc(NULL);
	glutIdleFunc(NULL);
	glutMenuStatusFunc(NULL);

	if(_createWindow)
	{
		// delete all textures created by us
		deleteAllTextures();
		if(solver->getEnvironment())
			((rr::RRBuffer*)solver->getEnvironment())->customData = NULL; //!!! customData is modified in const object
		for(unsigned i=0;i<solver->getNumObjects();i++)
			if(solver->getIllumination(i)->getLayer(layerNumber))
				solver->getIllumination(i)->getLayer(layerNumber)->customData = NULL;

		glutDestroyWindow(window);
	}
	if(ourEnv)
		delete solver->getEnvironment();
	SAFE_DELETE(solver);
	SAFE_DELETE(lv);
	SAFE_DELETE(lightField);
	SAFE_DELETE(lightFieldObjectIllumination);
	for(unsigned i=0;i<lightsToBeDeletedOnExit.size();i++) delete lightsToBeDeletedOnExit[i];
	lightsToBeDeletedOnExit.clear();
	gluDeleteQuadric(lightFieldQuadric);
	lightFieldQuadric = NULL;
}

}; // namespace
