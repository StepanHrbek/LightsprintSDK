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

	//! Texture formats. Implementation is not required to support all of them.
	enum Format
	{
		TF_RGB, // 8bit rgb, 24bits per pixel
		TF_RGBA, // 8bit rgba, 32bits per pixel
		TF_RGBF, // float rgb, 96bits per pixel
		TF_RGBAF, // float rgba, 128bits per pixel
		TF_NONE, // unspecified
	};

	//! Set size and contents of 2D texture.
	//! Textures initialized as cube stay cube textures, 2d stay 2d.
	//! \param width
	//!  Requested width of texture in texels.
	//! \param height
	//!  Requested height of texture in texels.
	//!  Must equal to width for cube texture.
	//! \param format
	//!  Format of data.
	//!  Implementation is not required to support all data formats.
	//! \param data
	//!  Data to be loaded into texture. When set to NULL, contents of texture stays uninitialized.
	//!  Format of data is specified by format.
	//! \param buildMipmaps
	//!  Texture mipmaps are built when set to true.
	//!  Implementation is not required to support mipmaps.
	//! \return True on success, false on error (unsupported size/format).
	virtual bool reset(unsigned width, unsigned height, Format format, unsigned char* data, bool buildMipmaps) = 0;

	//! \return Width of texture.
	virtual unsigned getWidth() const = 0;
	//! \return Height of texture.
	virtual unsigned getHeight() const = 0;
	//! \return Number of bits in one texel.
	virtual unsigned getTexelBits() {return 0;}
	//! Fills rgba[0..3] with rgba color 
	//! of 2d texel at [x,y] or cube texel at [x,y,z].
	//! 2d coordinates are in 0..1 range, 3d coordinates express direction from center to the texel.
	//! \return True when supported by implementation.
	virtual bool getPixel(float x, float y, float z, float rgba[4]) const {return false;}

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
	//!  If it's NULL, 1x1 pixel stub texture is created.
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
	//!   Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
	//!   Set to NULL for 2D texture.
	//! \param flipV
	//!  Flip vertically at load time.
	//! \param flipH
	//!  Flip horizontally at load time.
	//! \param magn Initial magnification filter, see glTexImage2D for more details.
	//! \param mini Initial minification filter, see glTexImage2D for more details.
	//! \param wrapS Initial clamping mode, see glTexImage2D for more details.
	//! \param wrapT Initial clamping mode, see glTexImage2D for more details.
	static Texture* load(const char *filename, const char* cubeSideName[6],
		bool flipV = false, bool flipH = false,
		int magn = GL_LINEAR, int mini = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

	//! Saves texture to disk and returns true on success.
	//! See load() for description of parameters.
	virtual bool save(const char* filename, const char* cubeSideName[6]) {return false;}

	//! Saves backbuffer to disk and returns true on success.
	static bool saveBackbuffer(const char* filename);
};

}; // namespace

#endif
