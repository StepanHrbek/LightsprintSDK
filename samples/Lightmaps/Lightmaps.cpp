// --------------------------------------------------------------------------
// Lightmaps sample
//
// This is a viewer of 3DS MAX .3DS and Collada .DAE scenes
// with realtime global illumination
// and ability to precompute/render/save/load
// higher quality texture based illumination.
//
// Unlimited options:
// - Ambient maps (indirect illumination) are precomputed here,
//   but you can tweak parameters of updateLightmaps()
//   to create any type of textures, including direct/indirect/global illumination.
// - Use setEnvironment() and capture illumination from skybox.
// - Use setLights() and capture illumination from arbitrary 
//   point/spot/dir/area/programmable lights.
// - Tweak materials and capture illumination from emissive materials.
// - Everything is HDR internally, custom scale externally.
//   Tweak setScaler() and/or RRIlluminationPixelBuffer
//   to set your scale and get HDR textures.
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//  spacebar = toggle between vertex based and ambient map based render
//  p = Precompute higher quality maps
//      alt-tab to console to see progress (takes several minutes)
//  s = Save maps to disk (alt-tab to console to see filenames)
//  l = Load maps from disk, stop realtime global illumination
//
// Remarks:
// - comment out #define COLLADA to switch from COLLADA to 3DS
// - To increase ambient map quality, you can
//    - provide better unwrap texcoords for meshes
//      (see getTriangleMapping or save unwrap into Collada document)
//    - call updateLightmaps with higher quality
//    - increase ambient map resolution (see newPixelBuffer)
// - To generate maps faster
//    - provide better unwrap texcoords for meshes
//      (see getTriangleMapping or save unwrap into Collada document)
//      and decrease map resolution (see newPixelBuffer)
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#define COLLADA // load Collada .DAE scene instead of .3DS scene
#define OPTIMIZED // render internal optimized scene instead of original scene

#ifdef COLLADA
#include "FCollada.h" // must be included before demoengine because of fcollada SAFE_DELETE macro
#include "FCDocument/FCDocument.h"
#include "../../samples/ImportCollada/RRObjectCollada.h"
#endif

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/DemoEngine/Water.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfScene.h"
#include "../../samples/Import3DS/RRObject3DS.h"
#include "../HelloRealtimeRadiosity/DynamicObject.h"

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 6xxx\n - GeForce 7xxx\n - GeForce 8xxx\n - Radeon 9500-9800\n - Radeon Xxxx\n - Radeon X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

#ifdef COLLADA
FCDocument*             collada;
#else
de::Model_3DS           m3ds;
#endif
de::Camera              eye = {{-1.416,1.741,-3.646},12.230,0,0.050,1.3,70.0,0.3,60.0};
de::Camera              light = {{-1.802,0.715,0.850},0.635,0,0.300,1.0,70.0,1.0,20.0};
de::AreaLight*          areaLight = NULL;
de::Texture*            lightDirectMap = NULL;
de::Water*              water = NULL;
de::UberProgram*        uberProgram = NULL;
rr_gl::RRDynamicSolverGL* solver = NULL;
rr_gl::RendererOfScene* rendererOfScene = NULL;
DynamicObject*          robot = NULL;
DynamicObject*          potato = NULL;
int                     winWidth = 0;
int                     winHeight = 0;
bool                    modeMovingEye = false;
float                   speedForward = 0;
float                   speedBack = 0;
float                   speedRight = 0;
float                   speedLeft = 0;
bool                    ambientMapsRender = false;

/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(de::UberProgramSetup uberProgramSetup)
{
	// render static scene
	rendererOfScene->setParams(uberProgramSetup,areaLight,lightDirectMap);
#ifdef OPTIMIZED
	rendererOfScene->useOptimizedScene();
#else
	rendererOfScene->useOriginalScene(0);
#endif
	rendererOfScene->render();

	// render dynamic objects
	// enable object space
	uberProgramSetup.OBJECT_SPACE = true;
	// when not rendering shadows, enable environment maps
	if(uberProgramSetup.LIGHT_DIRECT)
	{
		uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false; // stop using vertex illumination
		uberProgramSetup.LIGHT_INDIRECT_MAP = false; // stop using ambient map illumination
		uberProgramSetup.LIGHT_INDIRECT_ENV = true; // use indirect illumination from envmap
	}
	// move and rotate object freely, nothing is precomputed
	static float rotation = 0;
	if(!uberProgramSetup.LIGHT_DIRECT) rotation = (timeGetTime()%10000000)*0.07f;
	// render object1
	if(robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		robot->rotYZ = rr::RRVec2(rotation,0);
		robot->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			robot->updateIllumination(solver);
		robot->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye,NULL,1);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			potato->updateIllumination(solver);
		potato->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye,NULL,1);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	de::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	de::Texture* shadowmap = areaLight->getShadowMap(mapIndex);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	de::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
	renderScene(uberProgramSetup);
	shadowmap->renderingToEnd();
	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);
}


