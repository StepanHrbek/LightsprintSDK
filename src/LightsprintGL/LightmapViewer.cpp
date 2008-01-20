// --------------------------------------------------------------------------
// Viewer of lightmap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "LightmapViewer.h"
#include "Lightsprint/GL/UberProgram.h"
#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include <GL/glut.h>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// lightmap viewer

	static bool created = false; // only 1 instance is allowed
	static bool nearest;
	static bool alpha;
	static rr::RRReal zoom; // 1: 1 lmap pixel has 1 screen pixel, 2: 1 lmap pixel has 0.5x0.5 screen pixels
	static rr::RRVec2 center; // coord in pixels from center of lmap. texel with this coord is in center of screen
	static UberProgram* uberProgram;
	static Program* lmapProgram;
	static Program* lmapAlphaProgram;
	static Program* lineProgram;
	static rr::RRBuffer* buffer;
	static rr::RRMesh* mesh;

LightmapViewer* LightmapViewer::create(const char* _pathToShaders)
{
	return (!created) ? new LightmapViewer(_pathToShaders) : NULL;
}

LightmapViewer::LightmapViewer(const char* _pathToShaders)
{
	created = true;
	nearest = false;
	alpha = false;
	zoom = 1;
	center = rr::RRVec2(0);
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%stexture.vs",_pathToShaders);
	_snprintf(buf2,399,"%stexture.fs",_pathToShaders);
	uberProgram = UberProgram::create(buf1,buf2);
	lmapProgram = uberProgram->getProgram("#define TEXTURE\n");
	lmapAlphaProgram = uberProgram->getProgram("#define TEXTURE\n#define SHOW_ALPHA0\n");
	lineProgram = uberProgram->getProgram(NULL);
	buffer = NULL;
	mesh = NULL;
}

void LightmapViewer::setObject(rr::RRBuffer* _pixelBuffer, rr::RRMesh* _mesh, bool _bilinear)
{
	buffer = (_pixelBuffer && _pixelBuffer->getType()==rr::BT_2D_TEXTURE) ? _pixelBuffer : NULL;
	mesh = _mesh;
	if(buffer)
	{
		getTexture(buffer);
		glActiveTexture(GL_TEXTURE0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _bilinear?GL_LINEAR:GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

LightmapViewer::~LightmapViewer()
{
	created = false;
	SAFE_DELETE(uberProgram);
}

void LightmapViewer::mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		alpha = !alpha;
	}
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		zoom *= 0.625f;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		zoom *= 1.6f;
	}
}

void LightmapViewer::passive(int x, int y)
{
	unsigned winWidth = glutGet(GLUT_WINDOW_WIDTH);
	unsigned winHeight = glutGet(GLUT_WINDOW_HEIGHT);
	LIMITED_TIMES(1,glutWarpPointer(winWidth/2,winHeight/2);return;);
	x -= winWidth/2;
	y -= winHeight/2;
	if(x || y)
	{
		center[0] += 2*x/zoom;
		center[1] -= 2*y/zoom;
		glutWarpPointer(winWidth/2,winHeight/2);
	}
}

void LightmapViewer::display()
{
	if(!lmapProgram || !lmapAlphaProgram || !lineProgram)
	{
		RR_ASSERT(0);
		return;
	}

	unsigned winWidth = glutGet(GLUT_WINDOW_WIDTH);
	unsigned winHeight = glutGet(GLUT_WINDOW_HEIGHT);

	// clear
	glClear(GL_COLOR_BUFFER_BIT);

	// setup states
	glDisable(GL_DEPTH_TEST);

	unsigned bw = buffer ? buffer->getWidth() : 1;
	unsigned bh = buffer ? buffer->getHeight() : 1;
	float mult = MIN(winWidth/(float)bw,winHeight/float(bh))*0.9f;
	bw = (unsigned)(mult*bw);
	bh = (unsigned)(mult*bh);

	// render lightmap
	if(buffer)
	{
		float x = 0.5f + ( center[0] - bw*0.5f )*zoom/winWidth;
		float y = 0.5f + ( center[1] - bh*0.5f )*zoom/winHeight;
		float w = bw*zoom/winWidth;
		float h = bh*zoom/winHeight;
		Program* prg = alpha?lmapAlphaProgram:lmapProgram;
		prg->useIt();
		glActiveTexture(GL_TEXTURE0);
		getTexture(buffer)->bindTexture();
		prg->sendUniform("map",0);
		prg->sendUniform("color",1.0f,1.0f,1.0f,1.0f);
		glBegin(GL_POLYGON);
		glTexCoord2f(0,0);
		glVertex2f(2*x-1,2*y-1);
		glTexCoord2f(1,0);
		glVertex2f(2*(x+w)-1,2*y-1);
		glTexCoord2f(1,1);
		glVertex2f(2*(x+w)-1,2*(y+h)-1);
		glTexCoord2f(0,1);
		glVertex2f(2*x-1,2*(y+h)-1);
		glEnd();
	}

	// render mapping edges
	if(mesh)
	{
		lineProgram->useIt();
		lineProgram->sendUniform("color",1.0f,1.0f,1.0f,1.0f);
		glBegin(GL_LINES);
		for(unsigned i=0;i<mesh->getNumTriangles();i++)
		{
			rr::RRMesh::TriangleMapping mapping;
			mesh->getTriangleMapping(i,mapping);
			for(unsigned j=0;j<3;j++)
			{
				mapping.uv[j][0] = ( center[0]*2 + (mapping.uv[j][0]-0.5f)*2*bw )*zoom/winWidth;
				mapping.uv[j][1] = ( center[1]*2 + (mapping.uv[j][1]-0.5f)*2*bh )*zoom/winHeight;
			}
			for(unsigned j=0;j<3;j++)
			{
				glVertex2fv(&mapping.uv[j].x);
				glVertex2fv(&mapping.uv[(j+1)%3].x);
			}
		}
		glEnd(); // here Radeon X300/Catalyst2007.09 does random fullscreen effects for 5-10sec, X1650 is ok
	}

	// restore states
	glEnable(GL_DEPTH_TEST);
	glUseProgram(0); // prevents crashes in Radeon driver in AmbientOcclusion sample

	// show it
	glutSwapBuffers();
}

}; // namespace
