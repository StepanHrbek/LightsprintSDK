// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Texture, OpenGL extension to RRBuffer.
// --------------------------------------------------------------------------

#include <climits> // UINT_MAX
#include <cstdio> // sscanf()
#include <cstdlib> // rand
#include "Lightsprint/GL/FBO.h"
#include "Lightsprint/GL/PreserveState.h"
#include "Lightsprint/RRDebug.h"
#include "Workaround.h"
#include <vector>
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif

namespace rr_gl
{

// RR_GL_ES2 platforms (Android..) are ES 2.0 only, the rest (Windows..) can be switched
#ifndef RR_GL_ES2
	bool s_es = false;
#endif

/////////////////////////////////////////////////////////////////////////////
//
// locally cached state

bool glcache_enabled = false;
enum {NUM_CAPS=65536};
char glcache_isEnabled[NUM_CAPS]; // 0=false,1=true,2=unknown
GLint glcache_viewport[4];
GLint glcache_scissor[4];
GLenum glcache_cullFace;
GLenum glcache_activeTexture;
GLboolean glcache_depthMask;
GLboolean glcache_colorMask[4];
GLint glcache_blendFunc[2];
GLfloat glcache_clearColor[4];
struct { GLenum target; GLuint buffer; } glcache_bindBuffer[4] = {
	{GL_ARRAY_BUFFER,0},
	{GL_ELEMENT_ARRAY_BUFFER,0},
	{GL_PIXEL_PACK_BUFFER_ARB,0},
	{GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD,0}};


void configureGLStateCache(bool enable)
{
	if (enable && !glcache_enabled)
	{
		for (unsigned i=0;i<NUM_CAPS;i++)
			glcache_isEnabled[i] = 2;
		::glGetIntegerv(GL_VIEWPORT, glcache_viewport);
		::glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&glcache_cullFace);
		::glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&glcache_activeTexture);
		::glGetBooleanv(GL_DEPTH_WRITEMASK,&glcache_depthMask);
		::glGetBooleanv(GL_COLOR_WRITEMASK,glcache_colorMask);
		::glGetIntegerv(GL_BLEND_SRC_RGB,&glcache_blendFunc[0]);
		::glGetIntegerv(GL_BLEND_DST_RGB,&glcache_blendFunc[1]);
		::glGetFloatv(GL_COLOR_CLEAR_VALUE,glcache_clearColor);
		// glcache_bindBuffer not filled, could be problem if you call configureGLStateCache() in wrong moment
	}
	glcache_enabled = enable;
}

// --- setters ---

void glEnable(GLenum cap)
{
	if (!glcache_enabled || cap>=NUM_CAPS || glcache_isEnabled[cap] != 1)
	{
		glcache_isEnabled[cap] = 1;
		::glEnable(cap);
	}
}

void glDisable(GLenum cap)
{
	if (!glcache_enabled || cap>=NUM_CAPS || glcache_isEnabled[cap] != 0)
	{
		glcache_isEnabled[cap] = 0;
		::glDisable(cap);
	}
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	if (!glcache_enabled ||
		glcache_viewport[0] != x ||
		glcache_viewport[1] != y ||
		glcache_viewport[2] != w ||
		glcache_viewport[3] != h )
	{
		glcache_viewport[0] = x;
		glcache_viewport[1] = y;
		glcache_viewport[2] = w;
		glcache_viewport[3] = h;
		::glViewport(x,y,w,h);
	}
}

void glScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
	if (!glcache_enabled ||
		glcache_scissor[0] != x ||
		glcache_scissor[1] != y ||
		glcache_scissor[2] != w ||
		glcache_scissor[3] != h )
	{
		glcache_scissor[0] = x;
		glcache_scissor[1] = y;
		glcache_scissor[2] = w;
		glcache_scissor[3] = h;
		::glScissor(x,y,w,h);
	}
}

void glCullFace(GLenum mode)
{
	if (!glcache_enabled || glcache_cullFace != mode)
	{
		glcache_cullFace = mode;
		::glCullFace(mode);
	}
}