/////////////////////////////////////////////////////////////////////////////
//
// integration with Realtime Radiosity

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("../../data/shaders/")
	{
	}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		// Decide how big ambient map you want for object. 
		// In this sample, we pick res proportional to number of triangles in object.
		// When seams appear, increase res.
		// Optimal res depends on quality of unwrap provided by object->getTriangleMapping.
		// This sample has bad unwrap -> high res map is needed.
		unsigned res = 16;
		unsigned sizeFactor = 20; // decrease for good unwrap
		while(res<2048 && res<sizeFactor*sqrtf(object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		return createIlluminationPixelBuffer(res,res);
	}
	// skipped, material properties were already readen from .3ds and never change
	virtual void detectMaterials() {}
	// detects direct illumination irradiances on all faces in scene
	virtual bool detectDirectIllumination()
	{
		// shadowmap could be outdated, update it
		updateShadowmap(0);

		return RRDynamicSolverGL::detectDirectIllumination();
	}
	// set shader so that direct light+shadows+emissivity are rendered, but no materials
	virtual void setupShader(unsigned objectNumber)
	{
		// render scene with forced 2d positions of all triangles
		de::UberProgramSetup uberProgramSetup;
		uberProgramSetup.SHADOW_MAPS = 1;
		uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_MAP = true;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.FORCE_2D_POSITION = true;
		if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap,NULL,1))
			error("Failed to compile or link GLSL program.\n",true);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void special(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 1; break;
		case GLUT_KEY_DOWN: speedBack = 1; break;
		case GLUT_KEY_LEFT: speedLeft = 1; break;
		case GLUT_KEY_RIGHT: speedRight = 1; break;
	}
}

void specialUp(int c, int x, int y)
{
	switch(c) 
	{
		case GLUT_KEY_UP: speedForward = 0; break;
		case GLUT_KEY_DOWN: speedBack = 0; break;
		case GLUT_KEY_LEFT: speedLeft = 0; break;
		case GLUT_KEY_RIGHT: speedRight = 0; break;
	}
}

