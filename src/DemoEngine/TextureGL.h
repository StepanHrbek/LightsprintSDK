// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef TEXTUREGL_H
#define TEXTUREGL_H

#include <cassert>
#include <GL/glew.h>
#include "TextureInMemory.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

class TextureGL : public Texture
{
public:
	TextureGL(const unsigned char *data, int width, int height, bool cube, Format format,
		int magn=GL_LINEAR, int mini = GL_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	// for load
	virtual bool reset(unsigned width, unsigned height, Format format, const unsigned char* data, bool buildMipmaps);

	// for save
	virtual const unsigned char* lock();
	virtual void unlock();

	virtual unsigned getWidth() const {return textureInMemory->getWidth();}
	virtual unsigned getHeight() const {return textureInMemory->getHeight();}
	virtual Format getFormat() const {return textureInMemory->getFormat();}
	virtual bool isCube() const {return cubeOr2d==GL_TEXTURE_CUBE_MAP;}
	virtual bool getPixel(float x, float y, float z, float rgba[4]) const;

	virtual void bindTexture() const;

	virtual bool renderingToBegin(unsigned side = 0); ///< If more textures call this repeatedly, it is faster when they have the same resolution.
	virtual void renderingToEnd(); ///< Can be omitted if you follow with another renderingToBegin().

	virtual ~TextureGL();

protected:

	TextureInMemory* textureInMemory;

	unsigned id;
	GLenum   glformat; // GL_RGB, GL_RGBA
	GLenum   gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	unsigned bytesPerPixel; // 3, 4, 12, 16
	unsigned bytesTotal;
	unsigned cubeOr2d; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP

	// single FBO instance shared by TextureGL and TextureShadowMap, used by renderingToBegin()
	// automatically created when needed, destructed with last texture instances
	static class FBO* globalFBO;
	static unsigned numPotentialFBOUsers;
};

}; // namespace

#endif //TEXTURE
