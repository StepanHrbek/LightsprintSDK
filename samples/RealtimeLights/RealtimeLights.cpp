// --------------------------------------------------------------------------
// RealtimeLights sample
//
// This is a viewer of Collada .DAE scenes with multiple lights.
// (other samples work with 1 light only, for simplicity)
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//  +-= change brightness
//  */= change contrast
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include "FCollada.h" // must be included before LightsprintGL because of fcollada SAFE_DELETE macro
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
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

rr_gl::Camera              eye(-1.416,1.741,-3.646, 12.230,0,0.050,1.3,70.0,0.1,100.0);
rr_gl::AreaLight*          areaLight = NULL;
rr_gl::Texture*            lightDirectMap = NULL;
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
float                      brightness[4] = {1,1,1,1};
float                      gamma = 1;

/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(rr_gl::UberProgramSetup uberProgramSetup)
{
	// render static scene
	rendererOfScene->setParams(uberProgramSetup,areaLight,lightDirectMap);
	rendererOfScene->useOptimizedScene();
	rendererOfScene->setBrightnessGamma(brightness,gamma);
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
		robot->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye,brightness,gamma);
	}
	if(potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if(uberProgramSetup.LIGHT_INDIRECT_ENV)
			solver->updateEnvironmentMap(potato->illumination);
		potato->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,eye,brightness,gamma);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	rr_gl::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	rr_gl::Texture* shadowmap = areaLight->getShadowMap(mapIndex);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	rr_gl::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
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
		while(res<2048 && res<sizeFactor*sqrtf(object->getCollider()->getMesh()->getNumTriangles())) res*=2;
		return createIlluminationPixelBuffer(res,res);
	}
	// skipped, material properties were already readen from .dae and never change
	virtual void detectMaterials() {}
	// detects direct illumination irradiances on all faces in scene
	virtual unsigned* detectDirectIllumination()
	{
		// don't try to detect when window is not created yet
		if(!winWidth) return NULL;

		// shadowmap could be outdated, update it
		updateShadowmap(0);

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
		case '+':
			for(unsigned i=0;i<4;i++) brightness[i] *= 1.2;
			break;
		case '-':
			for(unsigned i=0;i<4;i++) brightness[i] /= 1.2;
			break;
		case '*':
			gamma *= 1.2;
			break;
		case '/':
			gamma /= 1.2;
			break;
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
			eye.angle -= 0.005*x;
			eye.angleX -= 0.005*y;
			CLAMP(eye.angleX,-M_PI*0.49f,M_PI*0.49f);
		}
		else
		{
			areaLight->getParent()->angle -= 0.005*x;
			areaLight->getParent()->angleX -= 0.005*y;
			CLAMP(areaLight->getParent()->angleX,-M_PI*0.49f,M_PI*0.49f);
			solver->reportDirectIlluminationChange(true);
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	eye.update(0);
	areaLight->getParent()->update(0.3f);
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++) updateShadowmap(i);

	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	renderScene(uberProgramSetup);

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
		rr_gl::Camera* cam = modeMovingEye?&eye:areaLight->getParent();
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam!=&eye) solver->reportDirectIlluminationChange(true);
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
	rr::RRReporter::setFilter(true,3,true);

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
	uberProgram = rr_gl::UberProgram::create("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");
	
	// init textures
	lightDirectMap = rr_gl::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.png not found.\n",false);

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	robot = DynamicObject::create("..\\..\\data\\objects\\I_Robot_female.3ds",0.3f,material,16,16);
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("..\\..\\data\\objects\\potato\\potato01.3ds",0.004f,material,16,16);

	// init static scene and solver and lights
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
	solver->setLights(*adaptLightsFromFCollada(collada));
	rr_gl::Camera* light = new rr_gl::Camera(*solver->getLights()[0]);
	areaLight = new rr_gl::AreaLight(light,1,512);
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(solver->loadIlluminationEnvironmentMap("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true));
	rendererOfScene = new rr_gl::RendererOfScene(solver,"../../data/shaders/");
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);
	solver->calculate();

	glutMainLoop();
	return 0;
}
