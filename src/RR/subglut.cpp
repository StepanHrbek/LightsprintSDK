#include <memory.h>    // NULL
#include "misc.h"
#include "subglut.h"

void (*displayFunc)(void);
void (*reshapeFunc)(int width, int height);
void (*keyboardFunc)(unsigned char key, int x, int y);
void (*mouseFunc)(int button, int state, int x, int y);
void (*motionFunc)(int x, int y);
void (*passiveMotionFunc)(int x, int y);
void (*entryFunc)(int state);
void (*visibilityFunc)(int state);
void (*idleFunc)(void);
void (*specialFunc)(int key,int x,int y);

void glutInit(int *argcp, char **argv)
{
	displayFunc=NULL;
	reshapeFunc=NULL;
	keyboardFunc=NULL;
	mouseFunc=NULL;
	motionFunc=NULL;
	passiveMotionFunc=NULL;
	entryFunc=NULL;
	visibilityFunc=NULL;
	idleFunc=NULL;
	specialFunc=NULL;
	mouse_init();
}

void glutMainLoop(void)
{
	while(1)
	{
		displayFunc();
//                while(!kb_hit())
//                        if(idleFunc) idleFunc();
		while(kb_hit())
		{
			unsigned char ch=kb_get();
			if(!ch) {if(specialFunc) specialFunc(kb_get(),0,0);}
			else if(keyboardFunc) keyboardFunc(ch,0,0);
		}
		int x,y,z,b;
		if(mouse_get(x,y,z,b))
		{
			if(mouseFunc) mouseFunc(1,b&1,x,y);
		}
	}
}

void glutDisplayFunc(void (*func)(void))
{
	displayFunc=func;
}

void glutReshapeFunc(void (*func)(int width, int height))
{
	reshapeFunc=func;
}

void glutKeyboardFunc(void (*func)(unsigned char key, int x, int y))
{
	keyboardFunc=func;
}

void glutMouseFunc(void (*func)(int button, int state, int x, int y))
{
	mouseFunc=func;
}

void glutMotionFunc(void (*func)(int x, int y))
{
	motionFunc=func;
}

void glutPassiveMotionFunc(void (*func)(int x, int y))
{
	passiveMotionFunc=func;
}

void glutEntryFunc(void (*func)(int state))
{
	entryFunc=func;
}

void glutVisibilityFunc(void (*func)(int state))
{
	visibilityFunc=func;
}

void glutIdleFunc(void (*func)(void))
{
	idleFunc=func;
}

void glutSpecialFunc(void (*func)(int key, int x, int y))
{
	specialFunc=func;
}

