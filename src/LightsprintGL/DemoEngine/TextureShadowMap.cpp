// --------------------------------------------------------------------------
// DemoEngine
// TextureShadowMap, Texture that can hold shadowmap, uses FBO when available.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include "TextureShadowmap.h"
#include "FBO.h"

namespace rr_gl
{

// AMD doesn't work properly with GL_LINEAR on shadowmaps, it needs GL_NEAREST
static GLenum filtering()
{
	static GLenum mode = GL_LINEAR;
	static bool inited = 0;
	if(!inited)
	{
		char* renderer = (char*)glGetString(GL_RENDERER);
		if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")||strstr(renderer,"FireGL"))) mode = GL_NEAREST;
		inited = 1;
	}
	return mode;
}

/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

TextureShadowMap::TextureShadowMap(unsigned _width, unsigned _height)
	: TextureGL(NULL, _width, _height, false, TF_NONE, filtering(), filtering(), GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
{
	// for shadow2D() instead of texture2D()
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
}

bool TextureShadowMap::renderingToBegin(unsigned side)
{
	if(!globalFBO) globalFBO = new FBO();
	globalFBO->setRenderTargetDepth(id);

	//GLint depthBits;
	//glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	//printf("FBO depth precision = %d\n", depthBits);

	return globalFBO->isStatusOk();
}

void TextureShadowMap::renderingToEnd()
{
	globalFBO->setRenderTargetDepth(0);
	globalFBO->restoreDefaultRenderTarget();
}

unsigned TextureShadowMap::getTexelBits()
{
	GLint bits = 0;
	bindTexture();
	glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_DEPTH_SIZE,&bits);
	//printf("texture depth precision = %d\n", bits);

	// workaround for Radeon HD bug (Catalyst 7.9)
	if(bits==8) bits = 16;

	return bits;
}

/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::createShadowmap(unsigned width, unsigned height)
{
	return new TextureShadowMap(width,height);
}

}; // namespace
