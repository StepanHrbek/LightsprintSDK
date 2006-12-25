// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object. Able to load truecolor .tga from disk.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>
#include "DemoEngine.h"


/////////////////////////////////////////////////////////////////////////////
//
// Texture

class DE_API Texture
{
public:
	/////////////////////////////////////////////////////////////////////////
	// interface
	/////////////////////////////////////////////////////////////////////////

	virtual unsigned getWidth() const = 0;
	virtual unsigned getHeight() const = 0;
	virtual unsigned getDepthBits() {return 0;} ///< Returns number of bits in texture's depth.
	virtual bool getPixel(float x, float y, float* rgb) const {return false;}

	virtual void bindTexture() const = 0;

	virtual void renderingToBegin() = 0; ///< Begins rendering into the texture.
	virtual void renderingToEnd() = 0; ///< Ends rendering into the texture.

	virtual ~Texture() {};


	/////////////////////////////////////////////////////////////////////////
	// tools
	/////////////////////////////////////////////////////////////////////////

	static Texture* create(unsigned char *data, int width, int height, bool cube, int type, // data are adopted and delete[]d in destructor
		int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR, 
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	static Texture* createShadowmap(unsigned width, unsigned height);
	static Texture* load(const char *filename,
		int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
};

#endif
