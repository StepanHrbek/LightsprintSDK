// --------------------------------------------------------------------------
// DemoEngine
// TextureShadowMap, Texture that can hold shadowmap, uses FBO when available.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "TextureShadowmap.h"
#include "FBO.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

TextureShadowMap::TextureShadowMap(unsigned awidth, unsigned aheight)
	: TextureGL(NULL, awidth, aheight, false, TF_NONE, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
{
	// for shadow2D() instead of texture2D()
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
}

bool TextureShadowMap::renderingToBegin(unsigned side)
{
	if(!globalFBO) globalFBO = new FBO();
	globalFBO->setRenderTargetDepth(id);
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
