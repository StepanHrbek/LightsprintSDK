// --------------------------------------------------------------------------
//! \file Texture.h
//! \brief LightsprintGL | Texture in OpenGL 2.0, extension of RRBuffer
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
// --------------------------------------------------------------------------

#ifndef TEXTURE_H
#define TEXTURE_H

#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/GL/DemoEngine.h"
#include <GL/glew.h>

namespace rr_gl
{

//! Texture is very simple OpenGL wrapper around data from rr::RRBuffer.
//
//! It's basicly glGenTexture() and glTexImage2D(...,buffer->lock(BL_READ)) for 2d data
//! or cube map data so you can immediately use them as a texture in OpenGL pipeline.
class RR_GL_API Texture : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates texture.
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

	//! Begins rendering into the texture, sets graphics pipeline so that
	//! following rendering commands use this texture as render target.
	//! 
	//! You may set color render target and depth render target independently
	//! by calling colorTexture->renderingToBegin() and depthTexture->renderingToBegin().
	//! Be aware that current setting is not archived and restored at renderingToEnd(),
	//! so opening multiple renderingToBegin/End pairs will likely fail.
	//! \param side
	//!  Selects cube side for rendering into.
	//!  Set to 0 for 2D texture or 0..5 for cube texture, where
	//!  0=x+ side, 1=x- side, 2=y+ side, 3=y- side, 4=z+ side, 5=z- side.
	//! \return True on success, fail when rendering into texture is not possible.
	bool renderingToBegin(unsigned side = 0);
	//! Ends rendering into the texture, restores backbuffer (not previous settings).
	void renderingToEnd();
	
	~Texture();

protected:
	rr::RRBuffer* buffer;
	bool     ownBuffer;
	unsigned id;
	GLenum   cubeOr2d; // GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP
};

//! Converts rr::RRBuffer to Texture so it can be immediately used as a texture in OpenGL.
//
//! Before deleting buffer, you should delete texture or at least stop using it, deleteAllTextures() will mass-delete it later.
//! If you delete only texture, set buffer->customData=NULL.
//!
//! Parameters beyond buffer are respected only when called for first time.
//! Once texture is created, successive calls return the same texture.
//! Use Texture::reset() to change already created texture.
RR_GL_API Texture* getTexture(const rr::RRBuffer* buffer, bool buildMipMaps = true, bool compress = true, int magn = GL_LINEAR, int mini = GL_LINEAR, int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);

//! Deletes all textures created by getTexture(). (But you can delete textures one by one as well.)
RR_GL_API void deleteAllTextures();

}

#endif // TEXTURE_H
