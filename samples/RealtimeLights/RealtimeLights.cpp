// --------------------------------------------------------------------------
// RealtimeLights sample
//
// This is a viewer of Collada/Gamebryo/3ds/etc scenes with
// - realtime GI from lights, emissive materials, skybox
// - lights can be moved
// - dynamic objects can be moved
//
// Use commandline argument or drag&drop to open custom scene.
//
// Controls:
//  1..9 = switch to n-th light
//  arrows = move camera or light
//  mouse = rotate camera or light
//  left button = switch between camera and light
//  + - = change brightness
//  * / = change contrast
//	wheel = change camera FOV
//  space = randomly move dynamic objects
//
// Hint: hold space to see dynamic object occluding light
// Hint: press 1 or 2 and left/right arrows to move lights
//
// In comparison to SceneViewer sample, this one is simpler,
// but open for your experiments, customization.
// Everything important is in source code below.
//
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef _WIN32
#include <windows.h>    // SetCurrentDirectory
#endif
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "../RealtimeRadiosity/DynamicObject.h"
#include "Lightsprint/IO/ImportScene.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5200-9800, 100-295 (including GeForce Go)\n - Radeon 9500-9800, X300-1950, HD2300-5970 (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

class Solver*              solver = NULL;
DynamicObject*             robot;
DynamicObject*             potato;
rr_gl::Camera              eye(-1.856f,1.440f,2.097f,2.404f,0,0.02f,1.3f,110,0.1f,1000);
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = true;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
rr::RRVec4                 brightness(1);
float                      contrast = 1;
float                      rotation = 0;
rr::RRScene*               scene = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// GI solver and renderer

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	rr_gl::RendererOfScene* rendererOfScene;
	rr_gl::UberProgram*     uberProgram;

	Solver() : RRDynamicSolverGL("../../data/shaders/")
	{
		rendererOfScene = new rr_gl::RendererOfScene(this,"../../data/shaders/");
		uberProgram = rr_gl::UberProgram::create("../../data/shaders/ubershader.vs", "../../data/shaders/ubershader.fs");
	}
	~Solver()
	{
		delete uberProgram;
		delete rendererOfScene;
	}
	virtual void renderScene(rr_gl::UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight)
	{
		const rr::RRVector<rr_gl::RealtimeLight*>* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

		// render static scene
		rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight);
		rendererOfScene->useRealtimeGI(0);
		rendererOfScene->setBrightnessGamma(&brightness,contrast);
		rendererOfScene->render();

		// render dynamic objects
		// enable object space
		uberProgramSetup.OBJECT_SPACE = true;
		// when not rendering into shadowmaps, enable environment maps
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
		}
		// render objects
		if (robot)
		{
			robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
			robot->rotYZ = rr::RRVec2(rotation,0);
			robot->updatePosition();
			if (uberProgramSetup.LIGHT_INDIRECT_auto)
				updateEnvironmentMap(robot->illumination);
			robot->render(uberProgram,uberProgramSetup,lights,0,eye,&brightness,contrast,0);
		}
		if (potato)
		{
			potato->worldFoot = rr::RRVec3(0.4f*sin(rotation*0.05f)+1,1.0f,0.2f);
			potato->rotYZ = rr::RRVec2(rotation/2,0);
			potato->updatePosition();
			if (uberProgramSetup.LIGHT_INDIRECT_auto)
				updateEnvironmentMap(potato->illumination);
			potato->render(uberProgram,uberProgramSetup,lights,0,eye,&brightness,contrast,0);
		}
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
			contrast *= 1.2f;
			break;
		case '/':
			contrast /= 1.2f;
			break;
		case ' ':
			//printf("camera(%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f);\n",eye.pos[0],eye.pos[1],eye.pos[2],fmodf(eye.angle+100*RR_PI,2*RR_PI),eye.leanAngle,eye.angleX,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			rotation = (clock()%10000000)*0.07f;
			for (unsigned i=0;i<solver->realtimeLights.size();i++)
			{
				solver->reportDirectIlluminationChange(i,true,true);
			}
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
			selectedLightIndex = RR_MIN(c-'1',(int)solver->getLights().size()-1);
			if (solver->realtimeLights.size()) modeMovingEye = false;
			break;

		case 27:
			rr_gl::deleteAllTextures();
			delete solver->getEnvironment();
			delete solver->getScaler();
			delete solver;
			delete scene;
			delete robot;
			delete potato;
			delete rr::RRReporter::getReporter();
			rr::RRReporter::setReporter(NULL);
			exit(0);
	}
	solver->reportInteraction();
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
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && solver->realtimeLights.size())
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
	solver->reportInteraction();
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
			rr_gl::Camera* light = solver->realtimeLights[selectedLightIndex]->getParent();
			light->angle -= mouseSensitivity*x;
			light->angleX -= mouseSensitivity*y;
			RR_CLAMP(light->angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			solver->reportDirectIlluminationChange(selectedLightIndex,true,true);
			// changes position a bit, together with rotation
			// if we don't call it, solver updates light in a standard way, without position change
			light->pos += light->dir*0.3f;
			light->update();
			light->pos -= light->dir*0.3f;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
		solver->reportInteraction();
	}
}

