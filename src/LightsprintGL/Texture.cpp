// --------------------------------------------------------------------------
// Texture, OpenGL extension to RRBuffer.
// Copyright (C) 2006-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <climits> // UINT_MAX
#include <cstdlib> // rand
#include "Lightsprint/GL/FBO.h"
#include "Lightsprint/RRDebug.h"
#include "Workaround.h"
#include <vector>
#ifdef _MSC_VER
	#include <windows.h> // EXCEPTION_EXECUTE_HANDLER
#endif

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// init GL

bool s_es = false; // is this OpenGL ES?

const char* initializeGL()
{
	// init GLEW
	if (glewInit()!=GLEW_OK)
	{
		return "GLEW init failed (OpenGL 2.0 capable graphics card is required).\n";
	}

	// check gl version
	rr::RRReporter::report(rr::INF2,"OpenGL %s by %s on %s.\n",glGetString(GL_VERSION),glGetString(GL_VENDOR),glGetString(GL_RENDERER));
	int major, minor;
	s_es = sscanf((char*)glGetString(GL_VERSION),"OpenGL ES %d.%d",&major,&minor)==2 && major>=2;
	if (!s_es && (sscanf((char*)glGetString(GL_VERSION),"%d.%d",&major,&minor)!=2 || major<2))
	{
		return "Your system does not support OpenGL 2.0. You can see it with GLview. Note: Some multi-display systems support 2.0 only on one display.\n";
	}

	// check FBO
	if (!GLEW_EXT_framebuffer_object) // added in GL 3.0
	{
		return "GL_EXT_framebuffer_object not supported. Disable 'Extension limit' in Nvidia Control panel.\n";
	}

	// init misc GL states
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	// init "seamless cube maps" feature
	// in OSX 10.7, supported from Radeon HD2400, GeForce 9400(but not 9600,1xx), HD graphics 3000
	if (GLEW_ARB_seamless_cube_map)
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//
// FBO

static FBO      s_fboState;
static unsigned s_numPotentialFBOUsers = 0;
static GLuint   s_fb_id = 0;

void FBO::init()
{
	glGenFramebuffersEXT(1, &s_fb_id);

	// necessary for "new FBO; setRenderTargetDepth; render..."
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_fb_id);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void FBO::done()
{
	glDeleteFramebuffersEXT(1, &s_fb_id);
}

void FBO::setRenderTarget(GLenum attachment, GLenum target, Texture* tex)
{
	setRenderTargetGL(attachment,target,tex?tex->id:0);
}

void FBO::setRenderTargetGL(GLenum attachment, GLenum target, GLuint tex_id)
{
	RR_ASSERT(attachment==GL_DEPTH_ATTACHMENT_EXT || attachment==GL_COLOR_ATTACHMENT0_EXT);
	RR_ASSERT(target==GL_TEXTURE_2D || (target>=GL_TEXTURE_CUBE_MAP_POSITIVE_X && target<=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));

	GLuint fb_id = (tex_id || (attachment==GL_DEPTH_ATTACHMENT_EXT && s_fboState.color_id) || (attachment==GL_COLOR_ATTACHMENT0_EXT && s_fboState.depth_id)) ? s_fb_id : 0;
	if (s_fboState.fb_id != fb_id)
	{
		if (!fb_id)
		{
			if (s_fboState.depth_id)
				glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, s_fboState.depth_id = 0, 0);
			if (s_fboState.color_id)
			{
				glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, s_fboState.color_target = GL_TEXTURE_2D, s_fboState.color_id = 0, 0);
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
			}
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_fboState.fb_id = fb_id);
	}
	if (fb_id)
	{
		if (attachment==GL_DEPTH_ATTACHMENT_EXT)
		{
			RR_ASSERT(target==GL_TEXTURE_2D);
			if (s_fboState.depth_id != tex_id)
				glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, s_fboState.depth_id = tex_id, 0);
		}
		else
		{
			if (s_fboState.color_target != target || s_fboState.color_id != tex_id)
			{
				glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, s_fboState.color_target = target, s_fboState.color_id = tex_id, 0);
				if (tex_id)
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
		}
	}
}

bool FBO::isOk()
{
	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	switch(status)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return true;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			// choose different formats
			// 8800GTS returns this in some near out of memory situations, perhaps with texture that already failed to initialize
			rr::RRReporter::report(rr::ERRO,"FBO failed.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			// programming error; will fail on all hardware
			// possible reason: color_id texture has LINEAR_MIPMAP_LINEAR, but only one mip level (=incomplete)
			RR_ASSERT(0);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			// programming error; will fail on all hardware
			// possible reason: color_id and depth_id textures differ in size
			RR_ASSERT(0);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			// programming error; will fail on all hardware
			// possible reason: glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE); was not called before rendering to depth texture
			RR_ASSERT(0);
			break;
		default:
			// programming error; will fail on all hardware
			RR_ASSERT(0);
	}
	return false;
}


