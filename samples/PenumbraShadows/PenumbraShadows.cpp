// --------------------------------------------------------------------------
// Hello DemoEngine sample
//
// Use of DemoEngine is demonstrated on .3ds scene viewer.
//
// This is RealtimeRadiosity sample without Lightsprint engine,
// the same scene with the same material properties,
// but with constant ambient illumination.
//
// Dynamic objects reflect skybox (visible behind walls).
// You can switch it to constant ambient.
// Compare it with realistic reflection in RealtimeRadiosity
//
// Controls:
//  mouse = look around
//  arrows = move around
//  left button = switch between camera and light
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// Models by Raist, orillionbeta, atp creations
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/DemoEngine/Water.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"
#include "DynamicObject.h"

//#define WATER // enables water


/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 6xxx\n - GeForce 7xxx\n - GeForce 8xxx\n - Radeon 9500-9800\n - Radeon Xxxx\n - Radeon X1xxx");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

Model_3DS           m3ds;
de::Camera          eye = {{-1.416,1.741,-3.646},12.230,0,0.050,1.3,70.0,0.3,60.0};
de::Camera          light = {{-1.802,0.715,0.850},0.635,0,0.300,1.0,70.0,1.0,20.0};
de::AreaLight*      areaLight = NULL;
de::Texture*        lightDirectMap = NULL;
de::Texture*        environmentMap = NULL;
de::TextureRenderer*textureRenderer = NULL;
de::UberProgram*    uberProgram = NULL;
#ifdef WATER
de::Water*          water = NULL;
#endif
DynamicObject*      robot = NULL;
DynamicObject*      potato = NULL;
int                 winWidth = 0;
int                 winHeight = 0;
bool                modeMovingEye = false;
float               speedForward = 0;
float               speedBack = 0;
float               speedRight = 0;
float               speedLeft = 0;


/////////////////////////////////////////////////////////////////////////////
//
// rendering scene

void renderScene(de::UberProgramSetup uberProgramSetup)
{
	// render skybox
	if(uberProgramSetup.LIGHT_DIRECT)
		textureRenderer->renderEnvironment(environmentMap,NULL);

	// render static scene
	if(!uberProgramSetup.useProgram(uberProgram,areaLight,0,lightDirectMap,NULL,1))
		error("Failed to compile or link GLSL program.\n",true);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	m3ds.Draw(NULL,uberProgramSetup.LIGHT_DIRECT,uberProgramSetup.MATERIAL_DIFFUSE_MAP,uberProgramSetup.MATERIAL_EMISSIVE_MAP,NULL,NULL);

	// render dynamic objects
	uberProgramSetup.OBJECT_SPACE = true; // enable object space
	if(uberProgramSetup.SHADOW_MAPS) uberProgramSetup.SHADOW_MAPS = 1; // reduce shadow quality
	// move and rotate object freely, nothing is precomputed
	static float rotation = 0;
	if(!uberProgramSetup.LIGHT_DIRECT) rotation = (timeGetTime()%10000000)*0.07f;
	// render objects
	if(potato)
	{
		potato->worldFoot[0] = 2.2f*sin(rotation*0.005f);
		potato->worldFoot[1] = 1.0f;
		potato->worldFoot[2] = 2.2f;
		if(uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_SPECULAR = true;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = true;
			// LIGHT_INDIRECT_CONST = specular surface reflects constant ambient
			uberProgramSetup.LIGHT_INDIRECT_CONST = false;
			// LIGHT_INDIRECT_ENV = specular surface reflects constant envmap
			uberProgramSetup.LIGHT_INDIRECT_ENV = true;
		}
		potato->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,environmentMap,eye,rotation/2);
	}
	if(robot)
	{
		robot->worldFoot[0] = -1.83f;
		robot->worldFoot[1] = 0;
		robot->worldFoot[2] = -3;
		if(uberProgramSetup.LIGHT_DIRECT)
		{
			uberProgramSetup.MATERIAL_DIFFUSE = false;
			uberProgramSetup.MATERIAL_DIFFUSE_MAP = false;
			uberProgramSetup.MATERIAL_SPECULAR_MAP = false;
		}
		robot->render(uberProgram,uberProgramSetup,areaLight,0,lightDirectMap,environmentMap,eye,rotation);
	}
}

