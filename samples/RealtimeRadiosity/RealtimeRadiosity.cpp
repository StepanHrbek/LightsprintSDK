// --------------------------------------------------------------------------
// RealtimeRadiosity sample
//
// Shows realtime GI + area light with penumbra shadows + projected video _in custom renderer_.
//
// Advantages
// - source code of all draw commands
// - shaders can be customized (animated robot, potato material)
// - simple, extensible
// Disadvantages
// - you can do nearly the same in 5x fewer lines, see RealtimeLights or Lightmaps
// - custom 3ds renderer is slower, doesn't support transparency etc
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <ctime>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#ifdef __APPLE__
	#include <GLUT/glut.h>
#else
	#include <GL/glut.h>
#endif
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDynamicSolver.h"
#include "../src/LightsprintIO/Import3DS/Model_3DS.h"
#include "../src/LightsprintIO/Import3DS/RRObject3DS.h"
#include "DynamicObject.h"
#include "Lightsprint/IO/ImportScene.h"

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

Model_3DS                  m3ds;
rr_gl::Camera              eye(-1.416f,1.741f,-3.646f, 12.23f,0,0.05f,1.3f,70,0.3f,60);
rr_gl::RealtimeLight*      realtimeLight = NULL;
rr::RRBuffer*              environmentMap = NULL;
rr_gl::TextureRenderer*    textureRenderer = NULL;
rr_gl::UberProgram*        uberProgram = NULL;
rr_gl::RRDynamicSolverGL*  solver = NULL;
DynamicObject*             robot = NULL;
DynamicObject*             potato = NULL;
int                        winWidth = 0;
int                        winHeight = 0;
bool                       modeMovingEye = false;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene via small custom 3ds renderer

// callback that feeds 3ds renderer with our vertex illumination in RGBF format
const float* lockVertexIllum(void* solver,unsigned object)
{
	rr::RRBuffer* vertexBuffer = ((rr::RRDynamicSolver*)solver)->getStaticObjects()[object]->illumination.getLayer(0);
	return vertexBuffer && (vertexBuffer->getFormat()==rr::BF_RGBF) ? (float*)(vertexBuffer->lock(rr::BL_READ)) : NULL;
}

// callback that cleans vertex illumination
void unlockVertexIllum(void* solver,unsigned object)
{
	rr::RRBuffer* vertexBuffer = ((rr::RRDynamicSolver*)solver)->getStaticObjects()[object]->illumination.getLayer(0);
	if (vertexBuffer) vertexBuffer->unlock();
}

