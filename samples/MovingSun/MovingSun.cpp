// --------------------------------------------------------------------------
// MovingSun sample
//
// Shows Sun moving over sky, with global illumination
// rendered on all static and dynamic objects.
//
// Feel free to load your custom Collada scene to see the same effect
// (use drag&drop or commandline parameter).
//
// mouse  = look
// wsadqz = move camera
// space  = toggle automatic movement of sun and objects
// arrows = move sun
//
// Sponza atrium model by Marko Dabrovic
// Character models by 3DRender.fi
// --------------------------------------------------------------------------

#define DEFAULT_SCENE           "../../data/scenes/sponza/sponza.dae"
#define DYNAMIC_OBJECTS         15    // number of characters randomly moving around
#define SUN_SPEED               0.05f // speed of Sun movement
#define OBJ_SPEED               0.05f // speed of dynamic object movement
#define CAM_SPEED               0.04f // relative speed of camera movement (increases when scene size increases)

#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _MSC_VER
	#include <windows.h> // SetCurrentDirectoryA
	#include <direct.h> // _mkdir
#endif // _MSC_VER
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

#if defined(LINUX) || defined(linux)
	#include <sys/stat.h>
	#include <sys/types.h>
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
// path of sun

// Sets lights in scene (positions, directions, intensities, whole skybox etc) for given animation phase in 0..1 range.
// Sample uses only 1 directional light that moves (code below) and single static skybox (not touched here).
void setupLights(rr_gl::RRSolverGL* _solver, const rr::RRCamera* _observer, float _lightTime01)
{
	RR_ASSERT(_solver);
	RR_ASSERT(_solver->getLights().size()==1);
	_solver->realtimeLights[0]->getCamera()->setYawPitchRollRad(rr::RRVec3(0.3f,-RR_PI*sin(_lightTime01*RR_PI),0));
	_solver->realtimeLights[0]->configureCSM(_observer,_solver);
	_solver->realtimeLights[0]->dirtyShadowmap = 1;
	_solver->reportDirectIlluminationChange(0,true,true,false);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

rr_gl::RRSolverGL*  solver = nullptr;
rr::RRCamera               eye(rr::RRVec3(-1.856f,1.440f,2.097f), rr::RRVec3(2.404f,0.02f,0), 1.3f,90,0.1f,1000);
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
int                        winWidth = 0;
int                        winHeight = 0;
float                      cameraSpeed = 1; // camera speed in m/sec
bool                       autopilot = true;
float                      objectTime = 0; // arbitrary
float                      lightTime = 0; // arbitrary
float                      lightTime01 = 0; // lightTime doing pingpong in 0..1 range
rr::RRVec4                 brightness(2);
float                      contrast = 1;
char                       keyPressed[512];
rr::RRReal                 groundLevel = 0;


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
	keyPressed[(c&255)+256] = 1;
}

void specialUp(int c, int x, int y)
{
	keyPressed[(c&255)+256] = 0;
}

void keyboard(unsigned char c, int x, int y)
{
	keyPressed[c&255] = 1;
	switch (c)
	{
		case '+': brightness *= 1.2f; break;
		case '-': brightness /= 1.2f; break;
		case '*': contrast *= 1.2f; break;
		case '/': contrast /= 1.2f; break;
		case ' ': autopilot = !autopilot; break;

		case 27:
			// only longjmp can break us from glut mainloop
			longjmp(jmp,0);
	}

	solver->reportInteraction();
}


