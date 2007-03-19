// --------------------------------------------------------------------------
// DemoEngine
// FBO, OpenGL framebuffer object, GL_EXT_framebuffer_object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#ifndef FBO_H
#define FBO_H

#include <GL/glew.h>
#include "Lightsprint/DemoEngine/DemoEngine.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// FBO

class FBO
{
public:
	FBO();
	~FBO();
	//! \param textarget
	//!  GL_TEXTURE_2D for 2D texture, TEXTURE_CUBE_MAP_POSITIVE_X etc for cube texture.
	bool setRenderTarget(unsigned color_id, unsigned depth_id, unsigned textarget = GL_TEXTURE_2D);
	void restoreDefaultRenderTarget();
private:
	GLuint fb;
};

}; // namespace

#endif
