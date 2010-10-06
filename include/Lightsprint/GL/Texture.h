//--------------------------------------------------------------------------
//! \file Texture.h
//! \brief LightsprintGL | Texture in OpenGL 2.0, extension of RRBuffer
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
//! All rights reserved
//--------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/GL/DemoEngine.h"
#include <GL/glew.h>

namespace rr_gl
{

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
	//! Creates depth texture (shadowmap).
	static Texture* createShadowmap(unsigned width, unsigned height);
	//! Returns texture buffer (with type, width, height, format, data).
	rr::RRBuffer* getBuffer();
	//! Returns texture buffer (with type, width, height, format, data).
	const rr::RRBuffer* getBuffer() const;
	//! Rebuilds texture according to buffer. To be called when buffer changes.
	void reset(bool buildMipMaps, bool compress);
	//! Binds texture.
	void bindTexture() const;
	//! Returns number of bits per texel.
	unsigned getTexelBits() const;
	~Texture();

	unsigned version; // For interal use only. Version of data in GPU, copied from buffer->version.
protected:
	friend class FBO;
	rr::RRBuffer* buffer;
	GLuint   id; // For internal use only.
	GLenum   cubeOr2d; // GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP
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

}

#endif // TEXTURE_H
