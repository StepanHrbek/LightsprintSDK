// --------------------------------------------------------------------------
// Lightmaps sample
//
// This is a viewer of Collada .DAE scenes
// with realtime global illumination from 1 area light
// and ability to precompute/render/save/load
// higher quality texture or vertex based illumination
// from
// - skybox
// - arbitrary point/spot/dir lights
// - 1 realtime area light
//
// Unlimited options:
// - Ambient maps (indirect illumination) are precomputed here,
//   but you can tweak parameters of updateLightmaps()
//   to create any type of result, including direct/indirect/global illumination.
// - Use setEnvironment() and capture illumination from skybox.
// - Use setLights() and capture illumination from arbitrary 
//   point/spot/dir/programmable lights.
// - Tweak materials and capture illumination from emissive materials.
// - Everything is HDR internally, custom scale externally.
//   Tweak setScaler() and/or RRIlluminationPixelBuffer
//   to set your scale and get HDR textures.
// - Tweak map resolution: search for newPixelBuffer.
// - Tweak map quality: search for quality =
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//  spacebar = toggle realtime vertex ambient and static ambient maps
//  p = Precompute higher quality static maps
//      alt-tab to console to see progress (takes seconds to minutes)
//  s = Save maps to disk (alt-tab to console to see filenames)
//  l = Load maps from disk, stop realtime global illumination
//  v = toggle precomputed per-vertex and per-pixel
//  +-= change brightness
//  */= change contrast
//
// Remarks:
// - map quality depends on unwrap quality,
//   make sure you have good unwrap in your scenes
//   (save it as second TEXCOORD in Collada document, see RRObjectCollada.cpp)
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include "FCollada.h" // must be included before demoengine because of fcollada SAFE_DELETE macro
#include "FCDocument/FCDocument.h"
#include "../../samples/ImportCollada/RRObjectCollada.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "../RealtimeRadiosity/DynamicObject.h"
#include <windows.h> // timeGetTime

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

rr_gl::Camera              eye(-1.416f,1.741f,-3.646f, 12.230f,0,0.05f,1.3f,70,0.1f,100);
rr_gl::Camera*             light;
rr_gl::UberProgram*        uberProgram = NULL;
rr_gl::RRDynamicSolverGL*  solver = NULL;
rr_gl::RendererOfScene*    rendererOfScene = NULL;
DynamicObject*             robot = NULL;
DynamicObject*             potato = NULL;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = false;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
bool                       realtimeIllumination = true;
bool                       ambientMapsRender = false;
rr::RRVec4                 brightness(1);
float                      gamma = 1;

