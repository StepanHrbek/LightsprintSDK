#include <assert.h>
#include <stdio.h>
#include <GL/glut.h>
#include "videogl.h"

unsigned video_XRES, video_YRES, video_Bits;
float video_XFOV, video_YFOV;
bool video_inited=false;

extern bool video_Init(int xres, int yres)
{
	assert(!video_inited);
	if(video_inited) return true;

	glutInitWindowSize(xres, yres);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH|GLUT_STENCIL); // +stencil makes use of depth24 instead of depth16

	if(!(glutCreateWindow("ReeltimeRadiosity"))) return false;
	//  glutFullScreen();

	video_XRES = xres;
	video_YRES = yres;
	video_XFOV = 64;
	video_YFOV = 48;

	video_inited=true;

	return true;
}

extern void video_Clear(float rgb[3])
{
	if(!video_inited) return;

	glClearColor(rgb[0], rgb[1], rgb[2], 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

static void printGlutBitmapFont(char *string, void *font, int x, int y, float r, float g, float b)
{
	glColor3f(r, g, b);
	glRasterPos2i(x, y);
	glDisable(GL_DEPTH_TEST);
	while (*string)
		glutBitmapCharacter(font, *string++);
	glEnable(GL_DEPTH_TEST);
}

extern void video_WriteBuf(char *text)
{
	if(!video_inited)
		printf("%s\n",text);
	else
	{
		glDisable(GL_LIGHTING);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		int w=glutGet(GLUT_WINDOW_WIDTH);
		int h=glutGet(GLUT_WINDOW_HEIGHT);
		gluOrtho2D(0, w,h, 0);
		printGlutBitmapFont(text,GLUT_BITMAP_HELVETICA_18,1,21,0,0,0);
		printGlutBitmapFont(text,GLUT_BITMAP_HELVETICA_18,0,20,1,1,1);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
}

extern void video_WriteScreen(char *text)
{
	video_WriteBuf(text);
}

extern void video_Blit()
{
	if(!video_inited) return;
	glutSwapBuffers();
}

extern void video_Grab(char *name)
{
}

extern void video_Done()
{
	if(!video_inited) return;
	video_inited=false;
}

void video_PutPixel(unsigned x,unsigned y,unsigned color)
{
}

unsigned video_GetPixel(unsigned x,unsigned y)
{
	return 0;
}
