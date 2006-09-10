// --------------------------------------------------------------------------
// DemoEngine
// FBO, OpenGL framebuffer object, GL_EXT_framebuffer_object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef FBO_H
#define FBO_H

#include "DemoEngine/DemoEngine.h"
#include <GL/glew.h>


/////////////////////////////////////////////////////////////////////////////
//
// FBO

class FBO
{
public:
	FBO(unsigned awidth, unsigned aheight, bool acolor, bool adepth);
	~FBO();
	void setRenderTarget(unsigned color_id, unsigned depth_id);
	void restoreDefaultRenderTarget();
	unsigned getDepthBits();
private:
	unsigned width;
	unsigned height;
	GLuint fb;
	GLuint depth_rb;
	GLint depthBits;
};

#endif
