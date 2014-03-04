// --------------------------------------------------------------------------
// TextureRenderer, helper for rendering 2D or cube texture in OpenGL 2.0.
// Copyright (C) 2007-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRMemory.h"
#include "Lightsprint/GL/PreserveState.h"
#include "tmpstr.h"

#define PHYS2SRGB 0.45f
#define SRGB2PHYS 2.22222222f

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureRenderer

TextureRenderer::TextureRenderer(const rr::RRString& pathToShaders)
{
	skyProgram = UberProgram::create(rr::RRString(0,L"%lssky.vs",pathToShaders.w_str()),rr::RRString(0,L"%lssky.fs",pathToShaders.w_str()));
	twodProgram = UberProgram::create(rr::RRString(0,L"%lstexture.vs",pathToShaders.w_str()),rr::RRString(0,L"%lstexture.fs",pathToShaders.w_str()));
	if (!skyProgram)
		rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %lssky.*\n",pathToShaders.w_str());
	if (!twodProgram)
		rr::RRReporter::report(rr::ERRO,"Helper shaders failed: %lstexture.*\n",pathToShaders.w_str());
}

TextureRenderer::~TextureRenderer()
{
	delete skyProgram;
	delete twodProgram;
}

bool TextureRenderer::renderEnvironment(const rr::RRCamera& _camera, const Texture* _texture, float _angleRad, const rr::RRVec3& _brightness, float _gamma)
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
	bool equirectangular = _texture->getBuffer()->getType()==rr::BT_2D_TEXTURE;
	Program* program = skyProgram ? skyProgram->getProgram(tmpstr("#define POSTPROCESS_BRIGHTNESS\n%s%s",
		equirectangular?"#define PROJECTION_EQUIRECTANGULAR\n":"#define PROJECTION_CUBE\n",
		(_gamma!=1)?"#define POSTPROCESS_GAMMA\n":""
		)) : NULL;
	if (!program)
	{
		RR_ASSERT(0);
		return false;
	}
	program->useIt();
	_texture->bindTexture();
	program->sendUniform("map",0);
	program->sendUniform("postprocessBrightness",_brightness);
	if (_gamma!=1)
		program->sendUniform("postprocessGamma",_gamma);
	if (_texture->getBuffer()->getType()==rr::BT_2D_TEXTURE)
		program->sendUniform("shape",rr::RRVec4(-0.5f/RR_PI,1.0f/RR_PI,0.75f-_angleRad/(2*RR_PI),0.5f));

	// render
	rr::RRVec2 position[4] = {rr::RRVec2(-1,-1),rr::RRVec2(1,-1),rr::RRVec2(1,1),rr::RRVec2(-1,1)};
	rr::RRVec3 direction[4];
	rr::RRCamera camera = _camera;
	if (!equirectangular)
		camera.setYawPitchRollRad(camera.getYawPitchRollRad()+rr::RRVec3(_angleRad,0,0));
	for (unsigned i=0;i<4;i++)
		direction[i] = camera.getRayDirection(rr::RRVec2(position[i].x,-position[i].y));
	glEnableVertexAttribArray(VAA_POSITION);
	glVertexAttribPointer(VAA_POSITION, 2, GL_FLOAT, 0, 0, position);
	glEnableVertexAttribArray(VAA_NORMAL);
	glVertexAttribPointer(VAA_NORMAL, 3, GL_FLOAT, 0, 0, direction);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(VAA_NORMAL);
	glDisableVertexAttribArray(VAA_POSITION);

	return true;
}

bool TextureRenderer::renderEnvironment(const rr::RRCamera& _camera, const Texture* _texture0, float _angleRad0, const Texture* _texture1, float _angleRad1, float _blendFactor, const rr::RRVec4* _brightness, float _gamma, bool _allowDepthTest)
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

	// temporarily loads camera matrix with position 0
	rr::RRCamera camera = _camera;
	camera.setPosition(rr::RRVec3(0));

	// render
	bool result = renderEnvironment(camera,_texture0,_angleRad0,brightness*(1-_blendFactor),_gamma);
	if (result && _blendFactor)
	{
		// setup render states
		PreserveBlend p1;
		PreserveBlendFunc p2;
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		_texture1->bindTexture();

		// render
		result = renderEnvironment(camera,_texture1,_angleRad1,brightness*_blendFactor,_gamma);
	}

	return result;
};

