// --------------------------------------------------------------------------
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/RRDebug.h"
#include "tmpstr.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer(const char* pathToShaders)
{
	skyScaledProgram = Program::create("#define POSTPROCESS_BRIGHTNESS\n",
		tmpstr("%ssky.vs",pathToShaders),
		tmpstr("%ssky.fs",pathToShaders));
	if (!skyScaledProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %ssky.*\n",pathToShaders);
	skyPhysicalProgram = Program::create("#define POSTPROCESS_BRIGHTNESS\n#define POSTPROCESS_GAMMA\n",
		tmpstr("%ssky.vs",pathToShaders),
		tmpstr("%ssky.fs",pathToShaders));
	if (!skyPhysicalProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %ssky.*\n",pathToShaders);
	twodProgram = Program::create("#define TEXTURE\n",
		tmpstr("%stexture.vs",pathToShaders),
		tmpstr("%stexture.fs",pathToShaders));
	if (!twodProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %stexture.*\n",pathToShaders);
	oldCamera = NULL;
}

TextureRenderer::~TextureRenderer()
{
	delete twodProgram;
	delete skyScaledProgram;
	delete skyPhysicalProgram;
}

bool TextureRenderer::renderEnvironmentBegin(const rr::RRVec4* _color, bool _allowDepthTest, bool _physical, float _gamma)
{
	if (_physical) _gamma *= 0.45f;
	Program* program = (_gamma!=1) ? skyPhysicalProgram : skyScaledProgram;
	if (!program)
	{
		RR_ASSERT(0);
		return false;
	}
	// backup render states
	culling = glIsEnabled(GL_CULL_FACE);
	depthTest = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
	// setup render states
	if (!_allowDepthTest) glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	// render cube
	program->useIt();
	glActiveTexture(GL_TEXTURE0);
	program->sendUniform("cube",0);

	rr::RRVec4 correctedBrightness = _color?(*_color)*pow(_gamma,0.45f):rr::RRVec4(1.0f);
	program->sendUniform4fv("postprocessBrightness",&correctedBrightness.x);
	if (_gamma!=1)
		program->sendUniform("postprocessGamma",_gamma);
	oldCamera = Camera::getRenderCamera();
	if (oldCamera)
	{
		// temporarily loads camera matrix with position 0
		Camera tmp = *oldCamera;
		tmp.pos = rr::RRVec3(0);
		tmp.update();
		tmp.setupForRender();
	}
	return true;
}

void TextureRenderer::renderEnvironmentEnd()
{
	// restore render states
	if (depthTest) glEnable(GL_DEPTH_TEST);
	if (depthMask) glDepthMask(GL_TRUE);
	if (culling) glEnable(GL_CULL_FACE);
	if (oldCamera)
	{
		oldCamera->setupForRender();
	}
}

bool TextureRenderer::renderEnvironment(const Texture* texture,const rr::RRVec4* color,float gamma)
{
	if (!texture)
	{
		RR_LIMITED_TIMES(1, rr::RRReporter::report(rr::WARN,"Rendering NULL environment.\n"));
		return false;
	}
	if (renderEnvironmentBegin(color,false,!texture->getBuffer()->getScaled(),gamma))
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
	if (!twodProgram)
	{
		RR_ASSERT(0);
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
	if (!texture)
	{
		RR_ASSERT(0);
		return;
	}
	texture->bindTexture();
	glBegin(GL_POLYGON);
		glMultiTexCoord2f(GL_TEXTURE0,0,0);
		glVertex2f(2*x-1,2*y-1);
		glMultiTexCoord2f(GL_TEXTURE0,1,0);
		glVertex2f(2*(x+w)-1,2*y-1);
		glMultiTexCoord2f(GL_TEXTURE0,1,1);
		glVertex2f(2*(x+w)-1,2*(y+h)-1);
		glMultiTexCoord2f(GL_TEXTURE0,0,1);
		glVertex2f(2*x-1,2*(y+h)-1);
	glEnd();
}

void TextureRenderer::render2dEnd()
{
	if (depthTest) glEnable(GL_DEPTH_TEST);
	if (depthMask) glDepthMask(GL_TRUE);
	if (culling) glEnable(GL_CULL_FACE);
}

void TextureRenderer::render2D(const Texture* texture,float color[4], float x,float y,float w,float h)
{
	if (render2dBegin(color))
	{
		render2dQuad(texture,x,y,w,h);
		render2dEnd();
	}
	/*
	if (!texture || !twodProgram)
	{
		RR_ASSERT(0);
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
	if (depthTest) glEnable(GL_DEPTH_TEST);
	if (depthMask) glDepthMask(GL_TRUE);
	if (culling) glEnable(GL_CULL_FACE);*/
}

}; // namespace
