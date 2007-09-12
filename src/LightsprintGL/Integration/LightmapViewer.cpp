// --------------------------------------------------------------------------
// Viewer of lightmap.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#include "Lightsprint/GL/LightmapViewer.h"
#include "Lightsprint/GL/UberProgram.h"
#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include <GL/glut.h>

namespace rr_gl
{

	
/////////////////////////////////////////////////////////////////////////////
//
// helper adapter

class TextureFromPixelBuffer : public Texture
{
public:
	TextureFromPixelBuffer(rr::RRIlluminationPixelBuffer* _pixelBuffer)
	{
		pixelBuffer = _pixelBuffer;
	}
	virtual void bindTexture() const
	{
		pixelBuffer->bindTexture();
	}
	virtual bool reset(unsigned width, unsigned height, Format format, const unsigned char* data, bool buildMipmaps) {return 0;}
	virtual const unsigned char* lock() {return NULL;}
	virtual void unlock() {};

	virtual unsigned getWidth() const {return pixelBuffer->getWidth();}
	virtual unsigned getHeight() const {return pixelBuffer->getHeight();}
	virtual Format getFormat() const {return TF_RGBAF;}
	virtual bool isCube() const {return false;}

	virtual bool renderingToBegin(unsigned side = 0) {return 0;}
	virtual void renderingToEnd() {}
private:
	rr::RRIlluminationPixelBuffer* pixelBuffer;
};


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
	static Texture* lightmap;
	static rr::RRMesh* mesh;

LightmapViewer* LightmapViewer::create(Texture* _lightmap, rr::RRMesh* _mesh, const char* _pathToShaders)
{
	return (!created && _lightmap) ? new LightmapViewer(_lightmap,_mesh,_pathToShaders) : NULL;
}

LightmapViewer* LightmapViewer::create(rr::RRIlluminationPixelBuffer* _pixelBuffer, rr::RRMesh* _mesh, const char* _pathToShaders)
{
	return (!created && _pixelBuffer) ? new LightmapViewer(new TextureFromPixelBuffer(_pixelBuffer),_mesh,_pathToShaders) : NULL;
}

LightmapViewer::LightmapViewer(Texture* _lightmap, rr::RRMesh* _mesh, const char* _pathToShaders)
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
	lightmap = _lightmap;
	mesh = _mesh;
}

LightmapViewer::~LightmapViewer()
{
	created = false;
	SAFE_DELETE(lineProgram);
	SAFE_DELETE(lmapProgram);
	SAFE_DELETE(lmapAlphaProgram);
}

void LightmapViewer::mouse(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		if(lightmap)
		{	
			lightmap->bindTexture();
			nearest = !nearest;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearest?GL_NEAREST:GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest?GL_NEAREST:GL_LINEAR);
		}
	}
	if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
		alpha = !alpha;
	}
	if(button == GLUT_WHEEL_UP && state == GLUT_UP)
	{
		zoom *= 0.5f;
	}
	if(button == GLUT_WHEEL_DOWN && state == GLUT_UP)
	{
		zoom *= 2.0f;
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
	if(!(lightmap && lmapProgram && lmapAlphaProgram && lineProgram))
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

	// render lightmap
	float x = 0.5f + ( center[0] - lightmap->getWidth ()*0.5f )*zoom/winWidth;
	float y = 0.5f + ( center[1] - lightmap->getHeight()*0.5f )*zoom/winHeight;
	float w = lightmap->getWidth ()*zoom/winWidth;
	float h = lightmap->getHeight()*zoom/winHeight;
	Program* prg = alpha?lmapAlphaProgram:lmapProgram;
	prg->useIt();
	glActiveTexture(GL_TEXTURE0);
	lightmap->bindTexture();
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

	// render mapping edges
	lineProgram->useIt();
	lineProgram->sendUniform("color",1.0f,1.0f,1.0f,1.0f);
	glBegin(GL_LINES);
	for(unsigned i=0;i<mesh->getNumTriangles();i++)
	{
		rr::RRMesh::TriangleMapping mapping;
		mesh->getTriangleMapping(i,mapping);
		for(unsigned j=0;j<3;j++)
		{
			mapping.uv[j][0] = ( center[0]*2 + (mapping.uv[j][0]-0.5f)*2*lightmap->getWidth () )*zoom/winWidth;
			mapping.uv[j][1] = ( center[1]*2 + (mapping.uv[j][1]-0.5f)*2*lightmap->getHeight() )*zoom/winHeight;
		}
		for(unsigned j=0;j<3;j++)
		{
			glVertex2fv(&mapping.uv[j].x);
			glVertex2fv(&mapping.uv[(j+1)%3].x);
		}
	}
	glEnd(); // here Radeon X300/Catalyst2007.09 does random fullscreen effects for 5-10sec, X1650 is ok

	// restore states
	glEnable(GL_DEPTH_TEST);
	glUseProgram(0); // prevents crashes in Radeon driver in AmbientOcclusion sample

	// show it
	glutSwapBuffers();
}

}; // namespace
