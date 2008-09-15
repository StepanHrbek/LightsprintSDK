// --------------------------------------------------------------------------
// MovingSun sample
//
// Shows Sun moving over sky, with global illumination
// rendered on all static and dynamic objects.
//
// When run for first time, precalculations take 2-4 minutes.
//
// Feel free to load your custom Collada scene to see the same effect
// (use drag&drop or commandline parameter).
//
// mouse  = look
// wsadqz = move
// arrows = move sun
// space  = toggle automatic movement of sun and objects
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2008
// Sponza atrium model by Marko Dabrovic
// Character models by 3DRender.fi
// --------------------------------------------------------------------------

#define DEFAULT_SCENE           "../../data/scenes/sponza/sponza.dae"
//#define FORCE_PRECALCULATE            // precalculate even when data are already available
#define NUM_FRAMES              10    // more = more detailed changes in time
#define STATIC_QUALITY          200   // more = less noise. some scenes need at least 1000 (but precalc takes longer)
#define DYNAMIC_OBJECTS         30    // number of characters randomly moving around
#define SUN_SPEED               0.05f // speed of Sun movement
#define OBJ_SPEED               0.05f // speed of dynamic object movement
#define CAM_SPEED               0.04f // relative speed of camera movement (increases when scene size increases)
#define SELECTED_STATIC_OBJECT  9920  // index of static object awarded by higher quality indirect lighting (per-pixel)

#include <cassert>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <windows.h> // SetCurrentDirectoryA
#include <direct.h> // _mkdir
#endif // _MSC_VER
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/IO/ImportScene.h"
#include "../RealtimeRadiosity/DynamicObject.h"

#if defined(LINUX) || defined(linux)
#include <sys/stat.h>
#include <sys/types.h>
#endif

/////////////////////////////////////////////////////////////////////////////
//
// path of sun

// Sets lights in scene (positions, directions, intensities, whole skybox etc) for given animation phase in 0..1 range.
// Sample uses only 1 directional light that moves (code below) and single static skybox (not touched here).
void setupLights(const rr_gl::RRDynamicSolverGL* _solver, const rr_gl::Camera* _observer, float _lightTime01)
{
	RR_ASSERT(_solver);
	RR_ASSERT(_solver->getLights().size()==1);
	_solver->realtimeLights[0]->getParent()->dir = _solver->getLights()[0]->direction = rr::RRVec3(-0.5f,-sin(_lightTime01*3.14f),-cos(_lightTime01*3.14f));
	_solver->realtimeLights[0]->getParent()->updateDirFromAngles = false;
	_solver->realtimeLights[0]->getParent()->update(_observer);
	_solver->realtimeLights[0]->dirtyShadowmap = 1;
}


/////////////////////////////////////////////////////////////////////////////
//
// precalculation

