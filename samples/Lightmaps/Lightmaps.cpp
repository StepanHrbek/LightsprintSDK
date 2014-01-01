// --------------------------------------------------------------------------
// Lightmaps sample
//
// Shows differences between realtime and precomputed global illumination.
// At the beginning, GI is fully realtime computed (using architect solver).
// You can freely move light. After pressing 'p', sample switches to fully 
// precomputed GI (using lightmaps and lightfield).
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
//   Tweak setScaler() and/or RRBuffer
//   to set your scale and get HDR textures.
// - Tweak map resolution: search for rr::RRBuffer::create.
// - Tweak map quality: search for quality =
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//  p = Precompute higher quality static maps + lightfield
//      alt-tab to console to see progress (takes seconds to minutes)
//  1/2/3 = toggle realtime / precomputed per-vertex / precomputed per-pixel illumination
//  s = Save maps to disk (alt-tab to console to see filenames)
//  l = Load maps from disk, stop realtime global illumination
//  +-= change brightness
//  */= change contrast
//  r = randomize camera
//
// Remarks:
// - map quality depends on unwrap quality.
//   If your scene doesn't contain unwrap, you can build it with RRObjects::buildUnwrap().
//
// Copyright (C) 2006-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <GL/glew.h>
#ifdef __APPLE__
	#include <GLUT/glut.h>
	#include <ApplicationServices/ApplicationServices.h>
#else
	#include <GL/glut.h>
#endif
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PluginSky.h"
#include "Lightsprint/IO/IO.h"
#include <stdio.h>   // printf


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported GPUs:\n - those that support OpenGL 2.0 or OpenGL ES 2.0 or higher\n - i.e. nearly all from ATI/AMD since 2002, NVIDIA since 2003, Intel since 2006");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

enum Layer// arbitrary layer numbers
{
	LAYER_REALTIME = 0,
	LAYER_OFFLINE_VERTEX,
	LAYER_OFFLINE_PIXEL,
	LAYER_ENVIRONMENT,
};
Layer                      renderLayer = LAYER_REALTIME;
rr::RRCamera               eye(rr::RRVec3(-1.416f,1.741f,-3.646f), rr::RRVec3(9.09f,0.05f,0), 1.3f,70,0.1f,100);
rr::RRCamera*              light;
rr_gl::UberProgram*        uberProgram = NULL;
rr_gl::RRDynamicSolverGL*  solver = NULL;
rr::RRLightField*          lightField = NULL;
rr::RRObject*              robot = NULL;
rr::RRObject*              potato = NULL;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = false;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
rr::RRVec4                 brightness(2);
float                      contrast = 1;