void display(void)
{
	if (!winWidth || !winHeight)
	{
		winWidth = glutGet(GLUT_WINDOW_WIDTH);
		winHeight = glutGet(GLUT_WINDOW_HEIGHT);
		if (!winWidth || !winHeight) return; // can't display without window
		reshape(winWidth,winHeight);
	}

	eye.update();

	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	eye.setupForRender();
	// configure renderer
	rr_gl::UberProgramSetup uberProgramSetup = solver->getMaterialsInStaticScene(); // enable materials
	uberProgramSetup.SHADOW_MAPS = 1; // enable shadows
	uberProgramSetup.LIGHT_DIRECT = true; // enable direct illumination
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_auto = true; // enable indirect illumination
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true; // enable brightness/gamma adjustment
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	// render scene
	solver->renderScene(uberProgramSetup,NULL);

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
		rr_gl::Camera* cam = modeMovingEye?&eye:solver->realtimeLights[selectedLightIndex]->getParent();
		if (speedForward) cam->moveForward(speedForward*seconds);
		if (speedBack) cam->moveBack(speedBack*seconds);
		if (speedRight) cam->moveRight(speedRight*seconds);
		if (speedLeft) cam->moveLeft(speedLeft*seconds);
		if (speedForward || speedBack || speedRight || speedLeft)
		{
			if (cam!=&eye) 
			{
				solver->reportDirectIlluminationChange(selectedLightIndex,true,true);
			}
		}
	}
	prev = now;

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
#ifdef _WIN32
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 39436;
#endif // _WIN32

	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	//rr::RRReporter::setFilter(true,3,true);

	rr_io::registerLoaders();

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint RealtimeLights"); // for windowed mode

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
	
#ifdef _WIN32
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	SetCurrentDirectoryA(exedir);
	free(exedir);
#endif // _WIN32

	// init solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new Solver();
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	// load scene
	scene = new rr::RRScene("..\\..\\data\\scenes\\koupelna\\koupelna4.dae");
	solver->setStaticObjects(*scene->getObjects(), NULL);

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("../../data/objects/I_Robot_female.3ds",0.3f,material,16,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("../../data/objects/potato/potato01.3ds",0.004f,material,16,16);

	// init environment
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));
	if (!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init lights
	for (unsigned i=0;i<scene->getLights()->size();i++)
		(*scene->getLights())[i]->rtProjectedTextureFilename = _strdup("../../data/maps/spot0.png");
	solver->setLights(*scene->getLights());

	// enable Fireball - faster, higher quality, smaller realtime global illumination solver
	solver->loadFireball(NULL,true) || solver->buildFireball(350,NULL);

	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);
	glutSpecialUpFunc(specialUp);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutPassiveMotionFunc(passive);
	glutIdleFunc(idle);

	solver->observer = &eye; // solver automatically updates lights that depend on camera

	glutMainLoop();
	return 0;
}
