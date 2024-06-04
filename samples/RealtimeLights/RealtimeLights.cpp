// --------------------------------------------------------------------------
// RealtimeLights sample
//
// This is a viewer of Collada/3ds/etc scenes with
// - realtime GI from lights, emissive materials, skybox
// - lights can be moved
// - dynamic objects move
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
//  wheel = change camera FOV
//
// Hint: press 1 or 2 and left/right arrows to move lights
//
// In comparison to SceneViewer sample, this one lacks features,
// but is more open for your experiments, customization.
// Everything important is in source code below.
// --------------------------------------------------------------------------

#ifdef _WIN32
	#include <windows.h>    // SetCurrentDirectory
#endif
#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "Lightsprint/GL/RRSolverGL.h"
#include "Lightsprint/GL/PluginScene.h"
#include "Lightsprint/GL/PluginSky.h"
#include "Lightsprint/GL/PluginSSGI.h"
#include "Lightsprint/IO/IO.h"
#ifdef __APPLE__
	#include <GLUT/glut.h>
	#include <ApplicationServices/ApplicationServices.h>
#else
	#include <GL/glut.h>
#endif

// only longjmp can break us from glut mainloop
#include <setjmp.h>
jmp_buf jmp;

// arbitrary unique non-negative numbers
enum
{
	LAYER_LIGHTMAPS,
	LAYER_ENVIRONMENT,
};

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

rr_gl::RRSolverGL*  solver = nullptr;
rr::RRObject*              robot = nullptr;
rr::RRObject*              potato = nullptr;
rr::RRCamera               eye(rr::RRVec3(-0.856f,1.440f,2.097f), rr::RRVec3(5.744f,0.02f,0), 1.3f,110,0.1f,1000);
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
	object->getCollider()->getMesh()->getAABB(&mini,nullptr,&center);
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
	rr_gl::glViewport(0, 0, w, h);
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
		rr::RRCamera& cam = modeMovingEye ? eye : *solver->realtimeLights[selectedLightIndex]->getCamera();
		rr::RRVec3 yawPitchRollRad = cam.getYawPitchRollRad()-rr::RRVec3(x,y,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		cam.setYawPitchRollRad(yawPitchRollRad);
		if (!modeMovingEye)
			solver->reportDirectIlluminationChange(selectedLightIndex,true,true,false);
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

	// move dynamic objects
	rotation = (clock()%10000000)*0.07f;
	transformObject(robot,rr::RRVec3(-1.83f,0,-3),rr::RRVec2(rotation,0));
	transformObject(potato,rr::RRVec3(0.4f*sin(rotation*0.05f)+1,1.0f,0.2f),rr::RRVec2(rotation/2,0));
	solver->reportDirectIlluminationChange(-1,true,true,false);

	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	// configure plugins
	rr_gl::PluginParamsSky ppSky(nullptr,solver,1);
	rr_gl::PluginParamsScene ppScene(&ppSky,solver);
	ppScene.solver = solver;
	ppScene.lights = &solver->realtimeLights;
	ppScene.uberProgramSetup.enableAllLights();
	ppScene.uberProgramSetup.enableAllMaterials();
	ppScene.updateLayerLightmap = true;
	ppScene.updateLayerEnvironment = true;
	ppScene.layerLightmap = LAYER_LIGHTMAPS;
	ppScene.layerEnvironment = LAYER_ENVIRONMENT;
	rr_gl::PluginParamsSSGI ppSSGI(&ppScene,1,0.3f,0.1f);
	rr_gl::PluginParamsShared ppShared;
	ppShared.camera = &eye;
	ppShared.viewport[2] = winWidth;
	ppShared.viewport[3] = winHeight;
	ppShared.brightness = brightness;
	ppShared.gamma = contrast;
	// render scene
	solver->getRenderer()->render(&ppSSGI,ppShared);
	// render light frustum
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
	rr::RRCamera* cam = modeMovingEye?&eye:solver->realtimeLights[selectedLightIndex]->getCamera();
	if (speedForward || speedBack || speedRight || speedLeft)
	{
		cam->setPosition(cam->getPosition()
			+ cam->getDirection() * ((speedForward-speedBack)*seconds)
			+ cam->getRight() * ((speedRight-speedLeft)*seconds)
			);
		if (cam!=&eye) 
		{
			solver->reportDirectIlluminationChange(selectedLightIndex,true,true,false);
		}
	}

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

	rr_io::registerIO(argc,argv);

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint RealtimeLights"); // for windowed mode

	// init GL
	const char* err = rr_gl::initializeGL(true);
	if (err)
		error(err,true);
	
#ifdef _WIN32
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	SetCurrentDirectoryA(exedir);
	free(exedir);
#endif // _WIN32

	// init solver
	solver = new rr_gl::RRSolverGL("../../data/shaders/","../../data/maps/");
	solver->setColorSpace(rr::RRColorSpace::create_sRGB()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	// load static scene
	rr::RRScene staticScene("../../data/scenes/koupelna/koupelna4.dae");
	solver->setStaticObjects(staticScene.objects, nullptr);

	// load dynamic objects
	rr::RRScene robotScene("../../data/objects/I_Robot_female.3ds");
	robotScene.normalizeUnits(0.3f);
	rr::RRScene potatoScene("../../data/objects/potato/potato01.3ds");
	potatoScene.normalizeUnits(0.004f);
	bool aborting = false;
	robot = robotScene.objects.createMultiObject(rr::RRCollider::IT_LINEAR,aborting,-1,0,true,0,nullptr);
	potato = potatoScene.objects.createMultiObject(rr::RRCollider::IT_LINEAR,aborting,-1,0,true,0,nullptr);
	if (robot) robot->isDynamic = true;
	if (potato) potato->isDynamic = true;
	rr::RRObjects dynamicObjects;
	dynamicObjects.push_back(robot);
	dynamicObjects.push_back(potato);
	solver->setDynamicObjects(dynamicObjects);

	// init environment
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// init lights
	solver->setLights(staticScene.lights);

	// init buffers for calculated illumination
	solver->allocateBuffersForRealtimeGI(LAYER_LIGHTMAPS,LAYER_ENVIRONMENT);

	// enable Fireball - faster, higher quality, smaller realtime global illumination solver
	solver->loadFireball("",true) || solver->buildFireball(350,"");

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

	rr::RRReporter::report(rr::INF2,
		"\n"
		"For complete description, see comment at the beginning of sample source code.\n"
		"\n"
		"Controls:\n"
		"  1..9 = switch to n-th light\n"
		"  arrows = move camera or light\n"
		"  mouse = rotate camera or light\n"
		"  left button = switch between camera and light\n"
		"  + - = change brightness\n"
		"  * / = change contrast\n"
		"  wheel = change camera FOV\n"
		"\n");

	// only longjmp can break us from glut mainloop
	if (!setjmp(jmp))
		glutMainLoop();

	// free memory (just to check for leaks)
	rr_gl::deleteAllTextures();
	delete solver->getEnvironment();
	delete solver->getColorSpace();
	delete solver;
	delete robot;
	delete potato;
	delete reporter;
	return 0;
}
