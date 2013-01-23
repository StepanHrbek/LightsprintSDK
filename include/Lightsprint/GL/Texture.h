//--------------------------------------------------------------------------
//! \file Texture.h
//! \brief LightsprintGL | Texture in OpenGL 2.0, extension of RRBuffer
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2013
//! All rights reserved
//--------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/GL/DemoEngine.h"
#include <GL/glew.h>

namespace rr_gl
{

//! Initializes OpenGL to state expected by our renderer.
//
//! Initializes glew library and some OpenGL states.
//! Should be called only once, soon after OpenGL context creation.
//! \return NULL on success, error message when OpenGL was not initialized properly.
RR_GL_API const char* initializeGL();


/////////////////////////////////////////////////////////////////////////////
//
// Texture

//! Texture is simple OpenGL wrapper around rr::RRBuffer.
//
//! It supports BT_2D_TEXTURE and BT_CUBE_TEXTURE buffer types.
//! It can be constructed and destructed manually (use one of constructor)
//! or automatically by getTexture() function.
class RR_GL_API Texture : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates texture. Buffer is not adopted, not deleted in destructor.
	Texture(rr::RRBuffer* buffer, bool buildMipMaps, bool compress, int magn = GL_LINEAR, int mini = GL_LINEAR, int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
	//! Creates shadowmap, depth or color texture.
	static Texture* createShadowmap(unsigned width, unsigned height, bool color = false);
	//! Returns texture buffer (with type, width, height, format, data).
	rr::RRBuffer* getBuffer();
	//! Returns texture buffer (with type, width, height, format, data).
	const rr::RRBuffer* getBuffer() const;
	//! Rebuilds texture from buffer and additional parameters.
	//
	//! \param buildMipMaps
	//!  Builds texture with mipmaps.
	//! \param compress
	//!  Builds compressed texture.
	//! \param scaledAsSRGB
	//!  Builds SRGB (rather than RGB) texture for scaled buffers.
	void reset(bool buildMipMaps, bool compress, bool scaledAsSRGB);
	//! Binds texture.
	void bindTexture() const;
	//! Returns number of bits per texel.
	unsigned getTexelBits() const;
	~Texture();

	unsigned version; // For interal use only. Version of data in GPU, copied from buffer->version.
protected:
	friend class FBO;
	rr::RRBuffer* buffer;
	GLuint   id; // Texture id in OpenGL.
	GLenum   cubeOr2d; // one of GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP
	GLenum   internalFormat; // one of GL_RGB8, GL_RGBA8, GL_SRGB8, GL_SRGB8_ALPHA8, GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA, GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA, GL_RGB16F_ARB, GL_RGBA16F_ARB, GL_DEPTH_COMPONENT24...
};


/////////////////////////////////////////////////////////////////////////////
//
// getTexture

//! Converts rr::RRBuffer to Texture so it can be immediately used as a texture in OpenGL.
//
//! All textures created by getTexture() are deleted at once by deleteAllTexture().
//! For textures you want to delete individually, don't use getTexture(), use standard constructor,
//! e.g. "Texture t(...);"
//!
//! Texture uses buffer's customData, you must not use customData for other purposes.
//!
//! Parameters beyond buffer are ignored if texture already exists and buffer version did not change.
//! Change buffer version or use Texture::reset() to force texture update.
RR_GL_API Texture* getTexture(const rr::RRBuffer* buffer, bool buildMipMaps = true, bool compress = true, int magn = GL_LINEAR, int mini = GL_LINEAR, int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

//! Deletes all textures created by getTexture(). (But you can delete textures one by one as well.)
RR_GL_API void deleteAllTextures();

//! Fills entire buffer by reading data from OpenGL memory. Most often this is used to capture screenshot.
//
//! This is shortcut for glReadPixels(x,y,...), with parameters set to fill entire buffer.
//! See glReadPixels() for details on what is read, you might need to call glReadBuffer() to select data source.
//! Buffer type must be BT_2D_TEXTURE.
//! Buffer's size and format don't change, data are converted if format differs.
//! Optional x and y parameters let you read rectangle from given offset.
RR_GL_API void readPixelsToBuffer(rr::RRBuffer* buffer, unsigned x=0, unsigned y=0);

}

#endif // TEXTURE_H
