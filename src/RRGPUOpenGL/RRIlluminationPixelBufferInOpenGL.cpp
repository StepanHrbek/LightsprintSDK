// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "RRIlluminationPixelBufferInOpenGL.h"
#include "DemoEngine/Program.h"
#include "RRGPUOpenGL.h"

#define SAFE_DELETE_ARRAY(a)   {delete[] a;a=NULL;}

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// Helpers, one instance for all RRIlluminationPixelBufferInOpenGL

class Helpers
{
public:
	enum {
		MAX_AMBIENT_MAP_WIDTH = 2048, // for bigger ambient maps, filtering is turned off
		MAX_AMBIENT_MAP_HEIGHT = 2048,
	};
	Helpers(const char* pathToShaders)
	{
		tempTexture = de::Texture::create(NULL,MAX_AMBIENT_MAP_WIDTH,MAX_AMBIENT_MAP_HEIGHT,false,GL_RGBA,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
		char buf1[1000],buf2[1000];
		_snprintf(buf1,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.vp");
		_snprintf(buf2,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.fp");
		filterProgram = new de::Program(NULL,buf1,buf2);
	}
	~Helpers()
	{
		delete filterProgram;
		delete tempTexture;
	}
	de::Texture* tempTexture;
	de::Program* filterProgram;
};

static Helpers* helpers = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBufferInOpenGL

unsigned RRIlluminationPixelBufferInOpenGL::numInstances = 0;

RRIlluminationPixelBufferInOpenGL::RRIlluminationPixelBufferInOpenGL(unsigned awidth, unsigned aheight, const char* pathToShaders)
{
	rendering = false;

	if(!numInstances) helpers = new Helpers(pathToShaders);
	numInstances++;

	texture = de::Texture::create(NULL,awidth,aheight,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);

	renderedTexels = NULL;
}

void RRIlluminationPixelBufferInOpenGL::renderBegin()
{
	if(rendering) 
	{
		assert(!rendering);
		return;
	}
	rendering = true;
	// backup pipeline
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	texture->renderingToBegin();

	glViewport(0,0,texture->getWidth(),texture->getHeight());
	// clear to alpha=0 (color=pink, if we see it in scene, filtering or uv mapping is wrong)
	//glClearColor(1,0,1,0);
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	// setup pipeline
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	if(glUseProgram) glUseProgram(0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}

void RRIlluminationPixelBufferInOpenGL::renderTriangle(const IlluminatedTriangle& it)
{
	assert(rendering);
	if(!rendering) return;
	glBegin(GL_TRIANGLES);
	for(unsigned v=0;v<3;v++)
	{
		glColor3fv(&it.iv[v].measure[0]);
		glVertex2f(it.iv[v].texCoord[0]*2-1,it.iv[v].texCoord[1]*2-1);
	}
	glEnd();
}

//void RRIlluminationPixelBufferInOpenGL::renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles)
//{
//	assert(rendering);
//	if(!rendering) return;
//	write optimized version with interleaved array
//}

void RRIlluminationPixelBufferInOpenGL::renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color)
{
	if(!renderedTexels)
	{
		renderedTexels = new rr::RRColorRGBA8[texture->getWidth()*texture->getHeight()];
	}
	if(uv[0]>texture->getWidth())
	{
		assert(0);
		return;
	}
	if(uv[1]>texture->getHeight())
	{
		assert(0);
		return;
	}
	renderedTexels[uv[0]+uv[1]*texture->getWidth()] = 
		//!!! r <-> b swap, to compensate other swap on unknown place
		rr::RRColorRGBA8(color[2],color[1],color[0],color[3]);
}

void RRIlluminationPixelBufferInOpenGL::renderEnd()
{
	if(!rendering) 
	{
		assert(rendering);
		return;
	}
	rendering = false;

	if(renderedTexels)
	{
		texture->renderingToEnd();
		texture->bindTexture();
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture->getWidth(),texture->getHeight(),0,GL_RGBA,GL_UNSIGNED_BYTE,renderedTexels);
		SAFE_DELETE_ARRAY(renderedTexels);
	}

	// tempTexture must not be smaller than texture
	if(texture->getWidth()<=helpers->tempTexture->getWidth() && texture->getHeight()<=helpers->tempTexture->getHeight())
	for(unsigned pass=0;pass<1;pass++) //!!! with ~20 filtering passes, triangle borders degenerate, GPU precision problem?
	{
		// fill unused pixels
		helpers->filterProgram->useIt();
		helpers->filterProgram->sendUniform("lightmap",0);
		helpers->filterProgram->sendUniform("pixelDistance",1.0f/texture->getWidth(),1.0f/texture->getHeight());

		helpers->tempTexture->renderingToBegin();
		glViewport(0,0,texture->getWidth(),texture->getHeight());
		texture->bindTexture();

		glBegin(GL_POLYGON);
			glMultiTexCoord2f(0,0,0);
			glVertex2f(-1,-1);
			glMultiTexCoord2f(0,0,1);
			glVertex2f(-1,1);
			glMultiTexCoord2f(0,1,1);
			glVertex2f(1,1);
			glMultiTexCoord2f(0,1,0);
			glVertex2f(1,-1);
		glEnd();

		texture->renderingToBegin();
		helpers->tempTexture->bindTexture();
		helpers->filterProgram->sendUniform("pixelDistance",1.0f/helpers->tempTexture->getWidth(),1.0f/helpers->tempTexture->getHeight());

		float fracx = 1.0f*texture->getWidth()/helpers->tempTexture->getWidth();
		float fracy = 1.0f*texture->getHeight()/helpers->tempTexture->getHeight();
		glBegin(GL_POLYGON);
			glMultiTexCoord2f(0,0,0);
			glVertex2f(-1,-1);
			glMultiTexCoord2f(0,0,fracy);
			glVertex2f(-1,1);
			glMultiTexCoord2f(0,fracx,fracy);
			glVertex2f(1,1);
			glMultiTexCoord2f(0,fracx,0);
			glVertex2f(1,-1);
		glEnd();

		texture->renderingToEnd();
	}
	else
	{
		assert(0);
	}


	// restore pipeline
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
}

unsigned RRIlluminationPixelBufferInOpenGL::getWidth() const
{
	return texture->getWidth();
}

unsigned RRIlluminationPixelBufferInOpenGL::getHeight() const
{
	return texture->getHeight();
}

void RRIlluminationPixelBufferInOpenGL::bindTexture()
{
	texture->bindTexture();
}

RRIlluminationPixelBufferInOpenGL::~RRIlluminationPixelBufferInOpenGL()
{
	delete texture;
	delete[] renderedTexels;

	numInstances--;
	if(!numInstances)
	{
		delete helpers;
		helpers = NULL;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// RRGPUOpenGL

rr::RRIlluminationPixelBuffer* RRRealtimeRadiosityGL::createIlluminationPixelBuffer(unsigned w, unsigned h)
{
	return new RRIlluminationPixelBufferInOpenGL(w,h,pathToShaders);
}

} // namespace