void keyboardUp(unsigned char c, int x, int y)
{
	keyPressed[c&255] = 0;
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
		rr::RRVec3 yawPitchRollRad = eye.getYawPitchRollRad()-rr::RRVec3(x,y,0)*mouseSensitivity;
		RR_CLAMP(yawPitchRollRad[1],(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
		eye.setYawPitchRollRad(yawPitchRollRad);
		glutWarpPointer(winWidth/2,winHeight/2);
		solver->reportInteraction();
	}
}

void display(void)
{
	//rr::RRReportInterval report(rr::INF1,"display...\n");
	if (!winWidth || !winHeight)
	{
		winWidth = glutGet(GLUT_WINDOW_WIDTH);
		winHeight = glutGet(GLUT_WINDOW_HEIGHT);
		if (!winWidth || !winHeight) return; // can't display without window
		reshape(winWidth,winHeight);
	}

	// move characters
	rr::RRVec3 aabbMin,aabbMax; // AABB of static scene
	solver->getMultiObject()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,nullptr);	
	for (unsigned i=0;i<DYNAMIC_OBJECTS;i++)
	{
		float a = 1;
		float b = 0.2f;
		float t = objectTime+20.2f*i/DYNAMIC_OBJECTS;
		float s = 6.28f*i/DYNAMIC_OBJECTS;
		transformObject(
			solver->getDynamicObjects()[i],
			rr::RRVec3(
				(aabbMin.x+aabbMax.x)/2+(aabbMax.x-aabbMin.x)*0.45*( a*cos(t)*cos(s)-b*sin(t)*sin(s) ),
				groundLevel,
				(aabbMin.z+aabbMax.z)/2+(aabbMax.z-aabbMin.z)*0.45*( b*sin(t)*cos(s)+a*cos(t)*sin(s) )),
			rr::RRVec2(3.14f+3*objectTime+i,0));
	}

	setupLights(solver,&eye,lightTime01);
	solver->calculate();

	//rr::RRReportInterval report2(rr::INF1,"final...\n");
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
	rr_gl::PluginParamsSSGI ppSSGI(&ppScene,1,1.0f,0.1f);
	rr_gl::PluginParamsShared ppShared;
	ppShared.camera = &eye;
	ppShared.viewport[2] = winWidth;
	ppShared.viewport[3] = winHeight;
	ppShared.brightness = brightness;
	ppShared.gamma = contrast;
	// render scene
	solver->getRenderer()->render(&ppSSGI,ppShared);

	glutSwapBuffers();
}

void idle()
{
	if (!winWidth) return; // can't work without window

	// smooth keyboard movement
	static rr::RRTime time;
	float seconds = time.secondsSinceLastQuery();
	float distance = seconds * cameraSpeed;
	rr::RRCamera* cam = &eye;
	if (autopilot) lightTime += seconds*SUN_SPEED;
	if (keyPressed[GLUT_KEY_RIGHT+256]) lightTime += seconds*SUN_SPEED;
	if (keyPressed[GLUT_KEY_LEFT+256]) lightTime -= seconds*SUN_SPEED;
	if (keyPressed[GLUT_KEY_DOWN+256]) lightTime += seconds*5*SUN_SPEED;
	if (keyPressed[GLUT_KEY_UP+256]) lightTime -= seconds*5*SUN_SPEED;
	cam->setPosition(cam->getPosition()
		// some compilers complaing about indexing by char ['x'],
		// so we index by char16_t [u'x'] just to silence them
		+ cam->getDirection()*(keyPressed[u'w']-keyPressed[u's'])*distance
		+ cam->getRight()*(keyPressed[u'd']-keyPressed[u'a'])*distance
		+ cam->getUp()*(keyPressed[u'q']-keyPressed[u'z'])*distance
		);
	if (autopilot) objectTime += seconds*OBJ_SPEED;
	lightTime01 = fabs(fmod(lightTime,2.0f)-1);

	glutPostRedisplay();
}

