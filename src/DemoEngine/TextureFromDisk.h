// --------------------------------------------------------------------------
// DemoEngine
// Texture implementation that loads image from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef TEXTUREFROMDISK_H
#define TEXTUREFROMDISK_H

#include "TextureGL.h"


/////////////////////////////////////////////////////////////////////////////
//
// TextureFromDisk

class TextureFromDisk : public TextureGL
{
public:
	class xFileNotFound {};
	class xNotSuchFormat {};
	class xNotASupportedFormat {};

	TextureFromDisk(const char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
protected:
	unsigned char *loadData(const char *filename);
	unsigned char *loadTga(const char *filename);
};

#endif
