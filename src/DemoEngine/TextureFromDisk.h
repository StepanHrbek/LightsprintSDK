// --------------------------------------------------------------------------
// DemoEngine
// Texture implementation that loads image from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef TEXTUREFROMDISK_H
#define TEXTUREFROMDISK_H

#include "TextureGL.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureFromDisk

class TextureFromDisk : public TextureGL
{
public:
	class xFileNotFound {};
	class xNotSuchFormat {};
	class xNotASupportedFormat {};

	TextureFromDisk(const char *filename, bool flipV, bool flipH,
		int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	TextureFromDisk(const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH,
		int mag=GL_LINEAR, int min = GL_LINEAR);
protected:
	static unsigned char *loadData(const char *filename,unsigned& width,unsigned& height,unsigned& channels);
	static unsigned char *loadTga(const char *filename,unsigned& width,unsigned& height,unsigned& channels);
	static unsigned char *loadFreeImage(const char *filename,bool cube,bool flipV,bool flipH,unsigned& width,unsigned& height,unsigned& channels);
};

}; // namespace

#endif
