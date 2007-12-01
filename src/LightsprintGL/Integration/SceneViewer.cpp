// --------------------------------------------------------------------------
// Viewer of scene.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/RendererOfScene.h"
#include "Lightsprint/GL/Timer.h"
#include <cstdio>
#include <GL/glew.h>
#include <GL/glut.h>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if(gfxRelated)
		rr::RRReporter::report(rr::INF1,"Please update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families\n");
	if(glutGameModeGet(GLUT_GAME_MODE_ACTIVE))
		glutLeaveGameMode();
	else
		glutDestroyWindow(glutGetWindow());
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// globals are ugly, but required by GLUT design with callbacks

class Solver*              solver = NULL;
rr_gl::Camera              eye(-1.856,1.440,2.097,2.404,0.000,0.020,1.3,110.0,0.1,1000.0);
unsigned                   selectedLightIndex = 0; // index into lights, light controlled by mouse/arrows
int                        winWidth = 0;
int                        winHeight = 0;
bool                       renderLights = 1;
bool                       renderAmbient = 0;
bool                       modeMovingEye = true;
float                      speedGlobal = 1;
float                      speedForward = 0;
float                      speedBack = 0;
float                      speedRight = 0;
float                      speedLeft = 0;
rr::RRVec4                 brightness(1);
float                      gamma = 1;


/////////////////////////////////////////////////////////////////////////////
//
// GI solver and renderer

class Solver : public rr_gl::RRDynamicSolverGL
{
public:
	rr_gl::RendererOfScene* rendererOfScene;