/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char** argv)
{
#ifdef _MSC_VER
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
//	_crtBreakAlloc = 1;
#endif // _MSC_VER

	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	//rr::RRReporter::setFilter(true,1,true);

	rr_io::registerIO(argc,argv);

	// init keys
	memset(keyPressed,0,sizeof(keyPressed));

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	{
		unsigned w = glutGet(GLUT_SCREEN_WIDTH);
		unsigned h = glutGet(GLUT_SCREEN_HEIGHT);
		unsigned resolutionx = w-128;
		unsigned resolutiony = h-64;
		glutInitWindowSize(resolutionx,resolutiony);
		glutInitWindowPosition((w-resolutionx)/2,(h-resolutiony)/2);
	}
	glutCreateWindow("Lightsprint Moving Sun"); // for windowed mode

	// init GL
	const char* err = rr_gl::initializeGL(true);
	if (err)
		error(err,true);

#ifdef _MSC_VER
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	SetCurrentDirectoryA(exedir);
	free(exedir);
#endif // _MSC_VER

	// init solver
	solver = new rr_gl::RRSolverGL("../../data/shaders/","../../data/maps/");
	solver->setColorSpace(rr::RRColorSpace::create_sRGB()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	// load scene
	rr::RRScene scene((argc>1)?argv[1]:DEFAULT_SCENE);
	if (!scene.objects.size())
		error("No objects in scene.",false);
	solver->setStaticObjects(scene.objects, nullptr);
	groundLevel = solver->getMultiObject()->getCollider()->getMesh()->findGroundLevel();

	// init environment
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));
	if (!solver->getMultiObject())
		error("No objects in scene.",false);

	// init lights
	{
		rr::RRLights lights;
		lights.push_back(rr::RRLight::createDirectionalLight(rr::RRVec3(1),rr::RRVec3(3),true)); 
		lights[0]->rtNumShadowmaps = 2; // 2 reduces details, but they are hardly visible in this scene; use 3 for bigger scenes or closeups
		//lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(1),rr::RRVec3(1),rr::RRVec3(1),0.8f,0.4f)); 
		solver->setLights(lights);
	}

	// auto-set camera, speed
	//srand((unsigned)time(nullptr));
	eye.setView(rr::RRCamera::RANDOM,solver,nullptr,nullptr);
	cameraSpeed = eye.getFar()*CAM_SPEED;

	// init dynamic objects
	rr::RRScene character1("../../data/objects/lowpoly/lwpg0001.3ds");
	rr::RRScene character2("../../data/objects/lowpoly/lwpg0002.3ds");
	rr::RRScene character3("../../data/objects/lowpoly/lwpg0003.3ds");
	rr::RRScene character4("../../data/objects/lowpoly/lwpg0004.3ds");
	rr::RRScene character5("../../data/objects/lowpoly/lwpg0005.3ds");
	rr::RRScene character6("../../data/objects/lowpoly/lwpg0006.3ds");
	rr::RRScene character7("../../data/objects/lowpoly/lwpg0007.3ds");
	rr::RRScene character8("../../data/objects/lowpoly/lwpg0008.3ds");
	rr::RRScene character9("../../data/objects/lowpoly/lwpg0009.3ds");
	rr::RRScene* characters[9] = {&character1,&character2,&character3,&character4,&character5,&character6,&character7,&character8,&character9};
	rr::RRObjects dynamicObjects;
	for (unsigned i=0;i<DYNAMIC_OBJECTS;i++)
	{
		bool aborting = false;
		rr::RRObject* object = characters[i%9]->objects.createMultiObject(rr::RRCollider::IT_LINEAR,aborting,-1,0,true,0,nullptr);
		object->isDynamic = true;
		dynamicObjects.push_back(object);
	}
	solver->setDynamicObjects(dynamicObjects);

	// init buffers for calculated illumination
	solver->allocateBuffersForRealtimeGI(LAYER_LIGHTMAPS,LAYER_ENVIRONMENT);

	// enable Fireball - faster, higher quality, smaller realtime global illumination solver
	solver->loadFireball("",true) || solver->buildFireball(1000,"");

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
		" mouse  = look\n"
		" wsadqz = move camera\n"
		" space  = toggle automatic movement of sun and objects\n"
		" arrows = move sun\n"
		"\n");

	// only longjmp can break us from glut mainloop
	if (!setjmp(jmp))
		glutMainLoop();

	// free memory (just to check for leaks)
	rr_gl::deleteAllTextures();
	delete solver->getEnvironment();
	delete solver->getColorSpace();
	delete solver->getLights()[0];
	delete solver;
	for (unsigned i=0;i<dynamicObjects.size();i++)
		delete dynamicObjects[i];
	delete reporter;
	return 0;
}
