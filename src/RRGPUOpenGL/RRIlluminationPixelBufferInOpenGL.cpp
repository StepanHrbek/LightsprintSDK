// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "RRIlluminationPixelBufferInOpenGL.h"
#include "Lightsprint/DemoEngine/Program.h"
#include "Lightsprint/RRGPUOpenGL.h"

#define SAFE_DELETE(a)       {delete a;a=NULL;}
#define SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}

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
		tempTexture = de::Texture::create(NULL,MAX_AMBIENT_MAP_WIDTH,MAX_AMBIENT_MAP_HEIGHT,false,GL_RGBA,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
		char buf1[1000],buf2[1000];
		_snprintf(buf1,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.vp");
		_snprintf(buf2,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.fp");
		filterProgram = de::Program::create(NULL,buf1,buf2);
		if(!filterProgram) rr::RRReporter::report(rr::RRReporter::ERRO,"Helper shaders failed: %s/lightmap_filter.*\n",pathToShaders);
		_snprintf(buf1,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.vp");
		_snprintf(buf2,999,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.fp");
		renderTriangleProgram = de::Program::create(NULL,buf1,buf2);
		if(!renderTriangleProgram) rr::RRReporter::report(rr::RRReporter::ERRO,"Helper shaders failed: %s/lightmap_build.*\n",pathToShaders);
	}
	~Helpers()
	{
		delete renderTriangleProgram;
		delete filterProgram;
		delete tempTexture;
	}
	de::Texture* tempTexture;
	de::Program* filterProgram;
	de::Program* renderTriangleProgram;
};

static Helpers* helpers = NULL;


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBufferInOpenGL

unsigned RRIlluminationPixelBufferInOpenGL::numInstances = 0;

RRIlluminationPixelBufferInOpenGL::RRIlluminationPixelBufferInOpenGL(const char* filename, unsigned awidth, unsigned aheight, const char* pathToShaders)
{
	rendering = false;

	if(!numInstances) helpers = new Helpers(pathToShaders);
	numInstances++;

	if(filename)
		texture = de::Texture::load(filename,NULL,false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
	else
		texture = de::Texture::create(NULL,awidth,aheight,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);

	renderedTexels = NULL;
}

void RRIlluminationPixelBufferInOpenGL::renderBegin()
{
	if(rendering) 
	{
		assert(0);
		return;
	}
	rendering = true;
	renderTriangleProgramSet = false;
	// backup pipeline
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	texture->renderingToBegin();

	glViewport(0,0,texture->getWidth(),texture->getHeight());
	// clear to black, alpha=0
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	// setup pipeline
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
}

void RRIlluminationPixelBufferInOpenGL::renderTriangle(const IlluminatedTriangle& it)
{
	assert(rendering);
	if(!rendering || !helpers->renderTriangleProgram) return;

	if(!renderTriangleProgramSet)
	{
		renderTriangleProgramSet = true;
		helpers->renderTriangleProgram->useIt();
		glDisable(GL_CULL_FACE);
	}
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
		//for(unsigned i=0;i<texture->getWidth()*texture->getHeight();i++) renderedTexels[i].color=255;//!!!
		numTexelsRenderedWithoutOverlap = 0;
		numTexelsRenderedWithOverlap = 0;
	}
	if(uv[0]>=texture->getWidth())
	{
		assert(0);
		return;
	}
	if(uv[1]>=texture->getHeight())
	{
		assert(0);
		return;
	}
	if(renderedTexels[uv[0]+uv[1]*texture->getWidth()]==rr::RRColorRGBA8())
		numTexelsRenderedWithoutOverlap++;
	else
		numTexelsRenderedWithOverlap++;
	renderedTexels[uv[0]+uv[1]*texture->getWidth()] = 
		rr::RRColorRGBA8(color[0],color[1],color[2],color[3]);
}

void RRIlluminationPixelBufferInOpenGL::renderEnd(bool preferQualityOverSpeed)
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

		if(numTexelsRenderedWithOverlap)
		{
			rr::RRReporter::report(
				(numTexelsRenderedWithOverlap>numTexelsRenderedWithoutOverlap/100)?rr::RRReporter::ERRO:rr::RRReporter::WARN,
				"Overlapping texels rendered into map, bad unwrap? size=%d*%d, ok=%d overlap=%d\n",
				texture->getWidth(),texture->getHeight(),numTexelsRenderedWithoutOverlap,numTexelsRenderedWithOverlap);
		}

		// normal way
		//texture->bindTexture();
		//glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture->getWidth(),texture->getHeight(),0,GL_RGBA,GL_UNSIGNED_BYTE,renderedTexels);
		//SAFE_DELETE_ARRAY(renderedTexels);

		// workaround for ATI (ati swaps channels at second glTexImage2D)
		unsigned w = getWidth();
		unsigned h = getHeight();
		delete texture;
		texture = de::Texture::create((unsigned char*)renderedTexels,w,h,false,GL_RGBA,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
		renderedTexels = NULL; // renderedTexels intentionally not deleted here, adopted by texture
//unsigned q[4]={0,0,0,0};
//for(unsigned i=0;i<getWidth()*getHeight();i++) for(unsigned j=0;j<4;j++) q[j]+=(renderedTexels[i].color>>(j*8))&255;
//printf("%d %d %d %d\n",q[0]/getWidth()/getHeight(),q[1]/getWidth()/getHeight(),q[2]/getWidth()/getHeight(),q[3]/getWidth()/getHeight());
//texture->save("c:/amb0.png",NULL);
	}

/*if(rendering)
{
	rendering = false;
	goto ende;
}else{
	// backup pipeline
	glGetIntegerv(GL_VIEWPORT,viewport);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	glGetFloatv(GL_COLOR_CLEAR_VALUE,clearcolor);

	glViewport(0,0,texture->getWidth(),texture->getHeight());
	// clear to 0
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	// setup pipeline
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}*/

	// although state was already set in renderBegin,
	// other code between renderBegin and renderEnd could change it, set again
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// tempTexture must not be smaller than texture
	if(helpers->tempTexture
		&& helpers->filterProgram
		&& texture->getWidth()<=helpers->tempTexture->getWidth() 
		&& texture->getHeight()<=helpers->tempTexture->getHeight()
		)
	for(int pass=0;pass<(preferQualityOverSpeed?10:2);pass++)
	{
		// fill unused pixels
		helpers->filterProgram->useIt();
		helpers->filterProgram->sendUniform("lightmap",0);
		helpers->filterProgram->sendUniform("pixelDistance",1.f/texture->getWidth(),1.f/texture->getHeight());

		helpers->tempTexture->renderingToBegin();
		if(pass==0 && preferQualityOverSpeed)
		{
			// at the beginning, clear helpers->tempTexture to prevent random colors leaking into texture
			glViewport(0,0,helpers->tempTexture->getWidth(),helpers->tempTexture->getHeight());
			glClearColor(0,0,0,0);
			glClear(GL_COLOR_BUFFER_BIT);
		}
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

//helpers->tempTexture->save("c:/amb1.png",NULL);

		texture->renderingToBegin();
		helpers->tempTexture->bindTexture();
		helpers->filterProgram->sendUniform("pixelDistance",1.f/helpers->tempTexture->getWidth(),1.f/helpers->tempTexture->getHeight());

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

//texture->save("c:/amb2.png",NULL);

		texture->renderingToEnd();
	}
	else
	{
		assert(0);
	}

//ende:
	// restore pipeline
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	glClearColor(clearcolor[0],clearcolor[1],clearcolor[2],clearcolor[3]);
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
	// badly restored state
	if(glUseProgram) glUseProgram(0);
	//glActiveTexture(?);
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

bool RRIlluminationPixelBufferInOpenGL::save(const char* filename)
{
	return texture->save(filename,NULL);
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
	return new RRIlluminationPixelBufferInOpenGL(NULL,w,h,pathToShaders);
}

rr::RRIlluminationPixelBuffer* RRRealtimeRadiosityGL::loadIlluminationPixelBuffer(const char* filename)
{
	RRIlluminationPixelBufferInOpenGL* illum = new RRIlluminationPixelBufferInOpenGL(filename,0,0,pathToShaders);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