	Solver(const char* pathToShaders) : RRDynamicSolverGL(pathToShaders)
	{
		rendererOfScene = new rr_gl::RendererOfScene(this,pathToShaders);
	}
	~Solver()
	{
		delete rendererOfScene;
	}
	virtual void renderScene(rr_gl::UberProgramSetup uberProgramSetup)
	{
		const rr::RRVector<rr_gl::RealtimeLight*>* lights = uberProgramSetup.LIGHT_DIRECT ? &realtimeLights : NULL;

		// render static scene
		rendererOfScene->setParams(uberProgramSetup,lights);
		rendererOfScene->useOptimizedScene();
		rendererOfScene->setBrightnessGamma(&brightness,gamma);
		rendererOfScene->render();
	}

protected:
	virtual unsigned* detectDirectIllumination()
	{
		if(!winWidth) return NULL;
		return rr_gl::RRDynamicSolverGL::detectDirectIllumination();
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// menu

class Menu
{
public:
	Menu(Solver* solver)
	{
		// select submenu
		int selectHandle = glutCreateMenu(selectCallback);
		glutAddMenuEntry("camera", -1);
		for(unsigned i=0;i<solver->getLights().size();i++)
			glutAddMenuEntry("light", i);
		for(unsigned i=0;i<solver->getStaticObjects().size();i++)
			glutAddMenuEntry("object", 1000+i);

		// speed submenu
		int speedHandle = glutCreateMenu(speedCallback);
		glutAddMenuEntry("1/256", 1);
		glutAddMenuEntry("1/64", 4);
		glutAddMenuEntry("1/16", 16);
		glutAddMenuEntry("1/4", 64);
		glutAddMenuEntry("1", 256);
		glutAddMenuEntry("4", 1024);
		glutAddMenuEntry("16", 4096);
		glutAddMenuEntry("64", 16384);
		glutAddMenuEntry("256", 65536);

		// main menu
		glutCreateMenu(mainCallback);
		glutAddSubMenu("Select...", selectHandle);
		glutAddSubMenu("Speed...", speedHandle);
		glutAddMenuEntry("Toggle render ambient", ME_RENDER_AMBIENT);
		glutAddMenuEntry("Toggle render helpers", ME_RENDER_HELPERS);
		glutAttachMenu(GLUT_RIGHT_BUTTON);
	}
	static void mainCallback(int item)
	{
		switch(item)
		{
			case ME_RENDER_AMBIENT:
				renderAmbient = !renderAmbient;
				break;
			case ME_RENDER_HELPERS:
				renderLights = !renderLights;
				break;
		}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void selectCallback(int item)
	{
		if(item<0) modeMovingEye = 1;
		if(item>=0 && item<1000) {modeMovingEye = 0; selectedLightIndex = item;}
		glutWarpPointer(winWidth/2,winHeight/2);
	}
	static void speedCallback(int item)
	{
		speedGlobal = item/256.f;
		glutWarpPointer(winWidth/2,winHeight/2);
	}
protected:
	enum
	{
		ME_RENDER_AMBIENT,
		ME_RENDER_HELPERS,
	};
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
			brightness *= 1.2;
			break;
		case '-':
			brightness /= 1.2;
			break;
		case '*':
			gamma *= 1.2;
			break;
		case '/':
			gamma /= 1.2;
			break;

		//case 27:
		//	delete solver;
		//	exit(0);
	}
	solver->reportInteraction();
}

void reshape(int w, int h)
{
	winWidth = w;
	winHeight = h;
	glViewport(0, 0, w, h);
	eye.aspect = (double) winWidth / (double) winHeight;
	rr_gl::Texture* texture = rr_gl::Texture::createShadowmap(64,64);
	GLint shadowDepthBits = texture->getTexelBits();
	delete texture;
	glPolygonOffset(1, 12 << (shadowDepthBits-16) );
}

void mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && solver->realtimeLights.size())
		modeMovingEye = !modeMovingEye;
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		if(eye.fieldOfView>13) eye.fieldOfView -= 10;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		if(eye.fieldOfView<130) eye.fieldOfView+=10;
	}
	solver->reportInteraction();
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
			rr_gl::Camera* light = solver->realtimeLights[selectedLightIndex]->getParent();
			light->angle -= 0.005*x;
			light->angleX -= 0.005*y;
			CLAMP(light->angleX,-M_PI*0.49f,M_PI*0.49f);
			solver->reportDirectIlluminationChange(true);
			solver->realtimeLights[selectedLightIndex]->dirty = true;
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
	if(!winWidth || !winHeight) return; // can't display without window

	eye.update();

	solver->calculate();

	glClear(GL_DEPTH_BUFFER_BIT);
	eye.setupForRender();
	rr_gl::UberProgramSetup uberProgramSetup;
	uberProgramSetup.SHADOW_MAPS = 1;
	uberProgramSetup.SHADOW_SAMPLES = 1;
	uberProgramSetup.LIGHT_DIRECT = true;
	uberProgramSetup.LIGHT_DIRECT_COLOR = true;
	uberProgramSetup.LIGHT_DIRECT_MAP = true;
	uberProgramSetup.LIGHT_INDIRECT_CONST = renderAmbient;
	uberProgramSetup.LIGHT_INDIRECT_VCOLOR = true;
	uberProgramSetup.MATERIAL_DIFFUSE = true;
	uberProgramSetup.MATERIAL_DIFFUSE_VCOLOR = true;
	uberProgramSetup.POSTPROCESS_BRIGHTNESS = true;
	uberProgramSetup.POSTPROCESS_GAMMA = true;
	solver->renderScene(uberProgramSetup);

	if(renderLights)
	{
		solver->renderLights();
		glBegin(GL_LINES);
		enum {LINES=100, SIZE=100};
		for(unsigned i=0;i<LINES+1;i++)
		{
			if(i==LINES/2)
			{
				glColor3f(0,0,1);
				glVertex3f(0,-0.5*SIZE,0);
				glVertex3f(0,+0.5*SIZE,0);
			}
			else
				glColor3f(0,0,0.3f);
			float q = (i/float(LINES)-0.5f)*SIZE;
			glVertex3f(q,0,-0.5*SIZE);
			glVertex3f(q,0,+0.5*SIZE);
			glVertex3f(-0.5*SIZE,0,q);
			glVertex3f(+0.5*SIZE,0,q);
		}
		glEnd();
	}

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
		seconds *= speedGlobal;
		rr_gl::Camera* cam = modeMovingEye?&eye:solver->realtimeLights[selectedLightIndex]->getParent();
		if(speedForward) cam->moveForward(speedForward*seconds);
		if(speedBack) cam->moveBack(speedBack*seconds);
		if(speedRight) cam->moveRight(speedRight*seconds);
		if(speedLeft) cam->moveLeft(speedLeft*seconds);
		if(speedForward || speedBack || speedRight || speedLeft)
		{
			if(cam!=&eye) 
			{
				solver->reportDirectIlluminationChange(true);
				solver->realtimeLights[selectedLightIndex]->dirty = true;
				if(speedForward) cam->moveForward(speedForward*seconds);
			}
		}
	}
	prev = now;

	glutPostRedisplay();
}

void sceneViewer(rr::RRDynamicSolver* _solver, const char* pathToShaders)
{
	// init GLUT
	int argc=1;
	char* argv[] = {"abc",NULL};
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Scene Debugger"); // for windowed mode
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

	// init solver
	solver = new Solver(pathToShaders);
	solver->setScaler(_solver->getScaler());
	solver->setEnvironment(_solver->getEnvironment());
	solver->setStaticObjects(_solver->getStaticObjects(),NULL);
	solver->setLights(_solver->getLights());
	solver->observer = &eye; // solver automatically updates lights that depend on camera
	//solver->loadFireball(NULL) || solver->buildFireball(5000,NULL);
	solver->calculate();

	Menu menu(solver);

	// run
	glutMainLoop();
}

}; // namespace
