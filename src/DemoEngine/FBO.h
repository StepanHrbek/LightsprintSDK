// --------------------------------------------------------------------------
// DemoEngine
// FBO, OpenGL framebuffer object, GL_EXT_framebuffer_object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef FBO_H
#define FBO_H

#include <GL/glew.h>
#include "DemoEngine/DemoEngine.h"


/////////////////////////////////////////////////////////////////////////////
//
// FBO

class FBO
{
public:
	FBO();
	~FBO();
	void setRenderTarget(unsigned color_id, unsigned depth_id);
	void restoreDefaultRenderTarget();
private:
	GLuint fb;
};

#endif