void precalculateOneLayer(rr::RRDynamicSolver* _solver, rr::RRLightField* _lightfield, unsigned _layerNumber)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<_solver->getNumObjects();i++)
	{
		unsigned numVertices = _solver->getObject(i)->getCollider()->getMesh()->getNumVertices();
		if (numVertices)
		{
			_solver->getIllumination(i)->getLayer(_layerNumber) =
				(i==SELECTED_STATIC_OBJECT)
				?
				// allocate lightmap for selected object
				rr::RRBuffer::create(rr::BT_2D_TEXTURE,256,256,1,rr::BF_RGB,true,NULL)
				:
				// allocate vertex buffers for other objects
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,numVertices,1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// precalculate lightmaps
	rr::RRDynamicSolver::UpdateParameters paramsDirect(STATIC_QUALITY);
	rr::RRDynamicSolver::UpdateParameters paramsIndirect = paramsDirect;
	paramsDirect.applyLights = 0; // don't include direct sunlight, it will be computed in realtime
	_solver->updateLightmaps(_layerNumber,-1,-1,&paramsDirect,&paramsIndirect,NULL);

	// precalculate into lightfield
	_lightfield->captureLighting(_solver,_layerNumber);
}

void precalculateAllLayers(rr_gl::RRDynamicSolverGL* _solver, const rr::RRLightField*& _lightfield, float _groundLevel, rr::RRVec3 _aabbMin, rr::RRVec3 _aabbMax)
{
	// allocate lightfield
	rr::RRVec4 lfMin(_aabbMin.x,_groundLevel+1,_aabbMin.z,0);
	rr::RRVec4 lfMax(_aabbMax.x,_groundLevel+1,_aabbMax.z,1); // w: time in 0..1 range
	rr::RRLightField* lightfield = rr::RRLightField::create(lfMin,lfMax-lfMin,2,4,0,NUM_FRAMES); // disabled specular reflection
	_lightfield = lightfield;

	for (unsigned layerNumber=0;layerNumber<NUM_FRAMES;layerNumber++)
	{
		setupLights(_solver,NULL,layerNumber/float(NUM_FRAMES-1));
		precalculateOneLayer(_solver,lightfield,layerNumber);
	}
}

bool loadPrecalculatedData(const rr::RRObjects& _staticObjects, const rr::RRLightField*& _lightfield, std::string _path)
{
	_lightfield = rr::RRLightField::load((_path+"lightfield.lf").c_str());
	if (!_lightfield) return false;
	for (unsigned i=0;i<NUM_FRAMES;i++)
		_staticObjects.loadIllumination(_path.c_str(),i);
	return true;
}

void savePrecalculatedData(const rr::RRObjects& _staticObjects, const rr::RRLightField* _lightfield, std::string _path)
{
#ifdef _MSC_VER
	_mkdir(_path.c_str());
#endif // _MSC_VER
#if defined(LINUX) || defined(linux)
	mkdir(_path.c_str(), 0744);
#endif
	for (unsigned i=0;i<NUM_FRAMES;i++)
		_staticObjects.saveIllumination(_path.c_str(),i);
	_lightfield->save((_path+"lightfield.lf").c_str());
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

class Solver*              solver = NULL;
DynamicObject*             dynamicObject[DYNAMIC_OBJECTS];
rr_gl::Camera              eye(-1.856f,1.440f,2.097f,2.404f,0,0.02f,1.3f,90,0.1f,1000);
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
int                        winWidth = 0;
int                        winHeight = 0;
float                      cameraSpeed = 1; // camera speed in m/sec
bool                       autopilot = true;
float                      objectTime = 0; // arbitrary
float                      lightTime = 0; // arbitrary
float                      lightTime01 = 0; // lightTime doing pingpong in 0..1 range
rr::RRVec4                 brightness(1);
float                      contrast = 1;
const rr::RRLightField*    lightField;
rr_gl::ToneMapping*        toneMapping = NULL;
bool                       keyPressed[512];
rr::RRVec3                 aabbMin,aabbMax; // AABB of static scene
rr::RRReal                 groundLevel = 0;
rr_io::ImportScene*        scene = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx, 9xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


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
		//rr::RRReportInterval report(rr::INF1,"render...\n");
		const rr::RRVector<rr_gl::RealtimeLight*>* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

		// render static scene
		rendererOfScene->setParams(uberProgramSetup,lights,renderingFromThisLight);

		unsigned frame1 = (unsigned)(lightTime01*(NUM_FRAMES-1));
		unsigned frame2 = (frame1+1)%NUM_FRAMES;
		float blend = lightTime01*(NUM_FRAMES-1)-frame1;
		rendererOfScene->useOriginalSceneBlend(frame1,frame2,blend,frame1);
		rendererOfScene->setBrightnessGamma(&brightness,contrast);
		rendererOfScene->render();

		//rr::RRReportInterval report2(rr::INF1,"dynamic...\n");
		// render dynamic objects
		// enable object space
		uberProgramSetup.OBJECT_SPACE = true;
		// when not rendering into shadowmaps, enable environment maps
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
			uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false; // stop using vertex illumination
			uberProgramSetup.LIGHT_INDIRECT_MAP = false; // stop using ambient map illumination
			uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true; // use indirect illumination from envmap
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true; // use indirect illumination from envmap
		}
		// render objects
		for (unsigned i=0;i<DYNAMIC_OBJECTS;i++)
		{
			float a = 1;
			float b = 0.2f;
			float t = objectTime+20.2f*i/DYNAMIC_OBJECTS;
			float s = 6.28f*i/DYNAMIC_OBJECTS;
			dynamicObject[i]->worldFoot = rr::RRVec3(
				(aabbMin.x+aabbMax.x)/2+(aabbMax.x-aabbMin.x)*0.45*( a*cos(t)*cos(s)-b*sin(t)*sin(s) ),
				groundLevel,
				(aabbMin.z+aabbMax.z)/2+(aabbMax.z-aabbMin.z)*0.45*( b*sin(t)*cos(s)+a*cos(t)*sin(s) ));
			dynamicObject[i]->rotYZ = rr::RRVec2(3.14f+3*objectTime+i,0);
			dynamicObject[i]->updatePosition();
			if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			{
				lightField->updateEnvironmentMap(dynamicObject[i]->illumination,lightTime01);
				rr_gl::getTexture(dynamicObject[i]->illumination->diffuseEnvMap,false,false)->reset(false,false);
				//rr_gl::getTexture(dynamicObject[i]->illumination->specularEnvMap,false,false)->reset(false,false);
			}
			dynamicObject[i]->render(uberProgram,uberProgramSetup,lights,0,eye,&brightness,contrast);
		}
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void special(int c, int x, int y)
{
	keyPressed[(c&255)+256] = true;
}

void specialUp(int c, int x, int y)
{
	keyPressed[(c&255)+256] = false;
}

void keyboard(unsigned char c, int x, int y)
{
	keyPressed[c&255] = true;
	switch (c)
	{
		case '+': brightness *= 1.2f; break;
		case '-': brightness /= 1.2f; break;
		case '*': contrast *= 1.2f; break;
		case '/': contrast /= 1.2f; break;
		case ' ': autopilot = !autopilot; break;

		case 27:
			rr_gl::deleteAllTextures();
			delete toneMapping;
			delete lightField;
			delete solver->getEnvironment();
			delete solver->getScaler();
			delete solver->getLights()[0];
			delete solver;
			delete scene;
			for (unsigned i=0;i<DYNAMIC_OBJECTS;i++) delete dynamicObject[i];
			delete rr::RRReporter::getReporter();
			rr::RRReporter::setReporter(NULL);
			exit(0);
	}

	solver->reportInteraction();
}


void keyboardUp(unsigned char c, int x, int y)
{
	keyPressed[c&255] = false;
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = winWidth/(float)winHeight;
}

void mouse(int button, int state, int x, int y)
{
#ifdef GLUT_WHEEL_UP
	if (button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if (eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if (button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if (eye.fieldOfView<130) eye.fieldOfView+=10;
	}
#endif
	solver->reportInteraction();
}

void passive(int x, int y)
{
	if (!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if (x || y)
	{
#if defined(LINUX) || defined(linux)
		const float mouseSensitivity = 0.0002f;
#else
		const float mouseSensitivity = 0.005f;
#endif
		eye.angle -= mouseSensitivity*x;
		eye.angleX -= mouseSensitivity*y;
		CLAMP(eye.angleX,(float)(-M_PI*0.49),(float)(M_PI*0.49));
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

	setupLights(solver,&eye,lightTime01);

	eye.update();

	{
		//rr::RRReportInterval report(rr::INF1,"shadowmaps...\n");
		rr::RRDynamicSolver::CalculateParameters params;
		params.qualityIndirectDynamic = 0; // 0=ignore GI, update only shadowmaps
		solver->calculate(&params);
	}

	//rr::RRReportInterval report2(rr::INF1,"final...\n");
	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	rr_gl::UberProgramSetup uberProgramSetup = solver->getMaterialsInStaticScene();
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_INDIRECT_auto = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	//uberProgramSetup.POSTPROCESS_GAMMA = true;
	solver->renderScene(uberProgramSetup,NULL);

	// adjust tonemapping operator
	if (toneMapping)
	{
		float secondsSinceLastFrame;
		{
			static TIME oldTime = 0;
			TIME newTime = GETTIME;
			secondsSinceLastFrame = oldTime ? (newTime-oldTime)/float(PER_SEC) : 0;
			oldTime = newTime;
		}
		if (secondsSinceLastFrame>0 && secondsSinceLastFrame<10)
			toneMapping->adjustOperator(secondsSinceLastFrame,brightness,contrast);
	}

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
		CLAMP(seconds,0.001f,0.1f);
		float distance = seconds * cameraSpeed;
		rr_gl::Camera* cam = &eye;
		if (autopilot) lightTime += seconds*SUN_SPEED;
		if (keyPressed[GLUT_KEY_RIGHT+256]) lightTime += seconds*SUN_SPEED;
		if (keyPressed[GLUT_KEY_LEFT+256]) lightTime -= seconds*SUN_SPEED;
		if (keyPressed[GLUT_KEY_DOWN+256]) lightTime += seconds*5*SUN_SPEED;
		if (keyPressed[GLUT_KEY_UP+256]) lightTime -= seconds*5*SUN_SPEED;
		if (keyPressed['w']) cam->moveForward(distance);
		if (keyPressed['s']) cam->moveBack(distance);
		if (keyPressed['a']) cam->moveLeft(distance);
		if (keyPressed['d']) cam->moveRight(distance);
		if (keyPressed['q']) cam->moveUp(distance);
		if (keyPressed['z']) cam->moveDown(distance);
		if (autopilot) objectTime += seconds*OBJ_SPEED;
		lightTime01 = fabs(fmod(lightTime,2.0f)-1);
	}
	prev = now;

	glutPostRedisplay();
}

/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
#ifdef _MSC_VER
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
//	_crtBreakAlloc = 1;
#endif // _MSC_VER

	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	//rr::RRReporter::setFilter(true,1,true);
	//rr_gl::Program::logMessages(1);

	rr_io::setImageLoader();

	// init keys
	memset(keyPressed,sizeof(keyPressed),0);

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
	

#ifdef _MSC_VER
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	SetCurrentDirectoryA(exedir);
	free(exedir);
#endif // _MSC_VER

	// init solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new Solver();
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	const char* sceneFilename = (argc>1)?argv[1]:DEFAULT_SCENE;

	// load scene
	scene = new rr_io::ImportScene(sceneFilename,1,true);
	if (!scene->getObjects())
		error("No objects in scene.",false);

	solver->setStaticObjects(*scene->getObjects(), NULL);

	groundLevel = solver->getMultiObjectCustom()->getCollider()->getMesh()->findGroundLevel();
	solver->getMultiObjectCustom()->getCollider()->getMesh()->getAABB(&aabbMin,&aabbMax,NULL);	

	// init environment
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(rr::RRBuffer::load("../../data/maps/skybox/skybox_%s.jpg",cubeSideNames,true,true));
	if (!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// init lights
	{
		rr::RRLights lights;
		lights.push_back(rr::RRLight::createDirectionalLight(rr::RRVec3(1),rr::RRVec3(3),true)); 
		//lights.push_back(rr::RRLight::createSpotLight(rr::RRVec3(1),rr::RRVec3(1),rr::RRVec3(1),0.8f,0.4f)); 
		solver->setLights(lights);
	}

	// precalculate lightmaps and lightfield
	{
		std::string path = std::string(sceneFilename)+".precalc/";
#ifndef FORCE_PRECALCULATE
		if (!loadPrecalculatedData(solver->getStaticObjects(),lightField,path))
#endif
		{
			precalculateAllLayers(solver,lightField,groundLevel,aabbMin,aabbMax);
			savePrecalculatedData(solver->getStaticObjects(),lightField,path);
		}
	}

	// auto-set camera, speed
	if (solver->getMultiObjectCustom())
	{
		//srand((unsigned)time(NULL));
		solver->getMultiObjectCustom()->generateRandomCamera(eye.pos,eye.dir,eye.afar);
		cameraSpeed = eye.afar*CAM_SPEED;
		eye.anear = MAX(0.1f,eye.afar/500); // increase from 0.1 to prevent z fight in big scenes
		eye.afar = MAX(eye.anear+1,eye.afar); // adjust to fit all objects in range
		eye.setDirection(eye.dir);
	}

	// init tonemapping
	toneMapping = new rr_gl::ToneMapping("../../data/shaders/");

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	for (unsigned i=0;i<DYNAMIC_OBJECTS;i++)
	{
		char filename[100];
		sprintf(filename,"../../data/objects/lowpoly/lwpg%04d.3ds",1+(i%9));
		dynamicObject[i] = DynamicObject::create(filename,1,material,16,0);
	}

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

	solver->observer = &eye; // solver automatically updates lights that depend on camera

	glutMainLoop();
	return 0;
}
