// --------------------------------------------------------------------------
// DemoEngine
// Texture stored in system memory.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "TextureInMemory.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureInMemory

TextureInMemory::TextureInMemory(const unsigned char *adata, int awidth, int aheight, bool acube, Format aformat)
{
	// never changes in life of texture
	cube = acube;

	// changes only in reset()
	width = 0;
	height = 0;
	pixels = NULL;
	TextureInMemory::reset(awidth,aheight,aformat,adata,false);
}

bool TextureInMemory::reset(unsigned awidth, unsigned aheight, Format aformat, const unsigned char* adata, bool buildMipmaps)
{
	bytesTotal = awidth*aheight*getBytesPerPixel(aformat)*(cube?6:1);

	// copy data
	if(!pixels || !adata || width!=awidth || height!=aheight || format!=aformat)
	{
		delete[] pixels;
		pixels = adata ? new unsigned char[bytesTotal] : NULL;
	}
	if(pixels && pixels!=adata)
	{
		memcpy(pixels,adata,bytesTotal);
	}

	width = awidth;
	height = aheight;
	format = aformat;
	
	return true;
}

const unsigned char* TextureInMemory::lock()
{
	return pixels;
}

void TextureInMemory::unlock()
{
}

void TextureInMemory::bindTexture() const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"TextureInMemory::bindTexture() not supported."));
}

bool TextureInMemory::getPixel(float ax, float ay, float az, float rgba[4]) const
{
	if(!pixels) return false;
	unsigned ofs;
	if(!cube)
	{
		// 2d lookup
		unsigned x = unsigned(ax * (width)) % width;
		unsigned y = unsigned(ay * (height)) % height;
		ofs = x+y*width;
	}
	else
	{
		// cube lookup
		assert(width==height);
		// find major axis
		float direction[3] = {ax,ay,az};
		float d[3] = {fabs(ax),fabs(ay),fabs(az)};
		unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 ); // 0..2
		// find side
		unsigned side = 2*axis + ((direction[axis]<0)?1:0); // 0..5
		// find xy
		unsigned x = (unsigned) CLAMPED((int)((direction[ axis   ?0:1]/direction[axis]+1)*(0.5f*width )),0,(int)width -1); // 0..width
		unsigned y = (unsigned) CLAMPED((int)((direction[(axis<2)?2:1]/direction[axis]+1)*(0.5f*height)),0,(int)height-1); // 0..height
		// read texel
		assert(x<width);
		assert(y<height);
		ofs = width*height*side+width*y+x;
		if(ofs>=width*height*6)
		{
			assert(0);
			return false;
		}
	}
	ofs *= getBytesPerPixel(format);
	assert(ofs<bytesTotal);
	switch(format)
	{
		case TF_RGB:
			rgba[0] = pixels[ofs+0]/255.0f;
			rgba[1] = pixels[ofs+1]/255.0f;
			rgba[2] = pixels[ofs+2]/255.0f;
			rgba[3] = 1;
			break;
		case TF_RGBA:
			rgba[0] = pixels[ofs+0]/255.0f;
			rgba[1] = pixels[ofs+1]/255.0f;
			rgba[2] = pixels[ofs+2]/255.0f;
			rgba[3] = pixels[ofs+3]/255.0f;
			break;
		case TF_RGBF:
			rgba[0] = ((float*)(pixels+ofs))[0];
			rgba[1] = ((float*)(pixels+ofs))[1];
			rgba[2] = ((float*)(pixels+ofs))[2];
			rgba[3] = 1;
			break;
		case TF_RGBAF:
			rgba[0] = ((float*)(pixels+ofs))[0];
			rgba[1] = ((float*)(pixels+ofs))[1];
			rgba[2] = ((float*)(pixels+ofs))[2];
			rgba[3] = ((float*)(pixels+ofs))[3];
			break;
	}
	return true;
}

bool TextureInMemory::renderingToBegin(unsigned side)
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"TextureInMemory::renderingToBegin() not supported."));
	return false;
}

void TextureInMemory::renderingToEnd()
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"TextureInMemory::renderingToEnd() not supported."));
}

TextureInMemory::~TextureInMemory()
{
	delete[] pixels;
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::createM(const unsigned char *data, int width, int height, bool cube, Format format)
{
	return new TextureInMemory(data,width,height,cube,format);
}

Texture* Texture::loadM(const char *filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	Texture* texture = new TextureInMemory(NULL,1,1,cubeSideName!=NULL,Texture::TF_RGBA);
	if(!texture->reload(filename,cubeSideName,flipV,flipH))
	{
		SAFE_DELETE(texture);
	}
	return texture;
}

}; // namespace