// render scene using very simple custom 3ds renderer, see source code in m3ds.Draw()
void renderScene(rr_gl::UberProgramSetup uberProgramSetup)
{
	// render skybox
	if (uberProgramSetup.LIGHT_DIRECT && environmentMap)
		textureRenderer->renderEnvironment(rr_gl::getTexture(environmentMap),NULL,0,NULL,1,false);

	// render static scene
	if (!uberProgramSetup.useProgram(uberProgram,realtimeLight,0,uberProgramSetup.POSTPROCESS_BRIGHTNESS?&rr::RRVec4(2):NULL,1,NULL))
		error("Failed to compile or link GLSL program.\n",true);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	m3ds.Draw(solver,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,uberProgramSetup.LIGHT_INDIRECT_VCOLOR?lockVertexIllum:NULL,unlockVertexIllum);

	// render dynamic objects
	// enable object space
	uberProgramSetup.OBJECT_SPACE = true;
	// when not rendering shadows, enable environment maps
	if (uberProgramSetup.LIGHT_DIRECT)
	{
		uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
		uberProgramSetup.LIGHT_INDIRECT_VCOLOR = false; // stop using vertex illumination
		uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE = true; // use indirect illumination from envmap
		uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true; // use indirect illumination from envmap
	}
	// move and rotate object freely, nothing is precomputed
	float rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*70.f;
	if (robot)
	{
		robot->worldFoot = rr::RRVec3(-1.83f,0,-3);
		//robot->rotYZ = rr::RRVec2(rotation,0); // rotate
		robot->rotYZ = rr::RRVec2(55,0); robot->animationTime = rotation*0.01f; // wave
		robot->updatePosition();
		if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			solver->updateEnvironmentMap(robot->illumination,false);
		robot->render(uberProgram,uberProgramSetup,&solver->realtimeLights,0,eye,NULL,1);
	}
	if (potato)
	{
		potato->worldFoot = rr::RRVec3(2.2f*sin(rotation*0.005f),1.0f,2.2f);
		potato->rotYZ = rr::RRVec2(rotation/2,0);
		potato->updatePosition();
		if (uberProgramSetup.LIGHT_INDIRECT_ENV_DIFFUSE || uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR)
			solver->updateEnvironmentMap(potato->illumination,false);
		potato->render(uberProgram,uberProgramSetup,&solver->realtimeLights,0,eye,NULL,1);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// GI solver
//
// Shadowmaps are updated by solver, therefore to obtain robot shadows, we must
// either send robot to solver                         <-- this is shown in RealtimeLights, Lightmaps etc
// or overload solver->renderScene() to render robot   <-- this is shown here

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	Solver() : RRDynamicSolverGL("../../data/shaders/")
	{
		setDirectIlluminationBoost(2);
	}
protected:
	// called from RRDynamicSolverGL to update shadowmaps
	virtual void renderScene(
		const rr_gl::UberProgramSetup& _uberProgramSetup,
		const rr::RRLight* _renderingFromThisLight,
		bool _updateLightIndirect,
		unsigned _lightIndirectLayer,
		int _lightDetailMapLayer,
		float* _clipPlanes,
		bool _srgbCorrect,
		const rr::RRVec4* _brightness,
		float _gamma)
	{
		// disable all material properties not supported by custom 3ds renderer
		rr_gl::UberProgramSetup uberProgramSetup = _uberProgramSetup;
		//uberProgramSetup.MATERIAL_DIFFUSE
		uberProgramSetup.MATERIAL_DIFFUSE_X2 = false;
		uberProgramSetup.MATERIAL_DIFFUSE_CONST = false;
		//uberProgramSetup.MATERIAL_DIFFUSE_MAP
		//uberProgramSetup.MATERIAL_SPECULAR = false;
		//uberProgramSetup.MATERIAL_SPECULAR_CONST = false;
		uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		uberProgramSetup.MATERIAL_EMISSIVE_CONST = false;
		uberProgramSetup.MATERIAL_EMISSIVE_MAP = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_CONST = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_MAP = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_BLEND = false;
		uberProgramSetup.MATERIAL_TRANSPARENCY_IN_ALPHA = false;
		uberProgramSetup.MATERIAL_NORMAL_MAP = false;
		uberProgramSetup.MATERIAL_CULLING = false;

		// call custom 3ds renderer
		::renderScene(uberProgramSetup);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// this would print diagnostic messages from solver internals
	//solver->checkConsistency();

	// update shadowmaps, lightmaps
	eye.update();
	solver->reportDirectIlluminationChange(0,true,false);
	solver->reportInteraction(); // scene is animated -> call in each frame for higher fps
	solver->calculate();
	static unsigned solutionVersion = 0;
	if (solver->getSolutionVersion()!=solutionVersion)
	{
		solutionVersion = solver->getSolutionVersion();
		solver->updateLightmaps(0,-1,-1,NULL,NULL,NULL);
	}

	// render
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = realtimeLight->getNumShadowmaps();
	uberProgramSetup.SHADOW_SAMPLES = 4; // for 3ds draw, won't be reset by MultiPass
	uberProgramSetup.SHADOW_PENUMBRA = true;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = realtimeLight->getProjectedTexture()?true:false;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	renderScene(uberProgramSetup);

	glutSwapBuffers();
}

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
			realtimeLight->getParent()->angle -= mouseSensitivity*x;
			realtimeLight->getParent()->angleX -= mouseSensitivity*y;
			RR_CLAMP(realtimeLight->getParent()->angleX,(float)(-RR_PI*0.49),(float)(RR_PI*0.49));
			solver->reportDirectIlluminationChange(0,true,true);
			// changes also position a bit, together with rotation
			realtimeLight->getParent()->pos += realtimeLight->getParent()->dir*0.3f;
			realtimeLight->getParent()->update();
			realtimeLight->getParent()->pos -= realtimeLight->getParent()->dir*0.3f;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if (prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		RR_CLAMP(seconds,0.001f,0.3f);
		rr_gl::Camera* cam = modeMovingEye?&eye:realtimeLight->getParent();
		if (speedForward) cam->pos += cam->dir * (speedForward*seconds);
		if (speedBack) cam->pos -= cam->dir * (speedBack*seconds);
		if (speedRight) cam->pos += cam->right * (speedRight*seconds);
		if (speedLeft) cam->pos -= cam->right * (speedLeft*seconds);
		if (speedForward || speedBack || speedRight || speedLeft)
		{
			if (cam!=&eye) solver->reportDirectIlluminationChange(0,true,true);
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
	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::createPrintfReporter();
	//rr_gl::Program::showLog = true; // log also results of shader compiler

	rr_io::registerLoaders();

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Realtime Radiosity"); // for windowed mode
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
	textureRenderer = new rr_gl::TextureRenderer("../../data/shaders/");
	// for soft shadows, keep shadowmapsPerPass=1
	unsigned shadowmapsPerPass = 1;
	// for penumbra shadows: increase it to maximal number of shadowmaps renderable in one pass
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for detectMaxShadowmaps, won't be reset by MultiPass
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	if (!shadowmapsPerPass) error("",true);
	
	// uncomment to load and render and calculate with skybox (not visible from closed scene)
	//environmentMap = rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg");

	// init static .3ds scene
	if (!m3ds.Load("../../data/scenes/koupelna/koupelna4.3DS",NULL,0.03f))
		error("",false);

	// init dynamic objects
	rr_gl::UberProgramSetup material;
	material.MATERIAL_SPECULAR = true;
	material.ANIMATION_WAVE = true;
	robot = DynamicObject::create("../../data/objects/I_Robot_female.3ds",0.3f,material,16,16);
	material.ANIMATION_WAVE = false;
	material.MATERIAL_DIFFUSE = true;
	material.MATERIAL_DIFFUSE_MAP = true;
	material.MATERIAL_SPECULAR_MAP = true;
	potato = DynamicObject::create("../../data/objects/potato/potato01.3ds",0.004f,material,16,16);

	// init realtime radiosity solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	solver = new Solver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setScaler(rr::RRScaler::createRgbScaler());
	solver->setStaticObjects(*adaptObjectsFrom3DS(&m3ds),NULL);
	solver->setEnvironment(environmentMap);
	if (!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
		if (solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices())
			solver->getStaticObjects()[i]->illumination.getLayer(0) =
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getStaticObjects()[i]->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(0.4f,0.2f,1),RR_DEG2RAD(30),0.1f);
	// project texture
	//rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/maps/spot0.png");
	// Project video. You can do this in any other sample or context, simply use video instead of image, then call play().
	rrlight->rtProjectedTexture = rr::RRBuffer::load("../../data/video/Televisi1960.avi");
	rrlight->rtShadowmapSize = 512;
	rr::RRLights rrlights;
	rrlights.push_back(rrlight);
	solver->setLights(rrlights);
	realtimeLight = solver->realtimeLights[0];
	realtimeLight->numInstancesInArea = shadowmapsPerPass;
	realtimeLight->getParent()->setNear(0.5f); // adjusts shadowmapping near plane

	// Enable Fireball - faster, higher quality, smaller realtime global illumination solver for games.
	// You can safely skip it to stay with fully dynamic solver that doesn't need any precalculations.
	solver->loadFireball(NULL,true) || solver->buildFireball(5000,NULL);

	// start playing video
	if (rrlight->rtProjectedTexture)
		rrlight->rtProjectedTexture->play();

	glutMainLoop();
	return 0;
}