void glActiveTexture(GLenum texture)
{
	if (!glcache_enabled || glcache_activeTexture != texture)
	{
		glcache_activeTexture = texture;
		::glActiveTexture(texture);
	}
}

void glBindBuffer(GLenum target, GLuint buffer)
{
	if (glcache_enabled)
	{
		for (unsigned i=0;i<4;i++)
			if (glcache_bindBuffer[i].target == target)
			{
				if (glcache_bindBuffer[i].buffer != buffer)
				{
					glcache_bindBuffer[i].buffer = buffer;
					::glBindBuffer(target, buffer);
				}
				return;
			}
	}
	::glBindBuffer(target, buffer);
}

void glDepthMask(GLboolean depth)
{
	if (!glcache_enabled || glcache_depthMask != depth)
	{
		glcache_depthMask = depth;
		::glDepthMask(depth);
	}
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	if (!glcache_enabled || glcache_colorMask[0]!=red || glcache_colorMask[1]!=green || glcache_colorMask[2]!=blue || glcache_colorMask[3]!=alpha)
	{
		glcache_colorMask[0] = red;
		glcache_colorMask[1] = green;
		glcache_colorMask[2] = blue;
		glcache_colorMask[3] = alpha;
		::glColorMask(red, green, blue, alpha);
	}
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	if (!glcache_enabled || glcache_blendFunc[0]!=sfactor || glcache_blendFunc[1]!=dfactor)
	{
		glcache_blendFunc[0] = sfactor;
		glcache_blendFunc[1] = dfactor;
		::glBlendFunc(sfactor, dfactor);
	}
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	if (!glcache_enabled || glcache_clearColor[0]!=red || glcache_clearColor[1]!=green || glcache_clearColor[2]!=blue || glcache_clearColor[3]!=alpha)
	{
		glcache_clearColor[0] = red;
		glcache_clearColor[1] = green;
		glcache_clearColor[2] = blue;
		glcache_clearColor[3] = alpha;
		::glClearColor(red,green,blue,alpha);
	}
}

// --- getters ---

GLboolean glIsEnabled(GLenum cap)
{
	if (glcache_enabled && cap<NUM_CAPS)
	{
		switch (glcache_isEnabled[cap])
		{
			case 0: return GL_FALSE;
			case 1: return GL_TRUE;
			default: return (glcache_isEnabled[cap] = ::glIsEnabled(cap)?1:0) ? GL_TRUE : GL_FALSE;
		}
	}
	else
		return ::glIsEnabled(cap);
}

void glGetIntegerv(GLenum pname, GLint* params)
{
	if (glcache_enabled && params)
	{
		switch (pname)
		{
			case GL_VIEWPORT:
				params[0] = glcache_viewport[0];
				params[1] = glcache_viewport[1];
				params[2] = glcache_viewport[2];
				params[3] = glcache_viewport[3];
				return;
			case GL_SCISSOR_BOX:
				params[0] = glcache_scissor[0];
				params[1] = glcache_scissor[1];
				params[2] = glcache_scissor[2];
				params[3] = glcache_scissor[3];
				return;
			case GL_CULL_FACE_MODE:
				params[0] = glcache_cullFace;
				return;
			case GL_BLEND_SRC_RGB:
				params[0] = glcache_blendFunc[0];
				return;
			case GL_BLEND_DST_RGB:
				params[0] = glcache_blendFunc[1];
				return;
		}
	}
	::glGetIntegerv(pname, params);
}

void glGetFloatv(GLenum pname, GLfloat* params)
{
	if (glcache_enabled && params)
	{
		switch (pname)
		{
			case GL_COLOR_CLEAR_VALUE:
				params[0] = glcache_clearColor[0];
				params[1] = glcache_clearColor[1];
				params[2] = glcache_clearColor[2];
				params[3] = glcache_clearColor[3];
				return;
		}
	}
	::glGetFloatv(pname, params);
}

void glGetBooleanv(GLenum pname, GLboolean* params)
{
	if (glcache_enabled && params)
	{
		switch (pname)
		{
			case GL_DEPTH_WRITEMASK:
				params[0] = glcache_depthMask;
				return;
			case GL_COLOR_WRITEMASK:
				params[0] = glcache_colorMask[0];
				params[1] = glcache_colorMask[1];
				params[2] = glcache_colorMask[2];
				params[3] = glcache_colorMask[3];
				return;
		}
	}
	::glGetBooleanv(pname, params);
}


