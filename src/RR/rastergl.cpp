#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <GL/glut.h>
#include "rastergl.h"

void raster_Init(int xres, int yres)
{
	assert(sizeof(GLfloat)==sizeof(real));

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	//  glClearDepth(1000);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	//  glFrontFace(GL_CW);
	//  glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
}

void raster_Clear()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

void raster_SetFOV(float xfov, float yfov)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(xfov/90,-xfov/90,-xfov/90,xfov/90,2,10000);
}

void raster_SetMatrix( raster_MATRIX *cam, raster_MATRIX *inv)
{
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf( (GLfloat *) *cam);
}

void raster_SetMatrix(Point3 &cam,Vec3 &dir)
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(cam.x,cam.y,cam.z,cam.x+dir.x,cam.y+dir.y,cam.z+dir.z,0,1,0.001234);
}

void raster_BeginTriangles()
{
//  glShadeModel(GL_SMOOTH);
//  glBegin(GL_TRIANGLES);
}

void raster_ZFlat(Point3 *p, unsigned *col, real brightness)
{
	glShadeModel(GL_FLAT);
	int i;

	#define SETCOL(brightness) i=(int)(brightness*256);i=col[(i<0)?0:((i>255)?255:i)]; glColor3f(((i&0xff0000)>>16)*brightness/255, ((i&0xff00)>>8)*brightness/255, (i&0xff)*brightness/255);
	SETCOL(brightness);

	glBegin(GL_TRIANGLES);
	glVertex3fv( (GLfloat *) &p[0]);
	glVertex3fv( (GLfloat *) &p[1]);
	glVertex3fv( (GLfloat *) &p[2]);
	glEnd();
}

void raster_ZGouraud(Point3 *p, unsigned *col, real brightness[3])
{
	glShadeModel(GL_SMOOTH);
	int i;

	glBegin(GL_TRIANGLES);
	SETCOL(brightness[0]);
	glVertex3fv( (GLfloat *)&p[0]);
	SETCOL(brightness[1]);
	glVertex3fv( (GLfloat *)&p[1]);
	SETCOL(brightness[2]);
	glVertex3fv( (GLfloat *)&p[2]);
	glEnd();
}

void raster_EndTriangles()
{
//  glEnd();
}

//////////////////////////////////////////////////////////////////////////
//
// Occlusion queries
/*
OcclusionQueries::OcclusionQueries(int amax)
{
	qmax=amax;
	queries=new GLuint[qmax];
	samples=new GLuint[qmax];
	glGenQueriesARB(qmax, queries);
}

OcclusionQueries::~OcclusionQueries()
{
	glDeleteQueriesARB(n, queries);
	delete[] queries;
	delete[] samples;
}

void OcclusionQueries::Reset(bool newFrame)
{
	qnow=0;
	if(newFrame) 
	{
		for(int i=0;i<qmax;i++) samples[i]=65535;
		glClear(GL_DEPTH_BUFFER_BIT);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
	} else {
		glDepthFunc(GL_EQUAL);
		glDepthMask(GL_FALSE);
	}
}

void OcclusionQueries::Begin()
{
	assert(qnow<qmax);
	glBeginQueryARB(GL_SAMPLES_PASSED_ARB, queries[qnow]);
}

void OcclusionQueries::End()
{
	glEndQueryARB(GL_SAMPLES_PASSED_ARB);
	qnow++;
}

int OcclusionQueries::Samples()
{
	assert(qnow<qmax);
	glGetQueryObjectuivARB(queries[qnow], GL_QUERY_RESULT_ARB, &samples[qnow]);
	qnow++;
}

OcclusionQueries queries;
*/
