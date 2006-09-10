// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef TEXTUREGL_H
#define TEXTUREGL_H

#include <cassert>
#include "DemoEngine/Texture.h"
#include <GL/glew.h>


/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

class TextureGL : public Texture
{
public:
	TextureGL(unsigned char *data, int width, int height, int type, // data are adopted and delete[]d later
		int mag=GL_LINEAR, int min = GL_LINEAR,//!!!_MIPMAP_LINEAR, 
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual bool getPixel(float x, float y, float* rgb) const;

	virtual void bindTexture() const;

	virtual void renderingToBegin();
	virtual void renderingToEnd();

	virtual ~TextureGL();

protected:
	unsigned int id;
	int channels;
	int width;
	int height;
	unsigned char* pixels;
private:
	static unsigned numInstances;
};

#endif //TEXTURE
