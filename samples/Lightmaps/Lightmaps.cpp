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
//  spacebar = toggle realtime vertex ambient and static ambient maps
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
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/IO/ImportScene.h"
#include <stdio.h>   // printf


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5200-9800, 100-580, ION\n - Radeon 9500-9800, X300-1950, HD2300-6990\n - subset of FireGL and Quadro families");
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
	LAYER_OFFLINE_PIXEL
};
Layer                      renderLayer = LAYER_REALTIME;
rr_gl::Camera              eye(-1.416f,1.741f,-3.646f, 12.230f,0,0.05f,1.3f,70,0.1f,100);
rr_gl::Camera*             light;
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
					lightField->captureLighting(solver,0,false);

				// start rendering computed maps
				renderLayer = LAYER_OFFLINE_PIXEL;
				modeMovingEye = true;
				break;
			}

		case 's':
			// save current indirect illumination (static snapshot) to disk
			solver->getStaticObjects().saveLayer(LAYER_OFFLINE_PIXEL,"../../data/export/","png");
			if (lightField)
				lightField->save("../../data/export/lightfield.lf");
			break;

		case 'l':
			// load static snapshot of indirect illumination from disk, stop realtime updates
			{
				solver->getStaticObjects().loadLayer(LAYER_OFFLINE_PIXEL,"../../data/export/","png");
				delete lightField;
				lightField = rr::RRLightField::load("../../data/export/lightfield.lf");
				// start rendering loaded maps
				renderLayer = LAYER_OFFLINE_PIXEL;
				modeMovingEye = true;
				break;
			}

		case 'r':
			eye.setView(rr_gl::Camera::FRONT,solver->getMultiObjectCustom());
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
		if (modeMovingEye)
		{
			eye.angle -= mouseSensitivity*x;
			eye.angleX -= mouseSensitivity*y;
			RR_CLAMP(eye.angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		}
		else
		{
			light->angle -= mouseSensitivity*x;
			light->angleX -= mouseSensitivity*y;
			RR_CLAMP(light->angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			solver->reportDirectIlluminationChange(0,true,true);
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
	if (!winWidth || !winHeight) return; // can't display without window

	// move dynamic objects
	float rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*50.f;
	transformObject(robot,rr::RRVec3(-1.83f,0,-3),rr::RRVec2(rotation,0));
	transformObject(potato,rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f),rr::RRVec2(rotation/2,0));

	// update dynamic objects from precomputed lightfield
	if (renderLayer!=LAYER_REALTIME && lightField)
	{
		lightField->updateEnvironmentMap(&robot->illumination,0);
		lightField->updateEnvironmentMap(&potato->illumination,0);
	}

	eye.update();

	solver->reportDirectIlluminationChange(0,true,false); // scene is animated -> direct illum changes
	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	eye.setupForRender();
	// configure renderer
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.enableAllMaterials();
	uberProgramSetup.SHADOW_MAPS = 1; // enable shadows
	uberProgramSetup.LIGHT_DIRECT = true; // enable direct illumination
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_auto = true; // enable indirect illumination
	uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true;
	uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true; // enable brightness/gamma adjustment
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	// render scene
	solver->renderScene(
		uberProgramSetup,
		NULL,
		renderLayer==LAYER_REALTIME,
		renderLayer,
		-1,
		0,
		false,
		&brightness,
		contrast);

	solver->renderLights();

	glutSwapBuffers();
}

void idle()
{
	if (!winWidth) return; // can't work without window

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if (prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		RR_CLAMP(seconds,0.001f,0.3f);
		rr_gl::Camera* cam = modeMovingEye?&eye:light;
		if (speedForward) cam->pos += cam->dir * (speedForward*seconds);
		if (speedBack) cam->pos -= cam->dir * (speedBack*seconds);
		if (speedRight) cam->pos += cam->right * (speedRight*seconds);
		if (speedLeft) cam->pos -= cam->right * (speedLeft*seconds);
		if (speedForward || speedBack || speedRight || speedLeft)
		{
			if (cam==light) solver->reportDirectIlluminationChange(0,true,true);
		}
	}
	prev = now;

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

	rr_io::registerLoaders();

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
#ifdef __APPLE__
//	OSX kills events ruthlessly
//	see http://stackoverflow.com/questions/728049/glutpassivemotionfunc-and-glutwarpmousepointer
	CGSetLocalEventsSuppressionInterval(0.0);
#endif

	// init GLEW
	if (glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if (sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	
	// init shaders
	uberProgram = rr_gl::UberProgram::create("../../data/shaders/ubershader.vs", "../../data/shaders/ubershader.fs");

	// init solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new rr_gl::RRDynamicSolverGL("../../data/shaders/");
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
		// realtime per-vertex
		solver->getStaticObjects()[i]->illumination.getLayer(LAYER_REALTIME) = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,rr::BF_RGBF,false,NULL);
		// offline per-vertex
		solver->getStaticObjects()[i]->illumination.getLayer(LAYER_OFFLINE_VERTEX) = rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,rr::BF_RGBF,false,NULL);
		// offline per-pixel
		unsigned res = 16;
		float sizeFactor = 5;
		while (res<2048 && (float)res<sizeFactor*sqrtf((float)(solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumTriangles()))) res*=2;
		solver->getStaticObjects()[i]->illumination.getLayer(LAYER_OFFLINE_PIXEL) = rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
	}
	// create remaining cubemaps and multiObject vertex buffers
	solver->allocateBuffersForRealtimeGI(LAYER_REALTIME,4,16,-1,true,false);
	solver->allocateBuffersForRealtimeGI(LAYER_OFFLINE_VERTEX,0,0,-1,true,false);

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLight(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f);
	rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/maps/spot0.png");
	rr::RRLights rrlights;
	rrlights.push_back(rrlight);
	solver->setLights(rrlights);
	light = solver->realtimeLights[0]->getParent();

	glutMainLoop();
	return 0;
}
