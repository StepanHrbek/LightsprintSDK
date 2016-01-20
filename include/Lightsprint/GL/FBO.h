//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file FBO.h
//! \brief LightsprintGL | render target manipulation
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
	//! Sets render target.
	//
	//! Default (provided by windowing system) and non-default (texture) targets are never mixed,
	//! so by setting texture, any previous default render target is automatically unset.
	//! To render to custom color and depth textures, call setRenderTarget twice, once for each texture.
	//! To return back to default render target, call restore() on previously backed up state,
	//! or on newly constructed FBO.
	//! \param attachment
	//!  GL_DEPTH_ATTACHMENT or GL_COLOR_ATTACHMENT0.
	//! \param target
	//!  GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP_POSITIVE_X or _NEGATIVE_X or _POSITIVE_Y or _NEGATIVE_Y or _POSITIVE_Z or _NEGATIVE_Z.
	//! \param texture
	//!  Texture to be set as render target. May be nullptr. If you set both color and depth, sizes must match.
	//! \param oldState
	//!  When setting nullptr color texture, setRenderBuffers(oldState.buffers) restores old buffers.
	static void setRenderTarget(GLenum attachment, GLenum target, const Texture* texture, const FBO& oldState);
	//! Similar to setRenderTarget(), but with raw OpenGL texture id instead of Lightsprint Texture.
	static void setRenderTargetGL(GLenum attachment, GLenum target, GLuint texture, const FBO& oldState);
	//! Wrapper for glDrawBuffer() and glReadBuffer().
	//
	//! It is used internally by setRenderTarget(), it is rarely needed outside.
	//! We only use it for setting left&right in quad buffered stereo.
	//! Possible parameters: GL_NONE, GL_COLOR_ATTACHMENT0, GL_BACK_LEFT, GL_BACK_RIGHT..
	static void setRenderBuffers(GLenum draw_buffer);
	//! Check whether render target is set correctly (all textures the same size etc).
	static bool isOk();

	//! Return current state, what textures are set as render target.
	static const FBO& getState();
	//! Restore saved state.
	void restore();

	//! Initializes new instance of default framebuffer, without modifying OpenGL state. Calling restore() would restore default framebuffer.
	FBO();
private:
	// Saved state.
	GLenum buffers; // GL_COLOR_ATTACHMENT0, GL_NONE, GL_BACK_LEFT, GL_BACK_RIGHT
	GLuint fb_id;
	GLenum color_target;
	GLuint color_id;
	GLuint depth_id;
};

}; // namespace

#endif