void keyboard(unsigned char c, int x, int y)
{
	switch (c)
	{
		case ' ':
			// toggle vertex based vs ambient map based render
			ambientMapsRender = !ambientMapsRender;
			break;

		case 'p':
			// Updates ambient maps (indirect illumination) in high quality.
			{
				rr::RRDynamicSolver::UpdateLightmapParameters paramsDirect;
				paramsDirect.applyCurrentIndirectSolution = true; // enable final gather of current indirect solution
				paramsDirect.quality = 1000;
				paramsDirect.applyEnvironment = true; // enable direct illumination from skybox (note: no effect in room without windows)
				paramsDirect.applyLights = true; // enable direct illumination from lights set by setLights() (note: no effect because no lights were set)

				// calculate all lightmaps at once
				//solver->updateLightmaps(0,true,&paramsDirect,NULL);

				// calculate lightmap only for first object
				if(!solver->getIllumination(0)->getLayer(0)->pixelBuffer)
					solver->getIllumination(0)->getLayer(0)->pixelBuffer = solver->createIlluminationPixelBuffer(512,512);
				solver->updateLightmap(0,solver->getIllumination(0)->getLayer(0)->pixelBuffer,&paramsDirect);

				// start rendering computed maps
				ambientMapsRender = true;
				modeMovingEye = true;
				break;
			}

		case 's':
			// save current indirect illumination (static snapshot) to disk
			{
				static unsigned captureIndex = 0;
				char filename[100];
				// save all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
				{
					rr::RRIlluminationPixelBuffer* map = solver->getIllumination(objectIndex)->getLayer(0)->pixelBuffer;
					if(map)
					{
						sprintf(filename,"../../data/export/cap%02d_statobj%d.png",captureIndex,objectIndex);
						bool saved = map->save(filename);
						printf(saved?"Saved %s.\n":"Error: Failed to save %s.\n",filename);
					}
				}
				captureIndex++;
				break;
			}

		case 'l':
			// load static snapshot of indirect illumination from disk, stop realtime updates
			{
				unsigned captureIndex = 0;
				char filename[100];
				// load all ambient maps (static objects)
				for(unsigned objectIndex=0;objectIndex<solver->getNumObjects();objectIndex++)
				{
					sprintf(filename,"../../data/export/cap%02d_statobj%d.png",captureIndex,objectIndex);
					rr::RRObjectIllumination::Layer* illum = solver->getIllumination(objectIndex)->getLayer(0);
					rr::RRIlluminationPixelBuffer* loaded = solver->loadIlluminationPixelBuffer(filename);
					printf(loaded?"Loaded %s.\n":"Error: Failed to load %s.\n",filename);
					if(loaded)
					{
						delete illum->pixelBuffer;
						illum->pixelBuffer = loaded;
					}
				}
				// start rendering loaded maps
				ambientMapsRender = true;
				modeMovingEye = true;
				break;
			}

		case 27:
			exit(0);
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = (double) winWidth / (double) winHeight;
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getTexelBits();
	glPolygonOffset(4, 42 << (shadowDepthBits-16) );
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
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
			eye.angle -= 0.005*x;
			eye.angleX -= 0.005*y;
			CLAMP(eye.angleX,-M_PI*0.49f,M_PI*0.49f);
		}
		else
		{
			light.angle -= 0.005*x;
			light.angleX -= 0.005*y;
			CLAMP(light.angleX,-M_PI*0.49f,M_PI*0.49f);
			solver->reportDirectIlluminationChange(true);
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	// update shadowmaps
	eye.update(0);
	light.update(0.3f);
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++) updateShadowmap(i);

#ifndef OPTIMIZED
	// update vertex color buffers if they need it
	if(!ambientMapsRender)
	{
		static unsigned solutionVersion = 0;
		if(solver->getSolutionVersion()!=solutionVersion)
		{
			solutionVersion = solver->getSolutionVersion();
			solver->updateVertexBuffers(0,true,RM_IRRADIANCE_PHYSICAL_INDIRECT);
		}
	}
#endif

	// update ambient maps if they don't exist yet
	if(ambientMapsRender && !solver->getIllumination(0)->getLayer(0)->pixelBuffer)
	{
		// precompute preview maps, takes few ms
		//solver->updateLightmaps(0,true,NULL,NULL);
		// precompute high quality maps, takes minutes
		keyboard('p',0,0);
	}

	// update water reflection
	water->updateReflectionInit(winWidth/4,winHeight/4,&eye,-0.3f);
	glClear(GL_DEPTH_BUFFER_BIT);
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = !ambientMapsRender;
	uberProgramSetup.LIGHT_INDIRECT_MAP = ambientMapsRender;
	uberProgramSetup.LIGHT_INDIRECT_auto = ambientMapsRender; // when map doesn't exist, render vcolors
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	renderScene(uberProgramSetup);
	water->updateReflectionDone();

	// render everything except water
	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	renderScene(uberProgramSetup);

	// render water
	water->render(100);

	glutSwapBuffers();
}

void idle()
{
	if(!winWidth) return; // can't work without window

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if(prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		CLAMP(seconds,0.001f,0.3f);
		de::Camera* cam = modeMovingEye?&eye:&light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam==&light) solver->reportDirectIlluminationChange(true);
		}
	}
	prev = now;

	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate();

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Lightmaps"); // for windowed mode
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
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
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(0.9999); // prevents backprojection

	// init shaders
	uberProgram = new de::UberProgram("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");
	water = new de::Water("..\\..\\data\\shaders\\",false,false);
	// for correct soft shadows: maximal number of shadowmaps renderable in one pass is detected
	// for usual soft shadows, simply set shadowmapsPerPass=1
	unsigned shadowmapsPerPass = 1;
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram);
	if(!shadowmapsPerPass) error("",true);
	
	// init textures
	lightDirectMap = de::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.png not found.\n",false);
	areaLight = new de::AreaLight(&light,shadowmapsPerPass,512);

	// init dynamic objects
	de::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("..\\..\\data\\objects\\I_Robot_female.3ds",0.3f,material,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("..\\..\\data\\objects\\potato\\potato01.3ds",0.004f,material,16);

	// init static scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
#ifdef COLLADA
	collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("..\\..\\data\\scenes\\koupelna\\koupelna4.dae");
	//collada->LoadFromFile("..\\..\\data\\scenes\\sponza\\sponza.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setObjects(*adaptObjectsFromFCollada(collada),NULL);
#else
	if(!m3ds.Load("..\\..\\data\\scenes\\koupelna\\koupelna4.3ds",0.03f))
	//if(!m3ds.Load("..\\..\\data\\scenes\\sponza\\sponza.3ds",1))
		error("",false);
	solver->setObjects(*adaptObjectsFrom3DS(&m3ds),NULL);
#endif
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(solver->loadIlluminationEnvironmentMap("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true));
	rendererOfScene = new rr_gl::RendererOfScene(solver);
	solver->calculate();
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	glutMainLoop();
	return 0;
}
