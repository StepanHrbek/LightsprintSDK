// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "RRIlluminationPixelBufferInOpenGL.h"
#include "Lightsprint/DemoEngine/Program.h"
#include "Lightsprint/RRGPUOpenGL.h"

//#define DIAGNOSTIC // kazdy texel dostane barvu podle toho kolikrat do nej bylo zapsano
//#define DIAGNOSTIC_RED_UNRELIABLE // ukaze nespolehlivy texely cervene

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
		tempTexture = de::Texture::create(NULL,MAX_AMBIENT_MAP_WIDTH,MAX_AMBIENT_MAP_HEIGHT,false,de::Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
		char buf1[400]; buf1[399] = 0;
		char buf2[400]; buf2[399] = 0;
		_snprintf(buf1,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.vs");
		_snprintf(buf2,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.fs");
		filterProgram = de::Program::create(NULL,buf1,buf2);
		if(!filterProgram) rr::RRReporter::report(rr::RRReporter::ERRO,"Helper shaders failed: %s/lightmap_filter.*\n",pathToShaders);
		_snprintf(buf1,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.vs");
		_snprintf(buf2,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.fs");
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

	if(!helpers) helpers = new Helpers(pathToShaders);
	numInstances++;

	if(filename)
		texture = de::Texture::load(filename,NULL,false,false,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);
	else
		texture = de::Texture::create(NULL,awidth,aheight,false,de::Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);

	renderedTexels = NULL;
}

void RRIlluminationPixelBufferInOpenGL::renderBegin()
{
	if(rendering) 
	{
		RR_ASSERT(0);
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
	RR_ASSERT(rendering);
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
//	RR_ASSERT(rendering);
//	if(!rendering) return;
//	write optimized version with interleaved array
//}

void RRIlluminationPixelBufferInOpenGL::renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color)
{
	if(!rendering) 
	{
		return; // used by updateTriangleLightmap
	}
	if(!renderedTexels)
	{
		renderedTexels = new rr::RRColorRGBAF[texture->getWidth()*texture->getHeight()];
	}
	if(uv[0]>=texture->getWidth())
	{
		RR_ASSERT(0);
		return;
	}
	if(uv[1]>=texture->getHeight())
	{
		RR_ASSERT(0);
		return;
	}

	renderedTexels[uv[0]+uv[1]*texture->getWidth()] += color;
	renderedTexels[uv[0]+uv[1]*texture->getWidth()][3] += color[3]; //missing vec4 operators
}

void RRIlluminationPixelBufferInOpenGL::renderEnd(bool preferQualityOverSpeed)
{
	if(!rendering) 
	{
		RR_ASSERT(rendering);
		return;
	}
	rendering = false;

	if(renderedTexels)
	{
		texture->renderingToEnd();

		#pragma omp parallel for schedule(static)
		for(int i=0;i<(int)(texture->getWidth()*texture->getHeight());i++)
		{
			if(renderedTexels[i].w)
			{
				renderedTexels[i] /= renderedTexels[i][3];
				renderedTexels[i][3] = 1; //missing vec4 operators
			}
#ifdef DIAGNOSTIC_RED_UNRELIABLE
			else
			{
				renderedTexels[i] = rr::RRColorRGBAF(1,0,0,0);//!!!
			}
#endif
		}

#ifdef DIAGNOSTIC
		// convert to visible colors
		unsigned diagColors[] =
		{
			0,0xff0000ff,0xff00ff00,0xffff0000,0xffffff00,0xffff00ff,0xff00ffff,0xffffffff
		};
		for(unsigned i=0;i<texture->getWidth()*texture->getHeight();i++)
		{
			renderedTexels[i].color = diagColors[CLAMPED(renderedTexels[i].color&255,0,7)];
		}
#endif

		// normal way
		//texture->bindTexture();
		//glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture->getWidth(),texture->getHeight(),0,GL_RGBA,GL_FLOAT,renderedTexels);

		// workaround for ATI (ati swaps channels at second glTexImage2D)
		unsigned w = getWidth();
		unsigned h = getHeight();
		delete texture;
		texture = de::Texture::create((unsigned char*)renderedTexels,w,h,false,de::Texture::TF_RGBAF,GL_LINEAR,GL_LINEAR,GL_CLAMP,GL_CLAMP);
//unsigned q[4]={0,0,0,0};
//for(unsigned i=0;i<getWidth()*getHeight();i++) for(unsigned j=0;j<4;j++) q[j]+=(renderedTexels[i].color>>(j*8))&255;
//printf("%d %d %d %d\n",q[0]/getWidth()/getHeight(),q[1]/getWidth()/getHeight(),q[2]/getWidth()/getHeight(),q[3]/getWidth()/getHeight());
//texture->save("c:/amb0.png",NULL);

		SAFE_DELETE_ARRAY(renderedTexels);
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
	for(int pass=0;pass<(preferQualityOverSpeed?
#ifdef DIAGNOSTIC_RED_UNRELIABLE
		0
#else
		2
#endif
		:2);pass++)
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
		RR_ASSERT(0);
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

void RRIlluminationPixelBufferInOpenGL::bindTexture() const
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
}

void RRDynamicSolverGL::cleanup()
{
	if(!RRIlluminationPixelBufferInOpenGL::numInstances)
	{
		SAFE_DELETE(helpers);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// RRGPUOpenGL

rr::RRIlluminationPixelBuffer* RRDynamicSolverGL::createIlluminationPixelBuffer(unsigned w, unsigned h)
{
	return new RRIlluminationPixelBufferInOpenGL(NULL,w,h,pathToShaders);
}

rr::RRIlluminationPixelBuffer* RRDynamicSolverGL::loadIlluminationPixelBuffer(const char* filename)
{
	RRIlluminationPixelBufferInOpenGL* illum = new RRIlluminationPixelBufferInOpenGL(filename,0,0,pathToShaders);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
