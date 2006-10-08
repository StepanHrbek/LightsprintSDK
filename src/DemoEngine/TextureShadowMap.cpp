#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "TextureShadowmap.h"
#include "FBO.h"


/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

static FBO* fbo = NULL;

TextureShadowMap::TextureShadowMap(unsigned awidth, unsigned aheight)
	: TextureGL(NULL, awidth, aheight, false, GL_DEPTH_COMPONENT, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
{
	glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	channels = 1;
	// for shadow2D() instead of texture2D()
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
}

void TextureShadowMap::renderingToBegin()
{
	if(!fbo) fbo = new FBO();
	fbo->setRenderTarget(0,id);
}

void TextureShadowMap::renderingToEnd()
{
	fbo->restoreDefaultRenderTarget();
}

unsigned TextureShadowMap::getDepthBits()
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
