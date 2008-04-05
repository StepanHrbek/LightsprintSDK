// --------------------------------------------------------------------------
// FBO, OpenGL framebuffer object, GL_EXT_framebuffer_object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2008, All rights reserved
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#include "FBO.h"
#include "Lightsprint/RRDebug.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FBO

FBO::FBO()
{
	if(!glewIsSupported("GL_EXT_framebuffer_object"))
	{
		rr::RRReporter::report(rr::ERRO,"GL_EXT_framebuffer_object not supported. Disable 'Extension limit' in Nvidia Control panel.\n");
		exit(0);
	}

	glGenFramebuffersEXT(1, &fb);

	// necessary for "new FBO; setRenderTargetDepth; render..."
	setRenderTargetColor(0);
	restoreDefaultRenderTarget();
}

void FBO::setRenderTargetColor(unsigned color_id, unsigned textarget) const
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, textarget, color_id, 0);
	if(color_id)
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	else
	{
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
}

void FBO::setRenderTargetDepth(unsigned depth_id) const
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth_id, 0);
}

bool FBO::isStatusOk() const
{
	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return true;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			// choose different formats
			// 8800GTS returns this in some near out of memory situations, perhaps with texture that already failed to initialize
			rr::RRReporter::report(rr::ERRO,"FBO failed, not enough memory?\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			// programming error; will fail on all hardware
			// possible reason: color_id texture has LINEAR_MIPMAP_LINEAR, but only one mip level (=incomplete)
			assert(0);
			break;
		default:
			// programming error; will fail on all hardware
			assert(0);
	}
	return false;
}

void FBO::restoreDefaultRenderTarget() const
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

FBO::~FBO()
{
	glDeleteFramebuffersEXT(1, &fb);
}

}; // namespace
