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
// In comparison to SceneViewer sample, this one lacks features,
// but is more open for your experiments, customization.
// Everything important is in source code below.
//
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
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
#ifdef __APPLE__
	#include <GLUT/glut.h>
	#include <ApplicationServices/ApplicationServices.h>
#else
	#include <GL/glut.h>
#endif
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/IO/ImportScene.h"

// only longjmp can break us from glut mainloop
#include <setjmp.h>
jmp_buf jmp;

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

rr_gl::RRDynamicSolverGL*  solver = NULL;
rr::RRObject*              robot = NULL;
rr::RRObject*              potato = NULL;
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
		case ' ':
			//printf("camera(%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f);\n",eye.pos[0],eye.pos[1],eye.pos[2],fmodf(eye.angle+100*RR_PI,2*RR_PI),eye.leanAngle,eye.angleX,eye.aspect,eye.fieldOfView,eye.anear,eye.afar);
			rotation = (clock()%10000000)*0.07f;
			solver->reportDirectIlluminationChange(-1,true,true);
			// move dynamic objects
			transformObject(robot,rr::RRVec3(-1.83f,0,-3),rr::RRVec2(rotation,0));
			transformObject(potato,rr::RRVec3(0.4f*sin(rotation*0.05f)+1,1.0f,0.2f),rr::RRVec2(rotation/2,0));
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
			// only longjmp can break us from glut mainloop
			// (exceptions don't propagate through foreign stack)
			longjmp(jmp,0);
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
	solver->renderScene(uberProgramSetup,NULL,true,0,-1,0,false,&brightness,contrast);

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
		if (speedForward) cam->pos += cam->dir * (speedForward*seconds);
		if (speedBack) cam->pos -= cam->dir * (speedBack*seconds);
		if (speedRight) cam->pos += cam->right * (speedRight*seconds);
		if (speedLeft) cam->pos -= cam->right * (speedLeft*seconds);
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

int main(int argc, char** argv)
{
#ifdef _WIN32
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 2305;
#endif // _WIN32

	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	//rr::RRReporter::setFilter(true,3,true);
	//rr_gl::Program::logMessages(true);

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
	solver = new rr_gl::RRDynamicSolverGL("../../data/shaders/");
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	// load static scene
	rr::RRScene staticScene("../../data/scenes/koupelna/koupelna4.dae");
	solver->setStaticObjects(staticScene.objects, NULL);

	// load dynamic objects
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
	keyboard(' ',0,0); // set initial positions

	// init environment
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// init lights
	solver->setLights(staticScene.lights);

	// init buffers for calculated illumination
	solver->allocateBuffersForRealtimeGI(0);

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
#ifdef __APPLE__
//	OSX kills events ruthlessly
//	see http://stackoverflow.com/questions/728049/glutpassivemotionfunc-and-glutwarpmousepointer
	CGSetLocalEventsSuppressionInterval(0.0);
#endif

	solver->observer = &eye; // solver automatically updates lights that depend on camera

	// only longjmp can break us from glut mainloop
	if (!setjmp(jmp))
		glutMainLoop();

	// free memory (just to check for leaks)
	rr_gl::deleteAllTextures();
	delete solver->getEnvironment();
	delete solver->getScaler();
	delete solver;
	delete robot;
	delete potato;
	delete reporter;
	return 0;
}
