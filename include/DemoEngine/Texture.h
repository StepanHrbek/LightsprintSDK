// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object. Able to load image from disk.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>
#include "DemoEngine.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// Texture

//! Generic texture interface, suitable for 2D and CUBE textures.
//
//! Contains tools that create empty 2D or CUBE texture,
//! shadowmap or load image from disk.
class DE_API Texture
{
public:
	/////////////////////////////////////////////////////////////////////////
	// interface
	/////////////////////////////////////////////////////////////////////////

	//! Set size of 2D texture.
	virtual void setSize(unsigned width, unsigned height) {};

	//! \return Width of texture.
	virtual unsigned getWidth() const = 0;
	//! \return Height of texture.
	virtual unsigned getHeight() const = 0;
	//! \return Number of bits in one texel.
	virtual unsigned getTexelBits() {return 0;}
	//! Fills rgb[0], rgb[1] and rgb[2] with rgb color of texel at [x,y] coordinates
	//! (in 0..1 range).
	//! \return True when supported by implementation.
	virtual bool getPixel(float x, float y, float* rgb) const {return false;}

	//! Binds texture for rendering.
	//! Various implementations may do OpenGL bind, Direct3D bind or nothing.
	virtual void bindTexture() const = 0;

	//! Begins rendering into the texture, sets graphics pipeline so that
	//! following rendering commands use this texture as render target.
	//! \param side
	//!  Selects cube side for rendering into.
	//!  Set to 0 for 2D texture or 0..5 for cube texture, where
	//!  0=x+ side, 1=x- side, 2=y+ side, 3=y- side, 4=z+ side, 5=z- side.
	//! \return True on success, fail when rendering into texture is not possible.
	virtual bool renderingToBegin(unsigned side = 0) = 0;
	//! Ends rendering into the texture.
	virtual void renderingToEnd() = 0;

	virtual ~Texture() {};


	/////////////////////////////////////////////////////////////////////////
	// tools
	/////////////////////////////////////////////////////////////////////////

	//! Creates 2D or CUBE texture in OpenGL.
	//! \param data Image used for initial contents of texture.
	//!  Stored in format that depends on type, see glTexImage2D for more details.
	//!  Memory block pointed by data is adopted and delete[]d in destructor of created instance.
	//!  If it's NULL, uninitialized texture is created.
	//! \param width Width of texture in texels.
	//!  Some computers may support only power of two sizes.
	//! \param height Height of texture in texels.
	//!  Some computers may support only power of two sizes.
	//! \param cube True for cube texture, false for 2D texture.
	//! \param type Type of texture, GL_RGB and GL_RGBA are supported.
	//! \param magn Initial magnification filter, see glTexImage2D for more details.
	//! \param mini Initial minification filter, see glTexImage2D for more details.
	//! \param wrapS Initial clamping mode, see glTexImage2D for more details.
	//! \param wrapT Initial clamping mode, see glTexImage2D for more details.
	static Texture* create(unsigned char *data, int width, int height, bool cube, int type,
		int magn = GL_LINEAR, int mini = GL_LINEAR, 
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	//! Creates shadowmap in OpenGL.
	//! \param width Width of shadowmap in texels.
	//!  Some computers may support only power of two sizes.
	//! \param height Height of shadowmap in texels.
	//!  Some computers may support only power of two sizes.
	static Texture* createShadowmap(unsigned width, unsigned height);

	//! Creates 2D or CUBE texture in OpenGL from image stored on disk.
	//! All formats supported by FreeImage are supported (jpg, png, dds etc).
	//! \param filename Name of image file. Must be in supported format.
	//!   For cube textures, filename must contain %s wildcard, that will be replaced by cubeSideName.
	//!   Example: "/maps/cube_%s.png".
	//! \param cubeSideName Array of six unique names of cube sides in following order:
	//!   x+ side, x- side, y+ side, y- side, z+ side, z- side.
	//!   Examples: {"0","1","2","3","4","5"}, {"rt","lf","up","dn","ft","bk"}.
	//!   Set to NULL for 2D texture.
	//! \param magn Initial magnification filter, see glTexImage2D for more details.
	//! \param mini Initial minification filter, see glTexImage2D for more details.
	//! \param wrapS Initial clamping mode, see glTexImage2D for more details.
	//! \param wrapT Initial clamping mode, see glTexImage2D for more details.
	static Texture* load(const char *filename, const char* cubeSideName[6],
		int magn = GL_LINEAR, int mini = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	//! Saves texture to disk and returns true on success.
	//! See load() for description of parameters.
	virtual bool save(const char* filename, const char* cubeSideName[6]) {return false;}
};

}; // namespace

#endif
