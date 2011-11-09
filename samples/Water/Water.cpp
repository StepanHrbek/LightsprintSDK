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
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#ifdef __APPLE__
	#include <GLUT/glut.h>
#else
	#include <GL/glut.h>
#endif
#include "Lightsprint/IO/ImportScene.h"
#include "Lightsprint/GL/Water.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDebug.h"
#ifdef _WIN32
	#include <GL/wglew.h>
#endif


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

rr_gl::Camera           eye(rr::RRVec3(-1,1.8,-3), rr::RRVec3(9.09,-0.250,0),1.3,70.0,0.3,600.0);
rr_gl::Texture*         environmentMap = NULL;
rr_gl::TextureRenderer* textureRenderer = NULL;
rr_gl::Water*           water = NULL;
int                     winWidth = 0;
int                     winHeight = 0;
bool                    editLight = 0;
rr::RRVec3              lightDirection(0.08f,-0.4f,-0.9f);


/////////////////////////////////////////////////////////////////////////////
//
// GLUT callbacks

void display(void)
{
	if (!winWidth || !winHeight) return; // can't display without window

	// init water reflection
	water->updateReflectionInit(winWidth/4,winHeight/4,&eye,0,false);
	textureRenderer->renderEnvironment(environmentMap,NULL,0,NULL,1,false);
	water->updateReflectionDone();

	// render everything except water
	setupForRender(eye);
	textureRenderer->renderEnvironment(environmentMap,NULL,0,NULL,1,false);

	// render water
	water->render(1000,rr::RRVec3(0),rr::RRVec4(0.1f,0.25f,0.35f,0.5f),lightDirection,rr::RRVec3(4));

	glutSwapBuffers();
}

void keyboard(unsigned char c, int x, int y)
{
	switch (c)
	{
		case 'q': eye.setPosition(eye.getPosition()+rr::RRVec3(0,0.1f,0)); break;
		case 'z': eye.setPosition(eye.getPosition()-rr::RRVec3(0,0.1f,0)); break;
		case 'x': eye.setYawPitchRollRad(eye.getYawPitchRollRad()-rr::RRVec3(0,0,0.01f)); break;
		case 'c': eye.setYawPitchRollRad(eye.getYawPitchRollRad()+rr::RRVec3(0,0,0.01f)); break;
		case ' ': editLight = !editLight; break;
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

void passive(int x, int y)
{
	if (!winWidth || !winHeight) return;
	RR_LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if (x || y)
	{
		if (editLight)
		{
			static float yaw = 0;
			static float pitch = 0;
			yaw -= 0.005*x;
			pitch -= 0.005*y;
			RR_CLAMP(pitch,-RR_PI*0.49f,-RR_PI*0.09f);
			lightDirection.x = sin(yaw);
			lightDirection.z = cos(yaw);
			lightDirection.y = pitch;
		}
		else
		{
			eye.setYawPitchRollRad(eye.getYawPitchRollRad()-rr::RRVec3(0.005*x,0.005*y,0));
		}
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

int main(int argc, char** argv)
{
	// log messages to console
	rr::RRReporter::createPrintfReporter();

	rr_io::registerLoaders();

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
#if defined(_WIN32)
	if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
#endif

	// init shaders
	water = new rr_gl::Water("../../data/shaders/",true,true);
	textureRenderer = new rr_gl::TextureRenderer("../../data/shaders/");
	
	// init textures
	rr_io::registerLoaders();
	environmentMap = new rr_gl::Texture(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"),false,false);

	glutMainLoop();
	return 0;
}