FBO::FBO()
{
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
	setRenderTargetGL(GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,depth_id);
	setRenderTargetGL(GL_COLOR_ATTACHMENT0_EXT,color_target,color_id);
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture::Texture(rr::RRBuffer* _buffer, bool _buildMipmaps, bool _compress, int magn, int mini, int wrapS, int wrapT)
{
	if (!_buffer)
		rr::RRReporter::report(rr::ERRO,"Creating texture from NULL buffer.\n");

	buffer = _buffer ? _buffer->createReference() : NULL;
	if (buffer)
		buffer->customData = this;

	if (!s_numPotentialFBOUsers)
	{
		s_fboState.init();
	}
	s_numPotentialFBOUsers++;

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

	bool srgb = _scaledAsSRGB && buffer->getScaled();
	if (srgb && !GLEW_EXT_texture_sRGB)
	{
		srgb = false;
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"sRGB textures not suported, results may be incorrect. Upgrade your GPU or driver.\n"));
	}
	GLenum glinternal; // GL_RGB8, GL_RGBA8, GL_SRGB8, GL_SRGB8_ALPHA8, GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA, GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA, GL_RGB16F_ARB, GL_RGBA16F_ARB, GL_DEPTH_COMPONENT24...
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	switch(buffer->getFormat())
	{
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
		default: rr::RRReporter::report(rr::ERRO,"Texture of unknown format created.\n"); break;
	}
	if (s_es)
		glinternal = glformat;
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
			const unsigned char* sideData = data?data+side*buffer->getWidth()*buffer->getHeight()*(buffer->getElementBits()/8):NULL;
			glGetError();
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
			if (glGetError())
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
		}
	}
	else
	{
		// 2d
		glGetError();
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
		if (glGetError())
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
	}

	// for shadow2D() instead of texture2D()
	if (buffer->getFormat()==rr::BF_DEPTH)
	{
		RR_ASSERT(!_buildMipmaps);
		// GL_NONE for sampler2D, GL_COMPARE_REF_TO_TEXTURE for sampler2DShadow, otherwise results are undefined
		// we keep all depth textures ready for sampler2DShadow
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		// GL_LEQUAL is default, but let's set it anyway, we don't do it often
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}
	else
	{
		if (_buildMipmaps)
		{
#ifdef _MSC_VER
			__try
			{
#endif
				glGenerateMipmapEXT(cubeOr2d); // part of EXT_framebuffer_object
#ifdef _MSC_VER
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				// This happens in carat-bsp1.dae/carat-bsp2.dae, not sure why.
				RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::ERRO,"GPU driver crashed in glGenerateMipmapEXT().\n"));
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

unsigned Texture::getTexelBits() const
{
	if (!buffer) return 0;

	if (buffer->getFormat()==rr::BF_DEPTH)
	{
		GLint bits = 0;
		bindTexture();
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_DEPTH_SIZE,&bits);
		//printf("texture depth precision = %d\n", bits);

		// workaround for Radeon HD bug (Catalyst 7.9)
		if (bits==8) bits = 16;

		return bits;
	}
	else
	{
		return getBuffer()->getElementBits();
	}
}

Texture::~Texture()
{
	if (buffer)
	{
		// delete buffer's pointer to us, we are destructing
		buffer->customData = NULL;
		// delete our pointer to buffer, buffer may or may not destruct
		delete buffer;
	}
	glDeleteTextures(1, &id);
	id = UINT_MAX;

	s_numPotentialFBOUsers--;
	if (!s_numPotentialFBOUsers)
	{
		s_fboState.done();
	}
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
		return NULL;
	}
	rr::RRBuffer* buffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,width,height,1,color?rr::BF_RGB:rr::BF_DEPTH,true,RR_GHOST_BUFFER);
	if (!buffer)
		return NULL;
	Texture* texture = new Texture(buffer,false,false, filtering(), filtering(), color?GL_CLAMP_TO_EDGE:GL_CLAMP_TO_BORDER, color?GL_CLAMP_TO_EDGE:GL_CLAMP_TO_BORDER);
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
		return NULL;
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
// render target -> RRBuffer

void readPixelsToBuffer(rr::RRBuffer* buffer, unsigned x, unsigned y)
{
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	if (!buffer || buffer->getType()!=rr::BT_2D_TEXTURE)
	{
		rr::RRReporter::report(rr::ERRO,"readBackbufferToBuffer() failed, buffer type is not 2d.\n");
		return;
	}
	switch(buffer->getFormat())
	{
		case rr::BF_RGB: glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_BGR: glformat = GL_BGR; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBA: glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glformat = GL_RGBA; gltype = GL_FLOAT; break;
		case rr::BF_DEPTH: glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCE: glformat = GL_LUMINANCE; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_LUMINANCEF: glformat = GL_LUMINANCE; gltype = GL_FLOAT; break;
		default: rr::RRReporter::report(rr::ERRO,"readBackbufferToBuffer() failed, buffer format not supported.\n"); return;
	}
	unsigned char* pixels = buffer->lock(rr::BL_DISCARD_AND_WRITE);
	if (!pixels)
	{
		rr::RRReporter::report(rr::ERRO,"readBackbufferToBuffer() failed, buffer can't be locked.\n");
		return;
	}
	glReadPixels(x,y,buffer->getWidth(),buffer->getHeight(),glformat,gltype,pixels);
	buffer->unlock();
}

}; // namespace