bool TextureRenderer::render2dBegin(const rr::RRVec4* color, float gamma, const char* extraDefines, float domeFovDeg)
{
	Program* program = twodProgram ? twodProgram->getProgram(tmpstr("#define TEXTURE\n%s%s",(gamma!=1)?"#define GAMMA\n":"",extraDefines?extraDefines:"")) : NULL;
	if (!program)
	{
		RR_ASSERT(0);
		return false;
	}
	// render 2d
	program->useIt();
	glActiveTexture(GL_TEXTURE0);
	program->sendUniform("map",0);
	program->sendUniform("color",color?*color:rr::RRVec4(1));
	if (gamma!=1)
		program->sendUniform("gamma",gamma);
	if (extraDefines && strstr(extraDefines,"#define TEXTURE_IS_CUBE\n") && strstr(extraDefines,"#define CUBE_TO_DOME\n"))
		program->sendUniform("domeFovDeg",domeFovDeg);
	glEnableVertexAttribArray(VAA_POSITION);
	glEnableVertexAttribArray(VAA_UV_MATERIAL_DIFFUSE);
	return true;
}

void TextureRenderer::render2dQuad(const Texture* texture, float x,float y,float w,float h,float z)
{
	if (!texture)
	{
		RR_ASSERT(0);
		return;
	}
	texture->bindTexture();

#ifndef RR_GL_ES2
	if (texture->getBuffer()->getFormat()==rr::BF_DEPTH)
		// must be GL_NONE for sampler2D, otherwise result is undefined
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif

	rr::RRVec3 position[4] = {rr::RRVec3(2*x-1,2*y-1,RR_MAX(z,0)),rr::RRVec3(2*(x+w)-1,2*y-1,RR_MAX(z,0)),rr::RRVec3(2*(x+w)-1,2*(y+h)-1,RR_MAX(z,0)),rr::RRVec3(2*x-1,2*(y+h)-1,RR_MAX(z,0))};
	rr::RRVec2 uv[4] = {rr::RRVec2(0,0),rr::RRVec2(1,0),rr::RRVec2(1,1),rr::RRVec2(0,1)};
	glVertexAttribPointer(VAA_POSITION, 3, GL_FLOAT, 0, 0, position);
	glVertexAttribPointer(VAA_UV_MATERIAL_DIFFUSE, 2, GL_FLOAT, 0, 0, uv);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

#ifndef RR_GL_ES2
	if (texture->getBuffer()->getFormat()==rr::BF_DEPTH)
		// must be GL_COMPARE_REF_TO_TEXTURE for sampler2DShadow, otherwise result is undefined
		// we keep all depth textures ready for sampler2DShadow
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
#endif
}

void TextureRenderer::render2dEnd()
{
	glDisableVertexAttribArray(VAA_UV_MATERIAL_DIFFUSE);
	glDisableVertexAttribArray(VAA_POSITION);
}

void TextureRenderer::render2D(const Texture* texture, const rr::RRVec4* color, float gamma, float x,float y,float w,float h,float z, const char* extraDefines, float domeFovDeg)
{
	GLboolean depthTest;
	if (z<0)
	{
		depthTest = glIsEnabled(GL_DEPTH_TEST);
		glDisable(GL_DEPTH_TEST);
	}
	if (render2dBegin(color,gamma,tmpstr("%s%s",(texture && texture->getBuffer() && texture->getBuffer()->getType()==rr::BT_CUBE_TEXTURE)?"#define TEXTURE_IS_CUBE\n":"",extraDefines?extraDefines:""),domeFovDeg))
	{
		render2dQuad(texture,x,y,w,h,z);
		render2dEnd();
	}
	if (z<0)
	{
		if (depthTest) glEnable(GL_DEPTH_TEST);
	}
}

void TextureRenderer::renderQuad(const float* position)
{
	float fixedPosition[8] = {-1,-1, -1,1, 1,1, 1,-1};
	glEnableVertexAttribArray(VAA_POSITION);
	glVertexAttribPointer(VAA_POSITION, 2, GL_FLOAT, 0, 0, position?position:fixedPosition);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(VAA_POSITION);
}

}; // namespace
