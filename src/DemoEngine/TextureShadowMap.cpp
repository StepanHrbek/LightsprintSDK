#include <cassert>
#include <cstdio>
#include <GL/glew.h>
#include "TextureShadowmap.h"
#include "FBO.h"

#define SHADOW_MAP_SIZE_MAX 512 //!!! hlidat preteceni

/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

static FBO* fbo = NULL;

void createFBO()
{
	fbo = new FBO(SHADOW_MAP_SIZE_MAX,SHADOW_MAP_SIZE_MAX,false,true);
}

TextureShadowMap::TextureShadowMap(unsigned awidth, unsigned aheight)
	: TextureGL(NULL, awidth, aheight, GL_DEPTH_COMPONENT, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
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
	if(!fbo) createFBO();
	//!!! zvetsit fbo pokud mam moc velkou shadowmapu
	fbo->setRenderTarget(0,id);
}

void TextureShadowMap::renderingToEnd()
{
	fbo->restoreDefaultRenderTarget();
}

unsigned TextureShadowMap::getDepthBits()
{
	if(!fbo) createFBO();
	return fbo->getDepthBits();
}

/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::createShadowmap(unsigned width, unsigned height)
{
	return new TextureShadowMap(width,height);
}
