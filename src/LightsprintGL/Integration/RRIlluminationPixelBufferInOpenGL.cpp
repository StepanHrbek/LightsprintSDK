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
// RRIlluminationPixelBufferInOpenGL

RRIlluminationPixelBufferInOpenGL::RRIlluminationPixelBufferInOpenGL(const char* filename, unsigned awidth, unsigned aheight)
{
	if(filename)
		texture = Texture::load(filename,NULL,false,false,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);
	else
		texture = Texture::create(NULL,awidth,aheight,false,Texture::TF_RGBA,GL_LINEAR,GL_LINEAR,GL_REPEAT,GL_REPEAT);
}

void RRIlluminationPixelBufferInOpenGL::reset(const rr::RRVec4* data)
{
	texture->reset(texture->getWidth(),texture->getHeight(),Texture::TF_RGBAF/*texture->getFormat()*/,(unsigned char*)data,false);
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
}


/////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolverGL

rr::RRIlluminationPixelBuffer* RRDynamicSolverGL::createIlluminationPixelBuffer(unsigned w, unsigned h)
{
	return new RRIlluminationPixelBufferInOpenGL(NULL,w,h);
}

rr::RRIlluminationPixelBuffer* RRDynamicSolverGL::loadIlluminationPixelBuffer(const char* filename)
{
	RRIlluminationPixelBufferInOpenGL* illum = new RRIlluminationPixelBufferInOpenGL(filename,0,0);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
