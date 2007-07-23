// --------------------------------------------------------------------------
// DemoEngine
// FBO, OpenGL framebuffer object, GL_EXT_framebuffer_object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef FBO_H
#define FBO_H

#include <GL/glew.h>
#include "Lightsprint/GL/DemoEngine.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FBO

class FBO
{
public:
	FBO();
	~FBO();
	//! Sets color buffer of FBO, could be combined with depth.
	//! Set 0 to disable color.
	//! \param textarget
	//!  GL_TEXTURE_2D for 2D texture, TEXTURE_CUBE_MAP_POSITIVE_X etc for cube texture.
	void setRenderTargetColor(unsigned color_id, unsigned textarget = GL_TEXTURE_2D) const;
	//! Sets depth buffer of FBO, could be combined with color.
	//! Set 0 to disable depth.
	void setRenderTargetDepth(unsigned depth_id) const;
	bool isStatusOk() const;
	void restoreDefaultRenderTarget() const;
private:
	GLuint fb;
};

}; // namespace

#endif
