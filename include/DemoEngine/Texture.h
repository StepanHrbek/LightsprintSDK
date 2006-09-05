// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object. Able to load truecolor .tga from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include "DemoEngine.h"
#include <GL/glew.h>


/////////////////////////////////////////////////////////////////////////////
//
// Texture

class RR_API Texture
{
public:
	Texture(unsigned char *data, int width, int height, int type, // data are adopted and delete[]d later
		int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR, 
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	void bindTexture() const;
	virtual bool getPixel(float x, float y, float* rgb);
	virtual ~Texture();

	static Texture* load(char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

protected:
	unsigned int id;
	int channels;
	int width;
	int height;
	unsigned char* pixels;
};

#endif //TEXTURE
