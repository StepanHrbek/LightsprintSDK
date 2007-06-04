// --------------------------------------------------------------------------
// DemoEngine
// Texture stored in system memory.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#ifndef TEXTUREINMEMORY_H
#define TEXTUREINMEMORY_H

#include "Lightsprint/DemoEngine/Texture.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureInMemory

class TextureInMemory : public Texture
{
public:
	TextureInMemory(unsigned char *data, int width, int height, bool cube, Format format, bool buildMipmaps);

	// for load
	virtual bool reset(unsigned width, unsigned height, Format format, unsigned char* data, bool buildMipmaps);

	// for save
	virtual const unsigned char* lock();
	virtual void unlock();

	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual Format getFormat() const {return format;}
	virtual bool isCube() const {return cubeOr2d==GL_TEXTURE_CUBE_MAP;}
	virtual bool getPixel(float x, float y, float z, float rgba[4]) const;

	virtual void bindTexture() const;

	virtual bool renderingToBegin(unsigned side = 0); ///< If more textures call this repeatedly, it is faster when they have the same resolution.
	virtual void renderingToEnd(); ///< Can be omitted if you follow with another renderingToBegin().

	virtual ~TextureInMemory();

protected:

	unsigned width;
	unsigned height;
	Format   format;
	unsigned char* pixels;

	unsigned bytesPerPixel; // 3, 4, 12, 16
	unsigned bytesTotal;
	unsigned cubeOr2d; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP
};

}; // namespace

#endif //TEXTURE