/////////////////////////////////////////////////////////////////////////////
//
// FBO

static FBO      s_fboState;
static GLuint   s_fb_id = 0;

void FBO_init()
{
	glGenFramebuffers(1, &s_fb_id);

	// necessary for "new FBO; setRenderTargetDepth; render..."
	glBindFramebuffer(GL_FRAMEBUFFER, s_fb_id);
	FBO::setRenderBuffers(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO_done()
{
	glDeleteFramebuffers(1, &s_fb_id);
}

void FBO::setRenderTarget(GLenum attachment, GLenum target, const Texture* tex, const FBO& oldState)
{
	setRenderTargetGL(attachment,target,tex?tex->id:0,oldState);
}

void FBO::setRenderTargetGL(GLenum attachment, GLenum target, GLuint tex_id, const FBO& oldState)
{
	RR_ASSERT(attachment==GL_DEPTH_ATTACHMENT || attachment==GL_COLOR_ATTACHMENT0);
	RR_ASSERT(target==GL_TEXTURE_2D || (target>=GL_TEXTURE_CUBE_MAP_POSITIVE_X && target<=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));

	GLuint fb_id = (tex_id || (attachment==GL_DEPTH_ATTACHMENT && s_fboState.color_id) || (attachment==GL_COLOR_ATTACHMENT0 && s_fboState.depth_id)) ? s_fb_id : 0;
	if (s_fboState.fb_id != fb_id)
	{
		if (!fb_id)
		{
			if (s_fboState.depth_id)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_fboState.depth_id = 0, 0);
			if (s_fboState.color_id)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, s_fboState.color_target = GL_TEXTURE_2D, s_fboState.color_id = 0, 0);
				FBO::setRenderBuffers(oldState.buffers);
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, s_fboState.fb_id = fb_id);
	}
	if (fb_id)
	{
		if (attachment==GL_DEPTH_ATTACHMENT)
		{
			RR_ASSERT(target==GL_TEXTURE_2D);
			if (s_fboState.depth_id != tex_id)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_fboState.depth_id = tex_id, 0);
		}
		else
		{
			if (s_fboState.color_target != target || s_fboState.color_id != tex_id)
			{
				if (!s_fboState.color_id && tex_id)
				{
					FBO::setRenderBuffers(GL_COLOR_ATTACHMENT0);
				}
				if (s_fboState.color_id && !tex_id)
				{
					FBO::setRenderBuffers(oldState.buffers);
				}
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, s_fboState.color_target = target, s_fboState.color_id = tex_id, 0);
			}
		}
	}
}

void FBO::setRenderBuffers(GLenum buffers)
{
	if (s_fboState.buffers != buffers)
	{
		s_fboState.buffers = buffers;
#ifndef RR_GL_ES2
		if (!s_es)
			glDrawBuffer(buffers);
		glReadBuffer(buffers);
#endif
	}
}