// estimates where character foot is, moves it to given world coordinate, then rotates character around Y and Z axes
static void transformObject(rr::RRObject* object, rr::RRVec3 worldFoot, rr::RRVec2 rotYZ)
{
	if (!object)
		return;
	rr::RRVec3 mini,center;
	object->getCollider()->getMesh()->getAABB(&mini,NULL,&center);
	float sz = sin(RR_DEG2RAD(rotYZ[1]));
	float cz = cos(RR_DEG2RAD(rotYZ[1]));
	float sy = sin(RR_DEG2RAD(rotYZ[0]));
	float cy = cos(RR_DEG2RAD(rotYZ[0]));
	float mx = -center.x;
	float my = -mini.y;
	float mz = -center.z;
	rr::RRMatrix3x4 worldMatrix;
	worldMatrix.m[0][0] = cz*cy;
	worldMatrix.m[1][0] = sz*cy;
	worldMatrix.m[2][0] = -sy;
	worldMatrix.m[0][1] = -sz;
	worldMatrix.m[1][1] = cz;
	worldMatrix.m[2][1] = 0;
	worldMatrix.m[0][2] = cz*sy;
	worldMatrix.m[1][2] = sz*sy;
	worldMatrix.m[2][2] = cy;
	worldMatrix.m[0][3] = cz*cy*mx-sz*my+cz*sy*mz+worldFoot[0];
	worldMatrix.m[1][3] = sz*cy*mx+cz*my+sz*sy*mz+worldFoot[1];
	worldMatrix.m[2][3] = -sy*mx+cy*mz+worldFoot[2];
	object->setWorldMatrix(&worldMatrix);
	object->illumination.envMapWorldCenter = rr::RRVec3(
		center.x*worldMatrix.m[0][0]+center.y*worldMatrix.m[0][1]+center.z*worldMatrix.m[0][2]+worldMatrix.m[0][3],
		center.x*worldMatrix.m[1][0]+center.y*worldMatrix.m[1][1]+center.z*worldMatrix.m[1][2]+worldMatrix.m[1][3],
		center.x*worldMatrix.m[2][0]+center.y*worldMatrix.m[2][1]+center.z*worldMatrix.m[2][2]+worldMatrix.m[2][3]);
}

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
			contrast *= 1.2f;
			break;
		case '/':
			contrast /= 1.2f;
			break;

		case '1': renderLayer = LAYER_REALTIME; break;
		case '2': renderLayer = LAYER_OFFLINE_VERTEX; break;
		case '3': renderLayer = LAYER_OFFLINE_PIXEL; break;
		
		case 'p':
			// Updates ambient maps (indirect illumination) in high quality.
			{
				rr::RRDynamicSolver::UpdateParameters paramsDirect;
				paramsDirect.quality = 1000;
				paramsDirect.applyCurrentSolution = false;
				paramsDirect.aoIntensity = 1;
				paramsDirect.aoSize = 1;
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
				//lights.push_back(rr::RRLight::createPointLight(rr::RRVec3(1,1,1),rr::RRVec3(0.5f)));
				//solver->setLights(lights);
				//  c) compute GI from skybox (note: no effect in closed room)
				//paramsDirect.applyEnvironment = true;
				//paramsIndirect.applyEnvironment = true;

				// 2. objects
				//  a) calculate whole scene at once
				solver->updateLightmaps(LAYER_OFFLINE_PIXEL,-1,-1,&paramsDirect,&paramsIndirect,NULL);
				//  b) calculate only one object
				//static unsigned obj=0;
				//solver->updateLightmap(obj,solver->getStaticObjects()[obj]->illumination->getLayer(LAYER_OFFLINE_PIXEL),NULL,NULL,&paramsDirect);
				//++obj%=solver->getStaticObjects().size();

				// update vertex buffers too, for comparison with pixel buffers
				solver->updateLightmaps(LAYER_OFFLINE_VERTEX,-1,-1,&paramsDirect,&paramsIndirect,NULL);

				// update lightfield
				rr::RRVec4 aabbMin,aabbMax;
				solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);
				aabbMin.w = aabbMax.w = 0;
				if (!lightField) lightField = rr::RRLightField::create(aabbMin,aabbMax-aabbMin,1);
				if (lightField)
					lightField->captureLighting(solver,0);

				// start rendering computed maps
				renderLayer = LAYER_OFFLINE_PIXEL;
				modeMovingEye = true;
				break;
			}

		case 's':
			// save current indirect illumination (static snapshot) to disk
			solver->getStaticObjects().saveLayer(LAYER_OFFLINE_PIXEL,"../../data/scenes/koupelna/koupelna4_precalculated/","png");
			solver->getStaticObjects().saveLayer(LAYER_OFFLINE_VERTEX,"../../data/scenes/koupelna/koupelna4_precalculated/","rrbuffer");
			if (lightField)
				lightField->save("../../data/scenes/koupelna/koupelna4_precalculated/lightfield.lf");
			break;

		case 'l':
			// load static snapshot of indirect illumination from disk, stop realtime updates
			{
				solver->getStaticObjects().loadLayer(LAYER_OFFLINE_PIXEL,"../../data/scenes/koupelna/koupelna4_precalculated/","png");
				solver->getStaticObjects().loadLayer(LAYER_OFFLINE_VERTEX,"../../data/scenes/koupelna/koupelna4_precalculated/","rrbuffer");
				delete lightField;
				lightField = rr::RRLightField::load("../../data/scenes/koupelna/koupelna4_precalculated/lightfield.lf");
				// start rendering loaded maps
				renderLayer = LAYER_OFFLINE_PIXEL;
				modeMovingEye = true;
				break;
			}

		case 'r':
			eye.setView(rr::RRCamera::RANDOM,solver);
			break;

		case 27:
			// immediate exit without freeing memory, leaks may be reported
			// see e.g. RealtimeLights sample for freeing memory before exit
			exit(0);
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.setAspect( winWidth/(float)winHeight );
}

