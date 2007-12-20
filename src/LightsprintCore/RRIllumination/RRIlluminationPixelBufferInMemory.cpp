// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include "Lightsprint/RRDebug.h"
#include "RRIlluminationPixelBufferInMemory.h"

//#define DIAGNOSTIC // kazdy texel dostane barvu podle toho kolikrat do nej bylo zapsano
//#define DIAGNOSTIC_RED_UNRELIABLE // ukaze nespolehlivy texely cervene

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBufferInMemory

RRIlluminationPixelBufferInMemory::RRIlluminationPixelBufferInMemory(const char* filename, unsigned _width, unsigned _height)
{
	if(filename)
	{
		texture = rr_gl::Texture::loadM(filename,NULL,false,false);
	}
	else
	{
		texture = rr_gl::Texture::createM(NULL,_width,_height,false,rr_gl::Texture::TF_RGBA);
	}
	lockedPixels = NULL;
}

void RRIlluminationPixelBufferInMemory::reset(const RRVec4* data)
{
	texture->reset(texture->getWidth(),texture->getHeight(),rr_gl::Texture::TF_RGBAF/*texture->getFormat()*/,(unsigned char*)data,false);
}

unsigned RRIlluminationPixelBufferInMemory::getWidth() const
{
	return texture->getWidth();
}

unsigned RRIlluminationPixelBufferInMemory::getHeight() const
{
	return texture->getHeight();
}

const RRColorRGBA8* RRIlluminationPixelBufferInMemory::lock()
{
	SAFE_DELETE_ARRAY(lockedPixels);
	rr_gl::Texture::Format format = texture->getFormat();
	// TF_RGBA
	if(format==rr_gl::Texture::TF_RGBA)
		return (const RRColorRGBA8*)texture->lock();
	// TF_RGBAF
	RR_ASSERT(format==rr_gl::Texture::TF_RGBAF);
	const float* lockedPixelsF = (const float*)texture->lock();
	unsigned elements = getWidth()*getHeight()*4;
	lockedPixels = new unsigned char[elements];
	for(unsigned i=0;i<elements;i++)
		lockedPixels[i] = CLAMPED((unsigned)(lockedPixelsF[i]*255),0,255);
	return (const RRColorRGBA8*)lockedPixels;
}

const RRColorRGBF* RRIlluminationPixelBufferInMemory::lockRGBF()
{
	SAFE_DELETE_ARRAY(lockedPixels);
	rr_gl::Texture::Format format = texture->getFormat();
	// TF_RGBF
	if(format==rr_gl::Texture::TF_RGBF)
		return (const RRColorRGBF*)texture->lock();
	// TF_RGBA
	return NULL;
}

void RRIlluminationPixelBufferInMemory::unlock()
{
	SAFE_DELETE_ARRAY(lockedPixels);
	texture->unlock();
}

void RRIlluminationPixelBufferInMemory::bindTexture() const
{
	texture->bindTexture();
}

bool RRIlluminationPixelBufferInMemory::save(const char* filename)
{
	return texture->save(filename,NULL);
}

RRIlluminationPixelBufferInMemory::~RRIlluminationPixelBufferInMemory()
{
	delete texture;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBuffer

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::create(unsigned w, unsigned h)
{
	return new RRIlluminationPixelBufferInMemory(NULL,w,h);
}

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::load(const char* filename)
{
	RRIlluminationPixelBufferInMemory* illum = new RRIlluminationPixelBufferInMemory(filename,0,0);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