bool FBO::isOk()
{
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch(status)
	{
		case GL_FRAMEBUFFER_COMPLETE:
			return true;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			// choose different formats
			// 8800GTS returns this in some near out of memory situations, perhaps with texture that already failed to initialize
			rr::RRReporter::report(rr::ERRO,"FBO failed.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			// programming error; will fail on all hardware
			// possible reason: color_id texture has LINEAR_MIPMAP_LINEAR, but only one mip level (=incomplete)
			RR_ASSERT(0);
			break;
#ifndef RR_GL_ES2
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			// programming error; will fail on all hardware
			// possible reason: color_id and depth_id textures differ in size
			RR_ASSERT(0);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			// programming error; will fail on all hardware
			// possible reason: glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE); was not called before rendering to depth texture
			RR_ASSERT(0);
			break;
#endif
		default:
			// programming error; will fail on all hardware
			RR_ASSERT(0);
	}
	return false;
}


FBO::FBO()
{
	buffers = GL_FRONT; // anything but GL_NONE, we call setRenderBuffers(GL_NONE) and it must not thing GL_NONE is already set
	fb_id = 0;
	color_target = GL_TEXTURE_2D;
	color_id = 0;
	depth_id = 0;
}

const FBO& FBO::getState()
{
	return s_fboState;
}

void FBO::restore()
{
	setRenderTargetGL(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth_id,*this);
	setRenderTargetGL(GL_COLOR_ATTACHMENT0,color_target,color_id,*this);
	//setRenderBuffers(buffers);
}



/////////////////////////////////////////////////////////////////////////////
//
// init GL

const char* initializeGL(bool enableGLStateCaching)
{
#ifdef __ANDROID__

	// no check for gl version, it should be ensured elsewhere

#else //!__ANDROID__

	// init GLEW
	if (glewInit()!=GLEW_OK)
	{
		return "GLEW init failed (OpenGL 2.0 capable graphics card is required).\n";
	}

	// check gl version
	rr::RRReporter::report(rr::INF2,"OpenGL %s by %s on %s.\n",glGetString(GL_VERSION),glGetString(GL_VENDOR),glGetString(GL_RENDERER));
	int major, minor;
#ifndef RR_GL_ES2
	s_es = sscanf((char*)glGetString(GL_VERSION),"OpenGL ES %d.%d",&major,&minor)==2 && major>=2;
	if (!s_es)
#endif
	if ((sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2))
	{
		return "Your system does not support OpenGL 2.0. You can see it with GLview. Note: Some multi-display systems support 2.0 only on one display.\n";
	}

	// check FBO
	if (!GLEW_ARB_framebuffer_object) // added in GL 3.0
	{
		return "GL_ARB_framebuffer_object not supported. Disable 'Extension limit' in Nvidia Control panel.\n";
	}

	// init "seamless cube maps" feature
	// in OSX 10.7, supported from Radeon HD2400, GeForce 9400(but not 9600,1xx), HD graphics 3000
	if (GLEW_ARB_seamless_cube_map)
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#endif //!__ANDROID__

	configureGLStateCache(enableGLStateCaching);
	FBO_init();

	// init misc GL states
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture::Texture(rr::RRBuffer* _buffer, bool _buildMipmaps, bool _compress, int magn, int mini, int wrapS, int wrapT)
{
	if (!_buffer)
		rr::RRReporter::report(rr::ERRO,"Creating texture from nullptr buffer.\n");

	buffer = _buffer ? _buffer->createReference() : nullptr;
	if (buffer)
		buffer->customData = this;

	if (buffer && buffer->getDuration())
	{
		// this is video, let's disable compression and mipmaps
		_compress = false;
		_buildMipmaps = false;
	}

	glGenTextures(1, &id);
	internalFormat = 0;
	reset(_buildMipmaps,_compress,false);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, (_buildMipmaps&&(mini==GL_LINEAR))?GL_LINEAR_MIPMAP_LINEAR:mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapT);
}

void Texture::reset(bool _buildMipmaps, bool _compress, bool _scaledAsSRGB)
{
	if (!buffer)
		return;

	if (// it would be slow to mipmap or compress video
		buffer->getDuration()
		// this might help some drivers (I experienced very rare crash in 8800 driver while creating 1x1 compressed mipmapped texture)
		|| (buffer->getWidth()==1 && buffer->getHeight()==1)
		)
	{
		_buildMipmaps = false;
		_compress = false;
	}

#ifdef RR_GL_ES2
	bool srgb = false;
#else
	bool srgb = _scaledAsSRGB;// && buffer->getScaled();
	if (srgb && !GLEW_EXT_texture_sRGB)
	{
		srgb = false;
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"sRGB textures not suported, results may be incorrect. Upgrade your GPU or driver.\n"));
	}
