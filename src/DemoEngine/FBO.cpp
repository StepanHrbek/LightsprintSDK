#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#include "FBO.h"


/////////////////////////////////////////////////////////////////////////////
//
// FBO

FBO::FBO(unsigned awidth, unsigned aheight, bool color, bool depth)
{
	width = awidth;
	height = aheight;

	if(!glewIsSupported("GL_EXT_framebuffer_object"))
	{
		printf("GL_EXT_framebuffer_object not supported, consider updating graphics card drivers.\n");
		printf("\nPress enter to close...");
		fgetc(stdin);
		exit(1);
	}

	glGenFramebuffersEXT(1, &fb);
	if(depth) glGenRenderbuffersEXT(1, &depth_rb);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
	if(depth)
	{
		// initialize depth renderbuffer
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_rb);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
			GL_DEPTH_COMPONENT, width, height);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
			GL_DEPTH_ATTACHMENT_EXT,
			GL_RENDERBUFFER_EXT, depth_rb);
	} else {
	}
	if(!color)
	{
		glDrawBuffer(GL_NONE); // necessary to get depthBits
		glReadBuffer(GL_NONE); // which is not possible with incomplete buffer
	}

	glGetIntegerv(GL_DEPTH_BITS, &depthBits);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void FBO::setRenderTarget(unsigned color_id, unsigned depth_id)
{
	//!!! overit ze this se vejde do fbo

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);

	if(color_id)
	{
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, color_id, 0);
	}
	else
	{
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}

	if(depth_id)
	{
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth_id, 0);
	}
	else
	{
		//...
	}

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			// choose different formats
			assert(0);
			break;
		default:
			// programming error; will fail on all hardware
			assert(0);
	}
}

void FBO::restoreDefaultRenderTarget()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

unsigned FBO::getDepthBits()
{
	assert(depthBits);
	return depthBits;
}

FBO::~FBO()
{
	if(depth_rb) glDeleteRenderbuffersEXT(1, &depth_rb);
	glDeleteFramebuffersEXT(1, &fb);
}
