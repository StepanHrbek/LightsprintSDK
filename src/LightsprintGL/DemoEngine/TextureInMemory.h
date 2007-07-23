// --------------------------------------------------------------------------
// DemoEngine
// Texture stored in system memory.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#ifndef TEXTUREINMEMORY_H
#define TEXTUREINMEMORY_H

#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureInMemory

class RR_GL_API TextureInMemory : public Texture
{
public:
	TextureInMemory(const unsigned char *data, int width, int height, bool cube, Format format);

	// for load
	virtual bool reset(unsigned width, unsigned height, Format format, const unsigned char* data, bool buildMipmaps);

	// for save
	virtual const unsigned char* lock();
	virtual void unlock();

	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual Format getFormat() const {return format;}
	virtual bool isCube() const {return cube;}
	virtual bool getPixel(float x, float y, float z, float rgba[4]) const;

	virtual void bindTexture() const;

	virtual bool renderingToBegin(unsigned side = 0); ///< If more textures call this repeatedly, it is faster when they have the same resolution.
	virtual void renderingToEnd(); ///< Can be omitted if you follow with another renderingToBegin().

	virtual ~TextureInMemory();

protected:
	unsigned width;
	unsigned height;
	Format   format;
	bool     cube;
	unsigned char* pixels;
	unsigned bytesTotal;
};

}; // namespace

#endif //TEXTURE