#endif
	GLenum glinternal; // GL_RGB8, GL_RGBA8, GL_SRGB8, GL_SRGB8_ALPHA8, GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA, GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA, GL_RGB16F_ARB, GL_RGBA16F_ARB, GL_DEPTH_COMPONENT24...
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	switch(buffer->getFormat())
	{
#ifdef RR_GL_ES2
		case rr::BF_RGB: glinternal = GL_RGB; glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_BGR: glinternal = GL_RGB; glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBA: glinternal = GL_RGBA; glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glinternal = GL_RGB; glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glinternal = GL_RGBA; glformat = GL_RGBA; gltype = GL_FLOAT; break;
		case rr::BF_DEPTH: glinternal = GL_DEPTH_COMPONENT; glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCE: glinternal = GL_LUMINANCE; glformat = GL_LUMINANCE; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCEF: glinternal = GL_LUMINANCE; glformat = GL_LUMINANCE; gltype = GL_FLOAT; break;
#else
		case rr::BF_RGB: glinternal = srgb?(_compress?GL_COMPRESSED_SRGB:GL_SRGB8):(_compress?GL_COMPRESSED_RGB:GL_RGB8); glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_BGR: glinternal = srgb?(_compress?GL_COMPRESSED_SRGB:GL_SRGB8):(_compress?GL_COMPRESSED_RGB:GL_RGB8); glformat = GL_BGR; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBA: glinternal = srgb?(_compress?GL_COMPRESSED_SRGB_ALPHA:GL_SRGB8_ALPHA8):(_compress?GL_COMPRESSED_RGBA:GL_RGBA8); glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glinternal = srgb?(_compress?GL_COMPRESSED_SRGB:GL_SRGB8):GL_RGB16F_ARB; glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glinternal = srgb?(_compress?GL_COMPRESSED_SRGB_ALPHA:GL_SRGB8_ALPHA8):GL_RGBA16F_ARB; glformat = GL_RGBA; gltype = GL_FLOAT; break;
		// GL_SRGB8, GL_SRGB8_ALPHA8, GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA in GL 2.1 or GL_EXT_texture_sRGB
		// GL_RGB16F_ARB, GL_RGBA16F_ARB in GL 3.0 or GL_ARB_texture_float
		// GL_DEPTH_COMPONENT24 reduces shadow bias (compared to GL_DEPTH_COMPONENT)
		// GL_DEPTH_COMPONENT32 does not reduce shadow bias (compared to GL_DEPTH_COMPONENT24)
		// Workaround::needsIncreasedBias() is tweaked for GL_DEPTH_COMPONENT24
		case rr::BF_DEPTH: glinternal = GL_DEPTH_COMPONENT24; glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCE: glinternal = srgb?(_compress?GL_COMPRESSED_SLUMINANCE:GL_SLUMINANCE8):(_compress?GL_COMPRESSED_LUMINANCE:GL_LUMINANCE8); glformat = GL_LUMINANCE; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCEF: glinternal = srgb?(_compress?GL_COMPRESSED_SLUMINANCE:GL_SLUMINANCE):(_compress?GL_COMPRESSED_LUMINANCE:GL_LUMINANCE16); glformat = GL_LUMINANCE; gltype = GL_FLOAT; break;
#endif
		default: rr::RRReporter::report(rr::ERRO,"Texture of unknown format created.\n"); break;
	}
#ifndef RR_GL_ES2
	if (s_es)
		glinternal = glformat;
