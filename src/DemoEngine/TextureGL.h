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
	TextureGL(unsigned char *data, int width, int height, bool cube, Format format,
		int magn=GL_LINEAR, int mini = GL_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	virtual bool reset(unsigned width, unsigned height, Format format, unsigned char* data, bool buildMipmaps);

	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual bool getPixel(float x, float y, float z, float rgba[4]) const;

	virtual void bindTexture() const;

	virtual bool save(const char* filename, const char* cubeSideName[6]);

	virtual bool renderingToBegin(unsigned side = 0); ///< If more textures call this repeatedly, it is faster when they have the same resolution.
	virtual void renderingToEnd(); ///< Can be omitted if you follow with another renderingToBegin().

	virtual ~TextureGL();

protected:
	unsigned width;
	unsigned height;
	Format   format;
	unsigned char* pixels;

	unsigned id;
	GLenum   glformat; // GL_RGB, GL_RGBA
	GLenum   gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	unsigned bytesPerPixel; // 3, 4, 12, 16
	unsigned bytesTotal;
	unsigned channels; // 3, 4
	unsigned cubeOr2d; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP

	// single FBO instance shared by TextureGL and TextureShadowMap, used by renderingToBegin()
	// automatically created when needed, destructed with last texture instances
	static class FBO* globalFBO;
	static unsigned numPotentialFBOUsers;
};

}; // namespace

#endif //TEXTURE
