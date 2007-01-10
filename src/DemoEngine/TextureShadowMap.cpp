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


/////////////////////////////////////////////////////////////////////////////
//
// TextureShadowMap

static FBO* fbo = NULL;
unsigned TextureShadowMap::numInstances = 0;

TextureShadowMap::TextureShadowMap(unsigned awidth, unsigned aheight)
	: TextureGL(NULL, awidth, aheight, false, GL_DEPTH_COMPONENT, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
{
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	channels = 1;
	// for shadow2D() instead of texture2D()
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	numInstances++;
}

void TextureShadowMap::setSize(unsigned awidth, unsigned aheight)
{
	bindTexture();
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,awidth,aheight,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,NULL);
	width = awidth;
	height = aheight;
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

unsigned TextureShadowMap::getTexelBits()
{
	GLint bits = 0;
	bindTexture();
	glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_DEPTH_SIZE,&bits);
	return bits;
}

TextureShadowMap::~TextureShadowMap()
{
	numInstances--;
	if(!numInstances)
	{
		delete fbo;
		fbo = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::createShadowmap(unsigned width, unsigned height)
{
	return new TextureShadowMap(width,height);
}