/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
{
	// render static scene
	rendererOfScene->setParams(uberProgramSetup,&solver->realtimeLights,renderingFromThisLight,false);
	rendererOfScene->useOriginalScene(realtimeIllumination?0:1);
	rendererOfScene->setBrightnessGamma(&brightness,gamma);
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
			solver->updateEnvironmentMap(robot->illumination);
		robot->render(uberProgram,uberProgramSetup,&solver->realtimeLights,0,eye,&brightness,gamma);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			solver->updateEnvironmentMap(potato->illumination);
		potato->render(uberProgram,uberProgramSetup,&solver->realtimeLights,0,eye,&brightness,gamma);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// integration with Realtime Radiosity

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("../../data/shaders/")
	{
		setDirectIlluminationBoost(2);
	}
protected:
	virtual rr::RRIlluminationPixelBuffer* newPixelBuffer(rr::RRObject* object)
	{
		// Decide how big ambient map you want for object. 
		// In this sample, we pick res proportional to number of triangles in object.
		// When seams appear, increase res.
		// Optimal res depends on quality of unwrap provided by object->getTriangleMapping.
		unsigned res = 16;
		unsigned sizeFactor = 5; // 5 is ok for scenes with unwrap (20 is ok for scenes without unwrap)
		while(res<2048 && (float)res<sizeFactor*sqrtf((float)(object->getCollider()->getMesh()->getNumTriangles()))) res*=2;
		return createIlluminationPixelBuffer(res,res);
	}
	// called from RRDynamicSolverGL to update shadowmaps
	virtual void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
	{
		::renderScene(uberProgramSetup,renderingFromThisLight);
	}
	// detects direct illumination irradiances on all faces in scene
	virtual unsigned* detectDirectIllumination()
	{
		// don't try to detect when window is not created yet
		if(!winWidth) return NULL;
		return RRDynamicSolverGL::detectDirectIllumination();
	}
	// set shader so that direct light+shadows+emissivity are rendered, but no materials
	virtual void setupShader(unsigned objectNumber)
	{
		// render scene with forced 2d positions of all triangles
		rr_gl::UberProgramSetup uberProgramSetup;
		uberProgramSetup.SHADOW_MAPS = 1;
		uberProgramSetup.SHADOW_SAMPLES = 1;
		uberProgramSetup.LIGHT_DIRECT = true;
		uberProgramSetup.LIGHT_DIRECT_MAP = true;
		uberProgramSetup.MATERIAL_DIFFUSE = true;
		uberProgramSetup.FORCE_2D_POSITION = true;
		if(!uberProgramSetup.useProgram(uberProgram,realtimeLights[0],0,NULL,1))
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
		case '+':
			brightness *= 1.2f;
			break;
		case '-':
			brightness /= 1.2f;
			break;
		case '*':
			gamma *= 1.2f;
			break;
		case '/':
			gamma /= 1.2f;
			break;

		case ' ':
			// toggle realtime vertex vs precomputed map ambient
			realtimeIllumination = !realtimeIllumination;
			ambientMapsRender = !realtimeIllumination;
			break;
		case 'v':
			// toggle precomputed vertex vs precomputed map ambient
			ambientMapsRender = !ambientMapsRender;
			realtimeIllumination = false;
			break;
		

		case 'p':
			// Updates ambient maps (indirect illumination) in high quality.
			{
				rr::RRDynamicSolver::UpdateParameters paramsDirect;
				paramsDirect.quality = 1000;
				paramsDirect.applyCurrentSolution = false;
				rr::RRDynamicSolver::UpdateParameters paramsIndirect;
				paramsIndirect.applyCurrentSolution = false;

				// 1. type of lighting
				//  a) improve current GI lighting from realtime light
				paramsDirect.applyCurrentSolution = true;
				//  b) compute GI from point/spot/dir lights
				//paramsDirect.applyLights = true;
				//paramsIndirect.applyLights = true;
				// lights from scene file are already set, but you may set your own:
				//rr::RRLights lights;
				//lights.push_back(rr::RRLight::createPointLight(rr::RRVec3(1,1,1),rr::RRColorRGBF(0.5f)));
				//solver->setLights(lights);
				//  c) compute GI from skybox (note: no effect in closed room)
				//paramsDirect.applyEnvironment = true;
				//paramsIndirect.applyEnvironment = true;

				// 2. objects
				//  a) calculate whole scene at once
				paramsDirect.measure = RM_IRRADIANCE_CUSTOM; // get maps in custom scale (sRGB)
				solver->updateLightmaps(1,-1,true,&paramsDirect,&paramsIndirect);
				//  b) calculate only one object
				//static unsigned obj=0;
				//if(!solver->getIllumination(obj)->getLayer(0)->pixelBuffer)
				//	solver->getIllumination(obj)->getLayer(0)->pixelBuffer = solver->createIlluminationPixelBuffer(256,256);
				//solver->updateLightmap(obj,solver->getIllumination(obj)->getLayer(0)->pixelBuffer,&paramsDirect);
				//++obj%=solver->getNumObjects();

				// update vertex buffers too, for comparison with pixel buffers
				paramsDirect.measure = RM_IRRADIANCE_PHYSICAL; // get vertex colors in physical scale (HDR)
				solver->updateVertexBuffers(1,-1,true,&paramsDirect,&paramsIndirect);

				// start rendering computed maps
				ambientMapsRender = true;
				realtimeIllumination = false;
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
					rr::RRIlluminationPixelBuffer* map = solver->getIllumination(objectIndex)->getLayer(1)->pixelBuffer;
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
					rr::RRObjectIllumination::Layer* illum = solver->getIllumination(objectIndex)->getLayer(1);
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
				realtimeIllumination = false;
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
	eye.aspect = winWidth/(float)winHeight;
	GLint shadowDepthBits = solver->realtimeLights[0]->getShadowMap(0)->getTexelBits();
	glPolygonOffset(4,(float)(42<<(shadowDepthBits-16)));
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView<130) eye.fieldOfView+=10;
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
			eye.angle -= 0.005f*x;
			eye.angleX -= 0.005f*y;
			CLAMP(eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
		}
		else
		{
			light->angle -= 0.005f*x;
			light->angleX -= 0.005f*y;
			CLAMP(light->angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
			solver->reportDirectIlluminationChange(true);
			// changes also position a bit, together with rotation
			light->pos += light->dir*0.3f;
			light->update();
			light->pos -= light->dir*0.3f;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	// update shadowmaps
	eye.update();
	light->update();
	for(unsigned i=0;i<solver->realtimeLights.size();i++)
		solver->realtimeLights[i]->dirty = true;
	solver->reportDirectIlluminationChange(false);

	// update vertex color buffers if they need it
	if(realtimeIllumination)
	{
		static unsigned solutionVersion = 0;
		if(solver->getSolutionVersion()!=solutionVersion)
		{
			solutionVersion = solver->getSolutionVersion();
			solver->updateVertexBuffers(0,-1,true,NULL,NULL);
		}
	}

	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = !ambientMapsRender;
	uberProgramSetup.LIGHT_INDIRECT_MAP = ambientMapsRender;
	uberProgramSetup.LIGHT_INDIRECT_auto = ambientMapsRender; // when map doesn't exist, render vcolors
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	renderScene(uberProgramSetup,NULL);

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
		rr_gl::Camera* cam = modeMovingEye?&eye:light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam==light) solver->reportDirectIlluminationChange(true);
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
	
	// init shaders
	uberProgram = rr_gl::UberProgram::create("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("..\\..\\data\\objects\\I_Robot_female.3ds",0.3f,material,16,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("..\\..\\data\\objects\\potato\\potato01.3ds",0.004f,material,16,16);

	// init static scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("..\\..\\data\\scenes\\koupelna\\koupelna4.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	solver->setStaticObjects(*adaptObjectsFromFCollada(collada),NULL);
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(solver->loadIlluminationEnvironmentMap("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true));
	rendererOfScene = new rr_gl::RendererOfScene(solver,"../../data/shaders/");
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init light
	rr::RRLights lights;
	lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),40*3.14159f/180,0.1f));
	solver->setLights(lights);
	solver->realtimeLights[0]->lightDirectMap = rr_gl::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	light = solver->realtimeLights[0]->getParent();

	glutMainLoop();
	return 0;
}