void mouse(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
#ifdef GLUT_WHEEL_UP
	float fov = eye.getFieldOfViewVerticalDeg();
	if (button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if (fov>13) fov -= 10; else fov /= 1.4f;
	}
	if (button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if (fov*1.4f<=3) fov *= 1.4f; else if (fov<130) fov += 10;
	}
	eye.setFieldOfViewVerticalDeg(fov);
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
		rr::RRCamera& cam = modeMovingEye ? eye : *light;
		rr::RRVec3 yawPitchRollRad = cam.getYawPitchRollRad()-rr::RRVec3(x,y,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		cam.setYawPitchRollRad(yawPitchRollRad);
		if (!modeMovingEye)
			solver->reportDirectIlluminationChange(0,true,true,false);
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// move dynamic objects
	float rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*50.f;
	transformObject(robot,rr::RRVec3(-1.83f,0,-3),rr::RRVec2(rotation,0));
	transformObject(potato,rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f),rr::RRVec2(rotation/2,0));

	// update dynamic objects from precomputed lightfield
	if (renderLayer!=LAYER_REALTIME && lightField)
	{
		lightField->updateEnvironmentMap(&robot->illumination,LAYER_ENVIRONMENT,0);
		lightField->updateEnvironmentMap(&potato->illumination,LAYER_ENVIRONMENT,0);
	}

	solver->reportDirectIlluminationChange(0,true,false,false); // scene is animated -> direct illum changes
	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	// configure plugins
	rr_gl::PluginParamsSky ppSky(NULL,solver);
	rr_gl::PluginParamsScene ppScene(&ppSky,solver);
	ppScene.solver = solver;
	ppScene.lights = &solver->realtimeLights;
	ppScene.uberProgramSetup.enableAllLights();
	ppScene.uberProgramSetup.enableAllMaterials();
	ppScene.uberProgramSetup.POSTPROCESS_BRIGHTNESS = true; // enable brightness/gamma adjustment
	ppScene.uberProgramSetup.POSTPROCESS_GAMMA = true;
	ppScene.updateLayers = renderLayer==LAYER_REALTIME;
	ppScene.layerLightmap = renderLayer;
	ppScene.layerEnvironment = LAYER_ENVIRONMENT;
	rr_gl::PluginParamsShared ppShared;
	ppShared.camera = &eye;
	ppShared.brightness = brightness;
	ppShared.gamma = contrast;
	// render scene
	solver->getRenderer()->render(&ppScene,ppShared);
	solver->renderLights(eye);

	glutSwapBuffers();
}

void idle()
{
	if (!winWidth) return; // can't work without window

	// smooth keyboard movement
	static rr::RRTime time;
	float seconds = time.secondsSinceLastQuery();
	RR_CLAMP(seconds,0.001f,0.3f);
	rr::RRCamera* cam = modeMovingEye?&eye:light;
	if (speedForward || speedBack || speedRight || speedLeft)
	{
		cam->setPosition(cam->getPosition()
			+ cam->getDirection() * ((speedForward-speedBack)*seconds)
			+ cam->getRight() * ((speedRight-speedLeft)*seconds)
			);
		if (cam==light) solver->reportDirectIlluminationChange(0,true,true,false);
	}

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char** argv)
{
	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::createPrintfReporter();

	rr_io::registerLoaders(argc,argv);

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA | GLUT_DEPTH);
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
#ifdef __APPLE__
//	OSX kills events ruthlessly
//	see http://stackoverflow.com/questions/728049/glutpassivemotionfunc-and-glutwarpmousepointer
	CGSetLocalEventsSuppressionInterval(0.0);
