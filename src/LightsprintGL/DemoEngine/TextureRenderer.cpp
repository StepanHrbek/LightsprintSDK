// --------------------------------------------------------------------------
// DemoEngine
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer(const char* pathToShaders)
{
	char buf1[400]; buf1[399] = 0;
	char buf2[400]; buf2[399] = 0;
	_snprintf(buf1,399,"%ssky.vs",pathToShaders);
	_snprintf(buf2,399,"%ssky.fs",pathToShaders);
	skyProgram = Program::create(NULL,buf1,buf2);
	if(!skyProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %ssky.*\n",pathToShaders);
	_snprintf(buf1,399,"%stexture.vs",pathToShaders);
	_snprintf(buf2,399,"%stexture.fs",pathToShaders);
	twodProgram = Program::create("#define TEXTURE\n",buf1,buf2);
	if(!twodProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %stexture.*\n",pathToShaders);
}

TextureRenderer::~TextureRenderer()
{
	delete twodProgram;
	delete skyProgram;
}

bool TextureRenderer::renderEnvironmentBegin(float color[4])
{
	if(!skyProgram)
	{
		assert(0);
		return false;
	}
	// backup render states
	culling = glIsEnabled(GL_CULL_FACE);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	// setup render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	// render cube
	skyProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	skyProgram->sendUniform("cube",0);
	skyProgram->sendUniform("color",color?color[0]:1,color?color[1]:1,color?color[2]:1,color?color[3]:1);
	//	skyProgram->sendUniform("worldEyePos",worldEyePos[0],worldEyePos[1],worldEyePos[2]);
	return true;
}

void TextureRenderer::renderEnvironmentEnd()
{
	// restore render states
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
	if(culling) glEnable(GL_CULL_FACE);
}

bool TextureRenderer::renderEnvironment(const Texture* texture,float color[4])
{
	if(!texture)
	{
		assert(0);
		return false;
	}
	if(renderEnvironmentBegin(color))
	{
		texture->bindTexture();
		glBegin(GL_POLYGON);
			glVertex3f(-1,-1,1);
			glVertex3f(1,-1,1);
			glVertex3f(1,1,1);
			glVertex3f(-1,1,1);
		glEnd();
		renderEnvironmentEnd();
		return true;
	}
	return false;
};

bool TextureRenderer::render2dBegin(float color[4])
{
	// backup render states
	culling = glIsEnabled(GL_CULL_FACE);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	if(!twodProgram)
	{
		assert(0);
		return false;
	}
	// setup render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	// render 2d
	twodProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	twodProgram->sendUniform("map",0);
	twodProgram->sendUniform("color",color?color[0]:1,color?color[1]:1,color?color[2]:1,color?color[3]:1);
	return true;
}

void TextureRenderer::render2dQuad(const Texture* texture, float x,float y,float w,float h)
{
	if(!texture)
	{
		assert(0);
		return;
	}
	texture->bindTexture();
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

void TextureRenderer::render2dEnd()
{
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
	if(culling) glEnable(GL_CULL_FACE);
}

void TextureRenderer::render2D(const Texture* texture,float color[4], float x,float y,float w,float h)
{
	if(render2dBegin(color))
	{
		render2dQuad(texture,x,y,w,h);
		render2dEnd();
	}
	/*
	if(!texture || !twodProgram)
	{
		assert(0);
		return;
	}
	// backup render states
	culling = glIsEnabled(GL_CULL_FACE);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	// setup render states
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	// render 2d
	twodProgram->useIt();
	glActiveTexture(GL_TEXTURE0);
	texture->bindTexture();
	twodProgram->sendUniform("map",0);
	twodProgram->sendUniform("color",color?color[0]:1,color?color[1]:1,color?color[2]:1,color?color[3]:1);
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
	// restore render states
	if(depthTest) glEnable(GL_DEPTH_TEST);
	if(depthMask) glDepthMask(GL_TRUE);
	if(culling) glEnable(GL_CULL_FACE);*/
}

}; // namespace
