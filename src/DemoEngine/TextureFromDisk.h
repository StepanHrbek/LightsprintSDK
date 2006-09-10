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

	TextureFromDisk(char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
protected:
	void changeNameConvention(char *filename) const;
	unsigned char *loadData(char *filename);
	//unsigned char *loadJpeg(char *filename);
	//unsigned char *decodeJpeg(struct jpeg_decompress_struct *cinfo);
	unsigned char *loadBmp(char *filename);
	unsigned char *loadBmpData(FILE *file);
	unsigned char *loadTga(char *filename);
	void checkFileOpened(FILE *file);

	const static char *JPG_EXT;
	const static char *BMP_EXT;
};

#endif
