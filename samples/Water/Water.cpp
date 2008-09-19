// --------------------------------------------------------------------------
// Water sample
//
// This is the most simple use of DemoEngine,
// scene with skybox and reflective water is rendered.
//
// Known issue: temporarily incorrect water reflection (wrong clipping).
//
// Controls:
//  mouse = look around
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2008
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/IO/ImportScene.h"
#include "Lightsprint/GL/Timer.h"
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDebug.h"


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
// globals are ugly, but required by GLUT design with callbacks

rr_gl::Camera           eye(-1.416,1.741,-3.646, 12.230,0,0.050,1.3,70.0,0.3,60.0);
rr_gl::Texture*         environmentMap = NULL;
rr_gl::TextureRenderer* textureRenderer = NULL;
rr_gl::Water*           water = NULL;
int                     winWidth = 0;
int                     winHeight = 0;


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// init water reflection
	water->updateReflectionInit(winWidth/4,winHeight/4,&eye);
	textureRenderer->renderEnvironment(environmentMap,NULL);
	water->updateReflectionDone();

	// render everything except water
	eye.setupForRender();
	textureRenderer->renderEnvironment(environmentMap,NULL);

	// render water
	water->render(100,rr::RRVec3(0));

	glutSwapBuffers();
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
}

void passive(int x, int y)
{
	if (!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if (x || y)
	{
		eye.angle -= 0.005*x;
		eye.angleX -= 0.005*y;
		CLAMP(eye.angleX,-M_PI*0.49f,M_PI*0.49f);
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void idle()
{
	glutPostRedisplay();
}


/////////////////////////////////////////////////////////////////////////////
//
// main

int main(int argc, char **argv)
{
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());

	// init GLUT
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	//glutGameModeString("800x600:32"); glutEnterGameMode(); // for fullscreen mode
	glutInitWindowSize(800,600);glutCreateWindow("Lightsprint Water"); // for windowed mode
	glutSetCursor(GLUT_CURSOR_NONE);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
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
	glDisable(GL_DEPTH_TEST);

	// init shaders
	water = new rr_gl::Water("../../data/shaders/",true,!false);
	textureRenderer = new rr_gl::TextureRenderer("../../data/shaders/");
	
	// init textures
	rr_io::setImageLoader();
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	environmentMap = new rr_gl::Texture(rr::RRBuffer::load("../../data/maps/skybox/skybox_%s.jpg",cubeSideNames,true,true),true,true);

	glutMainLoop();
	return 0;
}