#endif

	// init GL
	const char* err = rr_gl::initializeGL();
	if (err)
		error(err,true);
	
	// init shaders
	uberProgram = rr_gl::UberProgram::create("../../data/shaders/ubershader.vs", "../../data/shaders/ubershader.fs");

	// init solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new rr_gl::RRDynamicSolverGL("../../data/shaders/","../../data/maps/");
	solver->setDirectIlluminationBoost(2);
	solver->setScaler(rr::RRScaler::createRgbScaler());

	// init static scene
	rr::RRScene scene("../../data/scenes/koupelna/koupelna4.dae");
	solver->setStaticObjects(scene.objects, NULL);

	// init dynamic objects
	rr::RRScene robotScene("../../data/objects/I_Robot_female.3ds");
	robotScene.normalizeUnits(0.3f);
	rr::RRScene potatoScene("../../data/objects/potato/potato01.3ds");
	potatoScene.normalizeUnits(0.004f);
	bool aborting = false;
	robot = rr::RRObject::createMultiObject(&robotScene.objects,rr::RRCollider::IT_LINEAR,aborting,-1,0,true,0,NULL);
	potato = rr::RRObject::createMultiObject(&potatoScene.objects,rr::RRCollider::IT_LINEAR,aborting,-1,0,true,0,NULL);
	robot->isDynamic = true;
	potato->isDynamic = true;
	rr::RRObjects dynamicObjects;
	dynamicObjects.push_back(robot);
	dynamicObjects.push_back(potato);
	solver->setDynamicObjects(dynamicObjects);

	// edit some material
	if (robot)
	{
		robot->faceGroups[0].material->diffuseReflectance.color = rr::RRVec3(0.2f,0,0);
		robot->faceGroups[0].material->specularReflectance.color = rr::RRVec3(0.8f);
	}

	// init environment
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
	{
		unsigned numVertices = solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices();
		// offline per-pixel
		unsigned res = 16;
		float sizeFactor = 5;
		while (res<2048 && (float)res<sizeFactor*sqrtf((float)(solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumTriangles()))) res*=2;
		solver->getStaticObjects()[i]->illumination.getLayer(LAYER_OFFLINE_PIXEL) = rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
	}
	// create remaining cubemaps and multiObject vertex buffers
	solver->allocateBuffersForRealtimeGI(LAYER_REALTIME,LAYER_ENVIRONMENT);
	solver->allocateBuffersForRealtimeGI(LAYER_OFFLINE_VERTEX,-1);

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLight(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f);
	rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/maps/spot0.png");
	rr::RRLights rrlights;
	rrlights.push_back(rrlight);
	solver->setLights(rrlights);
	light = solver->realtimeLights[0]->getCamera();

	rr::RRReporter::report(rr::INF2,
		"\n"
		"Shows differences between realtime and precomputed global illumination.\n"
		"For complete description, see comment at the beginning of sample source code.\n"
		"\n"
		"Controls:\n"
		"  mouse = look around\n"
		"  arrows = move around\n"
		"  left button = switch between camera and light\n"
		"  p = Precompute higher quality static maps + lightfield\n"
		"  1/2/3 = toggle realtime / precomputed per-vertex / precomputed per-pixel GI\n"
		"  s = Save maps to disk (alt-tab to console to see filenames)\n"
		"  l = Load maps from disk, stop realtime global illumination\n"
		"  +-= change brightness\n"
		"  */= change contrast\n"
		"  r = randomize camera\n"
		"\n"
		"Realtime GI active, you can press 'p' to precalculate GI, then 1/2/3 to compare modes.\n"
		"\n");

	glutMainLoop();
	return 0;
}
