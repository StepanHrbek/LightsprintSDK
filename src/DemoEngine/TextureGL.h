// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef TEXTUREGL_H
#define TEXTUREGL_H

#include <cassert>
#include <GL/glew.h>
#include "Lightsprint/DemoEngine/Texture.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

class TextureGL : public Texture
{
public:
	TextureGL(unsigned char *data, int width, int height, bool cube, int type, // data are adopted and delete[]d later
		int magn=GL_LINEAR, int mini = GL_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	virtual void setSize(unsigned width, unsigned height);

	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual bool getPixel(float x, float y, float* rgb) const;

	virtual void bindTexture() const;

	virtual bool save(const char* filename, const char* cubeSideName[6]);

	virtual bool renderingToBegin(unsigned side = 0); ///< If more textures call this repeatedly, it is faster when they have the same resolution.
	virtual void renderingToEnd(); ///< Can be omitted if you follow with another renderingToBegin().

	virtual ~TextureGL();

protected:
	unsigned id;
	unsigned channels;
	unsigned cubeOr2d; // GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP
	unsigned width;
	unsigned height;
	unsigned char* pixels;
private:
	static unsigned numInstances;
};

}; // namespace

#endif //TEXTURE