#endif
	if (buffer->version==version && glinternal==internalFormat)
	{
		// buffer did not change since last reset
		// internal format (affected by _buildMipmaps, _compress, _scaledAsSRGB) did not change since last reset
		return;
	}

	if (srgb && buffer->getElementBits()>32)
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Float textures don't support sRGB, reducing precison.\n"));

	internalFormat = glinternal;

	switch(buffer->getType())
	{
		//case rr::BT_1D_TEXTURE: cubeOr2d = GL_TEXTURE_1D; break;
		case rr::BT_2D_TEXTURE: cubeOr2d = GL_TEXTURE_2D; break;
		//case rr::BT_3D_TEXTURE: cubeOr2d = GL_TEXTURE_3D; break;
		case rr::BT_CUBE_TEXTURE: cubeOr2d = GL_TEXTURE_CUBE_MAP; break;
		default: rr::RRReporter::report(rr::ERRO,"Texture of invalid type created.\n"); break;
	}

	const unsigned char* data = buffer->lock(rr::BL_READ);
	bindTexture();

	if (cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube
		// we rely on GL_TEXTURE_CUBE_MAP_SEAMLESS doing its job. if it is not available, seams are visible
		// in case of need, we can paste here CPU cubemap filtering code removed in revision 5296 from environmentMap.cpp
		for (unsigned side=0;side<6;side++)
		{
			const unsigned char* sideData = data?data+side*buffer->getWidth()*buffer->getHeight()*(buffer->getElementBits()/8):nullptr;
#ifndef RR_GL_ES2
			glGetError();
#endif
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
#ifndef RR_GL_ES2
			if (glGetError())
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
#endif
		}
	}
	else
	{
		// 2d
#ifndef RR_GL_ES2
		glGetError();
#endif
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
#ifndef RR_GL_ES2
		if (glGetError())
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
#endif
	}

	// for shadow2D() instead of texture2D()
	if (buffer->getFormat()==rr::BF_DEPTH)
	{
		RR_ASSERT(!_buildMipmaps);
#ifndef RR_GL_ES2
		// GL_NONE for sampler2D, GL_COMPARE_REF_TO_TEXTURE for sampler2DShadow, otherwise results are undefined
		// we keep all depth textures ready for sampler2DShadow
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		// GL_LEQUAL is default, but let's set it anyway, we don't do it often
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#endif
	}
	else
	{
		if (_buildMipmaps)
		{
#ifdef _MSC_VER
			__try
			{
#endif
				glGenerateMipmap(cubeOr2d); // exists in GL2+ARB_framebuffer_object, GL3, ES2
#ifdef _MSC_VER
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				// This happens in carat-bsp1.dae/carat-bsp2.dae, not sure why.
				RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"GPU driver crashed in glGenerateMipmap().\n"));
			}
#endif
		}
		//!!!
	}

	if (data) buffer->unlock();

	version = buffer->version;
}

void Texture::bindTexture() const
{
	glBindTexture(cubeOr2d, id);
}

rr::RRBuffer* Texture::getBuffer()
{
	return buffer;
}

const rr::RRBuffer* Texture::getBuffer() const
{
	return buffer;
}

Texture::~Texture()
{
	if (buffer)
	{
		// delete buffer's pointer to us, we are destructing
		buffer->customData = nullptr;
		// delete our pointer to buffer, buffer may or may not destruct
		delete buffer;
	}
	glDeleteTextures(1, &id);
	id = UINT_MAX;
}


/////////////////////////////////////////////////////////////////////////////
//
// ShadowMap

static GLenum filtering()
{
	return Workaround::needsUnfilteredShadowmaps()?GL_NEAREST:GL_LINEAR;
}

Texture* Texture::createShadowmap(unsigned width, unsigned height, bool color)
{
	if (width==0 || height==0)
	{
		rr::RRReporter::report(rr::ERRO,"Attempt to create %dx%d shadowmap.\n",width,height);
		return nullptr;
	}
	rr::RRBuffer* buffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,width,height,1,color?rr::BF_RGBA:rr::BF_DEPTH,true,RR_GHOST_BUFFER); // [#49] RGBA because unshadowed areas have a=1, colored shadows have a=0
	if (!buffer)
		return nullptr;
