// --------------------------------------------------------------------------
// Penumbra Shadows sample
//
// Use of LightsprintGL renderer is demonstrated on .3ds scene viewer.
//
// Indirect illumination is approximated by constant ambient.
// Dynamic objects reflect skybox (visible behind walls).
//
// See the same scene with global illumination in RealtimeRadiosity sample
// (it has 100 lines added).
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDebug.h"
#include "DynamicObject.h"
#include "Lightsprint/IO/ImportScene.h"


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	if (gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx, 9xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

Model_3DS              m3ds;
rr_gl::Camera          eye(-1.416f,1.741f,-3.646f, 12.230f,0,0.05f,1.3f,70,0.3f,60);
rr_gl::RealtimeLight*  realtimeLight = NULL;
rr::RRBuffer*          environmentMap = NULL;
rr_gl::TextureRenderer*textureRenderer = NULL;
rr_gl::UberProgram*    uberProgram = NULL;
DynamicObject*         robot = NULL;
DynamicObject*         potato = NULL;
int                    winWidth = 0;
int                    winHeight = 0;
bool                   modeMovingEye = false;
float                  speedForward = 0;
float                  speedBack = 0;
float                  speedRight = 0;
float                  speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(rr_gl::UberProgramSetup uberProgramSetup)
{
	// render skybox
	if (uberProgramSetup.LIGHT_DIRECT)
		textureRenderer->renderEnvironment(rr_gl::getTexture(environmentMap),rr::RRVec4(1),1);

	// render static scene
	if (!uberProgramSetup.useProgram(uberProgram,realtimeLight,0,NULL,1,0))
		error("Failed to compile or link GLSL program.\n",true);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	m3ds.Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);

	// render dynamic objects
	uberProgramSetup.OBJECT_SPACE = true; // enable object space
	if (uberProgramSetup.SHADOW_MAPS) uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
	// move and rotate object freely, nothing is precomputed
	static float rotation = 0;
	if (!uberProgramSetup.LIGHT_DIRECT) rotation = fmod(clock()/float(CLOCKS_PER_SEC),10000)*70;
	// render objects
	if (potato)
	{
		potato->worldFoot[0] = 2.2f*sin(rotation*0.005f);
		potato->worldFoot[1] = 1.0f;
		potato->worldFoot[2] = 2.2f;
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_SPECULAR = true;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = true;
			// LIGHT_INDIRECT_CONST = specular surface reflects constant ambient
			uberProgramSetup.LIGHT_INDIRECT_CONST = false;
			// LIGHT_INDIRECT_ENV = specular surface reflects constant envmap
			uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR = true;
		}
		potato->render(uberProgram,uberProgramSetup,realtimeLight,0,uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR?rr_gl::getTexture(environmentMap):NULL,eye,rotation/2);
	}
	if (robot)
	{
		robot->worldFoot[0] = -1.83f;
		robot->worldFoot[1] = 0;
		robot->worldFoot[2] = -3;
		if (uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_DIFFUSE = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		}
		robot->render(uberProgram,uberProgramSetup,realtimeLight,0,uberProgramSetup.LIGHT_INDIRECT_ENV_SPECULAR?rr_gl::getTexture(environmentMap):NULL,eye,rotation);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	rr_gl::Camera* lightInstance = realtimeLight->getShadowmapCamera(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	rr_gl::Texture* shadowmap = realtimeLight->getShadowmap(mapIndex);
	glViewport(0, 0, shadowmap->getBuffer()->getWidth(), shadowmap->getBuffer()->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	rr_gl::UberProgramSetup uberProgramSetup; // default constructor sets nearly all off, perfect for shadowmap
	uberProgramSetup.MATERIAL_CULLING = 0;
	renderScene(uberProgramSetup);
	shadowmap->renderingToEnd();
	glDisable(GL_POLYGON_OFFSET_FILL);
	glViewport(0, 0, winWidth, winHeight);
	glColorMask(1,1,1,1);
}


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// update shadowmaps
	eye.update();
	realtimeLight->getParent()->update();
	unsigned numInstances = realtimeLight->getNumShadowmaps();
	for (unsigned i=0;i<numInstances;i++) updateShadowmap(i);

	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for 3ds draw, won't be reset by MultiPass
	uberProgramSetup.SHADOW_PENUMBRA = true;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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
			exit(0);
	}
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.setAspect( winWidth/(float)winHeight );
	GLint shadowDepthBits = realtimeLight->getShadowmap(0)->getTexelBits();
	glPolygonOffset(4,(float)(42<<(shadowDepthBits-16)));
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
	if (!winWidth) return; // can't work without window

	// smooth keyboard movement
	static TIME prev = 0;
	TIME now = GETTIME;
	if (prev && now!=prev)
	{
		float seconds = (now-prev)/(float)PER_SEC;
		RR_CLAMP(seconds,0.001f,0.3f);
		rr_gl::Camera* cam = modeMovingEye?&eye:realtimeLight->getParent();
		if (speedForward) cam->moveForward(speedForward*seconds);
		if (speedBack) cam->moveBack(speedBack*seconds);
		if (speedRight) cam->moveRight(speedRight*seconds);
		if (speedLeft) cam->moveLeft(speedLeft*seconds);
	}
	prev = now;

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	rr_io::registerLoaders();

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Penumbra Shadows"); // for windowed mode
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
	// for correct soft shadows: maximal number of shadowmaps renderable in one pass is detected
	// set shadowmapsPerPass=1 for standard shadows
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4; // for detectMaxShadowmaps, won't be reset by MultiPass
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	unsigned shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	if (!shadowmapsPerPass) error("",true);
	
	// init textures
	environmentMap = rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg");

	// init light
	rr::RRLight* rrlight = rr::RRLight::createSpotLightNoAtt(rr::RRVec3(-1.802f,0.715f,0.850f),rr::RRVec3(1),rr::RRVec3(1,0.2f,1),RR_DEG2RAD(40),0.1f);
	rrlight->rtProjectedTextureFilename = _strdup("../../data/maps/spot0.png");
	realtimeLight = new rr_gl::RealtimeLight(*rrlight);
	realtimeLight->numInstancesInArea = shadowmapsPerPass;
	realtimeLight->setShadowmapSize(512);

	// init static .3ds scene
	if (!m3ds.Load("../../data/scenes/koupelna/koupelna4.3DS",0.03f))
		error("",false);

	// init dynamic objects
	robot = DynamicObject::create("../../data/objects/I_Robot_female.3ds",0.3f);
	potato = DynamicObject::create("../../data/objects/potato/potato01.3ds",0.004f);

	glutMainLoop();
	return 0;
}
