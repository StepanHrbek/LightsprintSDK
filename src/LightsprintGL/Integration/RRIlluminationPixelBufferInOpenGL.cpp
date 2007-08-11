// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "RRIlluminationPixelBufferInOpenGL.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/GL/RRDynamicSolverGL.h"

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
		tempTexture = Texture::create(NULL,MAX_AMBIENT_MAP_WIDTH,MAX_AMBIENT_MAP_HEIGHT,false,Texture::TF_RGBA,GL_NEAREST,GL_NEAREST,GL_CLAMP,GL_CLAMP);
		char buf1[400]; buf1[399] = 0;
		char buf2[400]; buf2[399] = 0;
		_snprintf(buf1,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.vs");
		_snprintf(buf2,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_filter.fs");
		filterProgram = Program::create(NULL,buf1,buf2);
		if(!filterProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %s/lightmap_filter.*\n",pathToShaders);
		_snprintf(buf1,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.vs");
		_snprintf(buf2,399,"%s%s",pathToShaders?pathToShaders:"","lightmap_build.fs");
		renderTriangleProgram = Program::create(NULL,buf1,buf2);
		if(!renderTriangleProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %s/lightmap_build.*\n",pathToShaders);
	}
	~Helpers()
	{
		delete renderTriangleProgram;
		delete filterProgram;
		delete tempTexture;
	}
	Texture* tempTexture;
	Program* filterProgram;
	Program* renderTriangleProgram;
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
		texture = Texture::load(filename,NULL,false,false,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);
	else
		texture = Texture::create(NULL,awidth,aheight,false,Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);

	renderedTexels = NULL;
	numRenderedTexels = 0;
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

void RRIlluminationPixelBufferInOpenGL::renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color)
{
	if(!rendering) 
	{
		return; // used by updateTriangleLightmap
	}
	if(!renderedTexels)
	{
		renderedTexels = new rr::RRColorRGBAF[texture->getWidth()*texture->getHeight()];
		numRenderedTexels = 0;
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
	numRenderedTexels++;
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
		if(numRenderedTexels<=1)
		{
			if(getWidth()*getHeight()<=64*64 || getWidth()<=16 || getHeight()<=16)
				rr::RRReporter::report(rr::WARN,"No texels rendered into map, bad unwrap (see RRMesh::getTriangleMapping) or low resolution(%dx%d)?\n",getWidth(),getHeight());
			else
				rr::RRReporter::report(rr::WARN,"No texels rendered into map, bad unwrap (see RRMesh::getTriangleMapping)?\n");
			return;
		}

		texture->renderingToEnd();

		#pragma omp parallel for schedule(static)
		for(int i=0;i<(int)(texture->getWidth()*texture->getHeight());i++)
		{
			if(renderedTexels[i].w)
			{
				renderedTexels[i] /= renderedTexels[i][3];
			}
		}

		// normal way (internally stored in halfs, can't be interpolated on old cards)
		//texture->reset(texture->getWidth(),texture->getHeight(),Texture::TF_RGBAF,(unsigned char*)renderedTexels,false);

		// normal way (internally stored in bytes)
		texture->bindTexture();
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture->getWidth(),texture->getHeight(),0,GL_RGBA,GL_FLOAT,renderedTexels);

		SAFE_DELETE_ARRAY(renderedTexels);
		numRenderedTexels = 0;
	}

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
	for(int pass=0;pass<(preferQualityOverSpeed?2:2);pass++)
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

		texture->renderingToEnd();
	}
	else
	{
		RR_ASSERT(0);
	}

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
// RRDynamicSolverGL

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
