//---------------------------------------------------------------------------
//! \file FBO.h
//! \brief LightsprintGL | Render target manipulation.
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2010
//! All rights reserved
//---------------------------------------------------------------------------

#ifndef FBO_H
#define FBO_H

#include "Lightsprint/GL/Texture.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FBO

//! Lets you set, save and restore render target.
class RR_GL_API FBO
{
public:
	//! Set render target.
	//! \param attachment
	//!  GL_DEPTH_ATTACHMENT_EXT or GL_COLOR_ATTACHMENT0_EXT.
	//! \param target
	//!  GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP_POSITIVE_X or _NEGATIVE_X or _POSITIVE_Y or _NEGATIVE_Y or _POSITIVE_Z or _NEGATIVE_Z.
	//! \param texture
	//!  Texture to be set as render target. May be NULL. If you set both color and depth, sizes must match.
	static void setRenderTarget(GLenum attachment, GLenum target, Texture* texture);
	//! Check whether render target is set correctly (all textures the same size etc).
	static bool isOk();

	//! Return currect state, what textures are set as render target.
	static FBO getState();
	//! Restore saved state.
	void restore();

	FBO();
private:
	static void setRenderTargetGL(GLenum attachment, GLenum target, GLuint texture);

	// Automatic one time initialization.
	friend class Texture;
	//! Called automatically when first texture is created.
	static void init();
	//! Called automatically when last texture is deleted.
	static void done();

	// Saved state.
	GLuint fb_id;
	GLenum color_target;
	GLuint color_id;
	GLuint depth_id;
};

}; // namespace

#endif