#ifdef RR_GL_ES2
	Texture* texture = new Texture(buffer,false,false, filtering(), filtering(), GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
#else
	Texture* texture = new Texture(buffer,false,false, filtering(), filtering(), color?GL_CLAMP_TO_EDGE:GL_CLAMP_TO_BORDER, color?GL_CLAMP_TO_EDGE:GL_CLAMP_TO_BORDER);
#endif
	delete buffer; // Texture already created its own reference, so here we just remove our reference
	return texture;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer -> Texture

static std::vector<Texture*> s_textures;

Texture* getTexture(const rr::RRBuffer* _buffer, bool _buildMipMaps, bool _compress, int _magn, int _mini, int _wrapS, int _wrapT)
{
	if (!_buffer)
		return nullptr;
	rr::RRBuffer* buffer = const_cast<rr::RRBuffer*>(_buffer); //!!! new Texture() may modify customData in const object
	Texture* texture = (Texture*)(buffer->customData);
	if (texture)
	{
		// verify that texture and buffer are linked in both directions
		RR_ASSERT(texture->getBuffer()==_buffer);
	}
	else
	{
		texture = new Texture(buffer,_buildMipMaps,_compress,_magn,_mini,_wrapS,_wrapT);
		s_textures.push_back(texture);
	}

	// It's easier to have automatic update(). For now, we don't register any unwanted effects, but one day,
	// we will possibly switch to manual update() and remove next line.
	// (potentially unwanted: video rendered several times per frame may be different each time it is rendered)
	buffer->update();

	if (texture->version!=_buffer->version)
		texture->reset(_buildMipMaps,_compress,false);
	return texture;
}

void deleteAllTextures()
{
	for (unsigned i=0;i<s_textures.size();i++)
	{
		delete s_textures[i];
	}
	s_textures.clear();

	extern void releaseAllBuffers1x1();
	releaseAllBuffers1x1();
}

/////////////////////////////////////////////////////////////////////////////
//
// Texture -> RRBuffer

static unsigned char* getFormatLockBuffer(rr::RRBuffer* buffer, GLenum& glformat, GLenum& gltype)
{
	//GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	//GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	if (!buffer)
	{
		rr::RRReporter::report(rr::ERRO,"readPixelsToBuffer() failed, buffer=nullptr.\n");
		return nullptr;
	}
	switch(buffer->getFormat())
	{
		case rr::BF_RGB: glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
#ifdef RR_GL_ES2
		case rr::BF_BGR: glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
#else
		case rr::BF_BGR: glformat = GL_BGR; gltype = GL_UNSIGNED_BYTE; break;
#endif
		case rr::BF_RGBA: glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glformat = GL_RGBA; gltype = GL_FLOAT; break;
		case rr::BF_DEPTH: glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCE: glformat = GL_LUMINANCE; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCEF: glformat = GL_LUMINANCE; gltype = GL_FLOAT; break;
		default: rr::RRReporter::report(rr::ERRO,"readPixelsToBuffer() failed, buffer format not supported.\n"); return nullptr;
	}
	unsigned char* pixels = buffer->lock(rr::BL_DISCARD_AND_WRITE); // increases buffer version
	if (!pixels)
	{
		rr::RRReporter::report(rr::ERRO,"readPixelsToBuffer() failed, buffer can't be locked.\n");
		return nullptr;
	}
	return pixels;
}

void readPixelsToBuffer(rr::RRBuffer* buffer, unsigned x, unsigned y)
{
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	unsigned char* pixels = getFormatLockBuffer(buffer,glformat,gltype);
	if (!pixels)
		return; // error was already reported
	if (buffer->getType()!=rr::BT_2D_TEXTURE)
	{
		rr::RRReporter::report(rr::ERRO,"readPixelsToBuffer() failed, buffer type is not 2d.\n");
		return;
	}
	glReadPixels(x,y,buffer->getWidth(),buffer->getHeight(),glformat,gltype,pixels);
	buffer->unlock();
}

void Texture::copyTextureToBuffer()
{
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	unsigned char* pixels = getFormatLockBuffer(buffer,glformat,gltype);
	if (!pixels)
		return; // error was already reported
	PreserveFBO p1;
	if (buffer->getType()==rr::BT_2D_TEXTURE)
	{
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,this,p1.state);
		glReadPixels(0,0,buffer->getWidth(),buffer->getHeight(),glformat,gltype,pixels);
	}
	else
	if (buffer->getType()==rr::BT_CUBE_TEXTURE)
	{
		for (unsigned side=0;side<6;side++)
		{
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,this,p1.state);
			glReadPixels(0,0,buffer->getWidth(),buffer->getHeight(),glformat,gltype,pixels+buffer->getBufferBytes()/6*side);
		}
	}
	else
	{
		rr::RRReporter::report(rr::ERRO,"copyTextureToBuffer() failed, buffer not 2d or cube.\n");
	}
	buffer->unlock();
	version = buffer->version; // sync version with buffer (buffer version was increased by lock())
}

}; // namespace