void updateShadowmap(unsigned mapIndex)
{
	de::Camera* lightInstance = areaLight->getInstance(mapIndex);
	lightInstance->setupForRender();
	delete lightInstance;
	glColorMask(0,0,0,0);
	de::Texture* shadowmap = areaLight->getShadowMap(mapIndex);
	glViewport(0, 0, shadowmap->getWidth(), shadowmap->getHeight());
	shadowmap->renderingToBegin();
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	de::UberProgramSetup uberProgramSetup; // default constructor sets all off, perfect for shadowmap
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
	if(!winWidth || !winHeight) return; // can't display without window

	// update shadowmaps
	eye.update(0);
	light.update(0.3f);
	unsigned numInstances = areaLight->getNumInstances();
	for(unsigned i=0;i<numInstances;i++) updateShadowmap(i);

	// update water reflection
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
#ifdef WATER
	water->updateReflectionInit(winWidth/4,winHeight/4,&eye,-0.3f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	renderScene(uberProgramSetup);
	water->updateReflectionDone();
#endif

	// render everything except water
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	uberProgramSetup.SHADOW_MAPS = numInstances;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	renderScene(uberProgramSetup);

#ifdef WATER
	// render water
	water->render(100);
#endif

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
	eye.aspect = (double) winWidth / (double) winHeight;
	GLint shadowDepthBits = areaLight->getShadowMap(0)->getTexelBits();
	glPolygonOffset(4, 42 << (shadowDepthBits-16) );
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
		modeMovingEye = !modeMovingEye;
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
			light.angle -= 0.005*x;
			light.angleX -= 0.005*y;
			CLAMP(light.angleX,-M_PI*0.49f,M_PI*0.49f);
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
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
		de::Camera* cam = modeMovingEye?&eye:&light;
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
	}
	prev = now;

	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Hello Demoengine"); // for windowed mode
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
	uberProgram = de::UberProgram::create("..\\..\\data\\shaders\\ubershader.vs", "..\\..\\data\\shaders\\ubershader.fs");
#ifdef WATER
	water = new de::Water("..\\..\\data\\shaders\\",false,false);
#endif
	textureRenderer = new de::TextureRenderer("..\\..\\data\\shaders\\");
	// for correct soft shadows: maximal number of shadowmaps renderable in one pass is detected
	// set shadowmapsPerPass=1 for standard shadows
	de::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_SAMPLES = 4;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_MAP = true;
	unsigned shadowmapsPerPass = uberProgramSetup.detectMaxShadowmaps(uberProgram,argc,argv);
	if(!shadowmapsPerPass) error("",true);
	
	// init textures
	lightDirectMap = de::Texture::load("..\\..\\data\\maps\\spot0.png", NULL, false, false, GL_LINEAR, GL_LINEAR, GL_CLAMP, GL_CLAMP);
	if(!lightDirectMap)
		error("Texture ..\\..\\data\\maps\\spot0.png not found.\n",false);
	areaLight = new de::AreaLight(&light,shadowmapsPerPass,512);
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	environmentMap = de::Texture::load("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);

	// init static .3ds scene
	if(!m3ds.Load("..\\..\\data\\scenes\\koupelna\\koupelna4.3ds",0.03f))
		error("",false);

	// init dynamic objects
	robot = DynamicObject::create("..\\..\\data\\objects\\I_Robot_female.3ds",0.3f);
	potato = DynamicObject::create("..\\..\\data\\objects\\potato\\potato01.3ds",0.004f);

	glutMainLoop();
	return 0;
}
