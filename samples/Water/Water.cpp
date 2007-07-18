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
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Lightsprint/DemoEngine/Timer.h"
#include "Lightsprint/DemoEngine/Water.h"
#include "Lightsprint/DemoEngine/TextureRenderer.h"


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

rr_gl::Camera          eye(-1.416,1.741,-3.646, 12.230,0,0.050,1.3,70.0,0.3,60.0);
rr_gl::Texture*        environmentMap = NULL;
rr_gl::TextureRenderer*textureRenderer = NULL;
rr_gl::Water*          water = NULL;
int                 winWidth = 0;
int                 winHeight = 0;


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if(!winWidth || !winHeight) return; // can't display without window

	// init water reflection
	water->updateReflectionInit(winWidth/4,winHeight/4,&eye);
	textureRenderer->renderEnvironment(environmentMap,NULL);
	water->updateReflectionDone();

	// render everything except water
	eye.setupForRender();
	textureRenderer->renderEnvironment(environmentMap,NULL);

	// render water
	water->render(100);

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
	eye.aspect = (double) winWidth / (double) winHeight;
}

void passive(int x, int y)
{
	if(!winWidth || !winHeight) return;
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
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
	if(glewInit()!=GLEW_OK) error("GLEW init failed.\n",true);

	// init GL
	int major, minor;
	if(sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2)
		error("OpenGL 2.0 capable graphics card is required.\n",true);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDisable(GL_DEPTH_TEST);

	// init shaders
	water = new rr_gl::Water("..\\..\\data\\shaders\\",true,false);
	textureRenderer = new rr_gl::TextureRenderer("..\\..\\data\\shaders\\");
	
	// init textures
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	environmentMap = rr_gl::Texture::load("..\\..\\data\\maps\\skybox\\skybox_%s.jpg",cubeSideNames,true,true,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);

	glutMainLoop();
	return 0;
}
