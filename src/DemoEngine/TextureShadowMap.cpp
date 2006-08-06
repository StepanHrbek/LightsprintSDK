#include <assert.h>
#include <GL/glew.h>
#include "DemoEngine/TextureShadowmap.h"

bool   TextureShadowMap::useFBO = false;
GLuint TextureShadowMap::fb = 0;
GLuint TextureShadowMap::depth_rb = 0;
GLint  TextureShadowMap::depthBits = 0;

TextureShadowMap::TextureShadowMap()
	: Texture(NULL, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, GL_DEPTH_COMPONENT, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER)
{
	glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	channels = 1;
	LIMITED_TIMES(1,oneTimeFBOInit());
	// for shadow2D() instead of texture2D()
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);
}

#define CHECK_FRAMEBUFFER_STATUS()                            \
{                                                           \
	GLenum status;                                            \
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT); \
	switch(status) \
	{                                          \
		case GL_FRAMEBUFFER_COMPLETE_EXT:                       \
			break;                                                \
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:                    \
			/* choose different formats */                        \
			assert(0);                                            \
			break;                                                \
		default:                                                \
			/* programming error; will fail on all hardware */    \
			assert(0);                                            \
	} \
}

void TextureShadowMap::renderingToInit()
{
	if(useFBO)
	{
		// Given:  id - TEXTURE_2D depth texture object
		//         fb - framebuffer object

		// Enable render-to-texture
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);

		// Set up depth_tex for render-to-texture
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
			GL_DEPTH_ATTACHMENT_EXT,
			GL_TEXTURE_2D, id, 0);

		// Check framebuffer completeness at the end of initialization.
		CHECK_FRAMEBUFFER_STATUS();
	}
}

void TextureShadowMap::renderingToDone()
{
	if(useFBO)
	{
		// Re-enable rendering to the window
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		//glBindTexture(GL_TEXTURE_2D, id);
	}
	else
	{
		bindTexture();
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height); // painfully slow on ATI (X800 PRO, Catalyst 6.6)
	}
}

void TextureShadowMap::oneTimeFBOInit()
{
	if(!glewIsSupported("GL_EXT_framebuffer_object"))
	{
		useFBO = false;
		glGetIntegerv(GL_DEPTH_BITS, &depthBits);
		printf("GL_EXT_framebuffer_object not supported, consider updating graphics card drivers.\n");
	}
	else
	{
		useFBO = true;
		glGenFramebuffersEXT(1, &fb);
		glGenRenderbuffersEXT(1, &depth_rb);

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);

		// initialize depth renderbuffer
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_rb);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
			GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
			GL_DEPTH_ATTACHMENT_EXT,
			GL_RENDERBUFFER_EXT, depth_rb);

		// No color buffer to draw to or read from
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		glGetIntegerv(GL_DEPTH_BITS, &depthBits);

		// Check framebuffer completeness at the end of initialization.
		GLenum status;
		status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		switch(status)
		{
			case GL_FRAMEBUFFER_COMPLETE_EXT:
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
				assert(0);
				break;
			default:
				assert(0);
		}

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
}

unsigned TextureShadowMap::getDepthBits()
{
	assert(depthBits);
	return depthBits;
}
