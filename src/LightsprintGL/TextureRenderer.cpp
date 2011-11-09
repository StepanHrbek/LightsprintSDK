// --------------------------------------------------------------------------
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <GL/glew.h>
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/RRDebug.h"
#include "PreserveState.h"
#include "tmpstr.h"

#define PHYS2SRGB 0.45f
#define SRGB2PHYS 2.22222222f

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer(const char* pathToShaders)
{
	for (unsigned projection=0;projection<2;projection++)
		for (unsigned scaled=0;scaled<2;scaled++)
		{
			skyProgram[projection][scaled] = Program::create(
				tmpstr("%s%s%s",
					(projection==0)?"#define PROJECTION_CUBE\n":(
					(projection==1)?"#define PROJECTION_EQUIRECTANGULAR\n":""),					
					"#define POSTPROCESS_BRIGHTNESS\n",
					scaled?"#define POSTPROCESS_GAMMA\n":""),
				tmpstr("%ssky.vs",pathToShaders),
				tmpstr("%ssky.fs",pathToShaders));
			if (!skyProgram[projection][scaled])
				rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %ssky.*\n",pathToShaders);
		}
	twodProgram = Program::create("#define TEXTURE\n",
		tmpstr("%stexture.vs",pathToShaders),
		tmpstr("%stexture.fs",pathToShaders));
	if (!twodProgram) rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %stexture.*\n",pathToShaders);
	oldCamera = NULL;
}

TextureRenderer::~TextureRenderer()
{
	for (unsigned projection=0;projection<2;projection++)
		for (unsigned scaled=0;scaled<2;scaled++)
			delete skyProgram[projection][scaled];
	delete twodProgram;
}

bool TextureRenderer::renderEnvironment(const Texture* _texture, const rr::RRVec3& _brightness, float _gamma)
{
	if (!_texture || !_texture->getBuffer())
	{
		RR_LIMITED_TIMES(1, rr::RRReporter::report(rr::WARN,"Rendering NULL environment.\n"));
		return false;
	}
	if (!_texture->getBuffer()->getScaled())
	{
		_brightness[0] = pow(_brightness[0],SRGB2PHYS);
		_brightness[1] = pow(_brightness[1],SRGB2PHYS);
		_brightness[2] = pow(_brightness[2],SRGB2PHYS);
		_gamma *= PHYS2SRGB;
	}
	unsigned projection = (_texture->getBuffer()->getType()==rr::BT_2D_TEXTURE)?1:0;
	unsigned scaled = (_gamma!=1)?1:0;
	Program* program = skyProgram[projection][scaled];
	if (!program)
	{
		RR_ASSERT(0);
		return false;
	}
	program->useIt();
	_texture->bindTexture();
	program->sendUniform("map",0);
	program->sendUniform("postprocessBrightness",_brightness);
	if (scaled)
		program->sendUniform("postprocessGamma",_gamma);
	if (projection==1)
		program->sendUniform("shape",rr::RRVec4(-0.5f/RR_PI,1.0f/RR_PI,0.75f,0.5f));

	// render
	glBegin(GL_POLYGON);
		glVertex3f(-1,-1,1);
		glVertex3f(1,-1,1);
		glVertex3f(1,1,1);
		glVertex3f(-1,1,1);
	glEnd();

	return true;
}

bool TextureRenderer::renderEnvironment(const Texture* _texture0, const Texture* _texture1, float _blendFactor, const rr::RRVec4* _brightness, float _gamma, bool _allowDepthTest)
{
	if (!_texture0 || !_texture0->getBuffer())
	{
		RR_LIMITED_TIMES(1, rr::RRReporter::report(rr::WARN,"Rendering NULL environment.\n"));
		return false;
	}
	rr::RRVec3 brightness = _brightness ? (rr::RRVec3)*_brightness : rr::RRVec3(1);

	// setup render states
	PreserveFlag p0(GL_CULL_FACE,false);
	PreserveDepthTest p1;
	PreserveDepthMask p2;
	if (!_allowDepthTest) glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glActiveTexture(GL_TEXTURE0);
	oldCamera = getRenderCamera();
	if (oldCamera)
	{
		// temporarily loads camera matrix with position 0
		Camera tmp = *oldCamera;
		tmp.setPosition(rr::RRVec3(0));
		setupForRender(tmp);
	}

	// render
	bool result = renderEnvironment(_texture0,brightness*(1-_blendFactor),_gamma);
	if (result && _blendFactor)
	{
		// setup render states
		PreserveBlend p1;
		PreserveBlendFunc p2;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		_texture1->bindTexture();

		// render
		result = renderEnvironment(_texture1,brightness*_blendFactor,_gamma);
	}

	// restore render states
	if (oldCamera)
	{
		setupForRender(*oldCamera);
	}
	return result;
};

bool TextureRenderer::render2dBegin(const rr::RRVec4* color)
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
	twodProgram->sendUniform("color",color?*color:rr::RRVec4(1));
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

void TextureRenderer::render2D(const Texture* texture, const rr::RRVec4* color, float x,float y,float w,float h)
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
