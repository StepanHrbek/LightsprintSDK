// --------------------------------------------------------------------------
// Texture, OpenGL extension to RRBuffer.
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <climits> // UINT_MAX
#include <cstdlib> // rand
#include "Lightsprint/GL/FBO.h"
#include "Lightsprint/RRDebug.h"
#include "Workaround.h"
#include <vector>

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// FBO

static FBO      s_fboState;
static unsigned s_numPotentialFBOUsers = 0;
static GLuint   s_fb_id = 0;

void FBO::init()
{
	if (!GLEW_EXT_framebuffer_object) // added in GL 3.0
	{
		rr::RRReporter::report(rr::ERRO,"GL_EXT_framebuffer_object not supported. Disable 'Extension limit' in Nvidia Control panel.\n");
		exit(0);
	}

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

FBO FBO::getState()
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
	reset(_buildMipmaps,_compress);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, (_buildMipmaps&&(mini==GL_LINEAR))?GL_LINEAR_MIPMAP_LINEAR:mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, (buffer && buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapT);
}

void Texture::reset(bool _buildMipmaps, bool _compress)
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

	GLenum glinternal; // GL_RGB8, GL_RGBA8, GL_SRGB8, GL_SRGB8_ALPHA8, GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA, GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA, GL_RGB16F_ARB, GL_RGBA16F_ARB, GL_DEPTH_COMPONENT24...
	GLenum glformat; // GL_RGB, GL_RGBA, GL_DEPTH_COMPONENT
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	switch(buffer->getFormat())
	{
		case rr::BF_RGB: glinternal = _compress?GL_COMPRESSED_RGB:GL_RGB8; glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_BGR: glinternal = _compress?GL_COMPRESSED_RGB:GL_RGB8; glformat = GL_BGR; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBA: glinternal = _compress?GL_COMPRESSED_RGBA:GL_RGBA8; glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glinternal = GL_RGB16F_ARB; glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glinternal = GL_RGBA16F_ARB; glformat = GL_RGBA; gltype = GL_FLOAT; break;
		// GL_DEPTH_COMPONENT24 reduces shadow bias (compared to GL_DEPTH_COMPONENT)
		// GL_DEPTH_COMPONENT32 does not reduce shadow bias (compared to GL_DEPTH_COMPONENT24)
		// Workaround::needsIncreasedBias() is tweaked for GL_DEPTH_COMPONENT24
		case rr::BF_DEPTH: glinternal = GL_DEPTH_COMPONENT24; glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		default: rr::RRReporter::report(rr::ERRO,"Texture of unknown format created.\n"); break;
	}
	if (buffer->version==version && glinternal==internalFormat)
	{
		// buffer did not change since last reset
		// internal format (affected by _buildMipmaps, _compress, _scaledAsSRGB) did not change since last reset
		return;
	}

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
	/* print histogram
	switch(buffer->getFormat())
	{
		case rr::BF_RGBF:
			if (data)
			{
				unsigned group[4]={0,0,0};
				for (unsigned i=0;i<buffer->getWidth()*buffer->getHeight()*buffer->getDepth()*3;i++)
				{
					group[(((float*)data)[i]<0)?0:((((float*)data)[i]>1)?3:(((float*)data)[i]?2:1))]++;
				}
				rr::RRReporter::report(rr::INF1,"Texture::reset RGBF (-inf..0)=%d <0>=%d (0..1>=%d (1..inf)=%d\n",group[0],group[1],group[2],group[3]);
			}
			break;
		case rr::BF_RGBAF:
			if (data)
			{
				unsigned group[4]={0,0,0,0};
				for (unsigned i=0;i<buffer->getWidth()*buffer->getHeight()*buffer->getDepth()*4;i++)
				{
					group[(((float*)data)[i]<0)?0:((((float*)data)[i]>1)?3:(((float*)data)[i]?2:1))]++;
				}
				rr::RRReporter::report(rr::INF1,"Texture::reset RGBAF (-inf..0)=%d <0>=%d (0..1>=%d (1..inf)=%d\n",group[0],group[1],group[2],group[3]);
			}
			break;
	}/**/
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if (glformat==GL_DEPTH_COMPONENT)
	{
		// depthmap -> init with no data
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
	}
	else
	if (cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube -> init with data
		for (unsigned side=0;side<6;side++)
		{
			const unsigned char* sideData = data?data+side*buffer->getWidth()*buffer->getHeight()*(buffer->getElementBits()/8):NULL;
			RR_ASSERT(!_buildMipmaps || sideData);
			if (_buildMipmaps && sideData)
				gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,glinternal,buffer->getWidth(),buffer->getHeight(),glformat,gltype,sideData);
			else
			{
				glGetError();
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
				if (glGetError())
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
			}
		}
	}
	else
	{
		// 2d -> init with data
		RR_ASSERT(!_buildMipmaps || data);
		if (_buildMipmaps && data)
			gluBuild2DMipmaps(GL_TEXTURE_2D,glinternal,buffer->getWidth(),buffer->getHeight(),glformat,gltype,data);
		else
		{
			glGetError();
			glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
			if (glGetError())
				glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
		}
	}

	// for shadow2D() instead of texture2D()
	if (buffer->getFormat()==rr::BF_DEPTH)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}
	else
	{
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
// Buffer -> Texture

static std::vector<Texture*> s_textures;

Texture* getTexture(const rr::RRBuffer* _buffer, bool _buildMipMaps, bool _compress, int _magn, int _mini, int _wrapS, int _wrapT)
{
	if (!_buffer)
		return NULL;
	rr::RRBuffer* buffer = const_cast<rr::RRBuffer*>(_buffer); //!!! new Texture() may modify customData in const object
	Texture* texture = (Texture*)(buffer->customData);
	if (!texture)
	{
		texture = new Texture(buffer,_buildMipMaps,_compress,_magn,_mini,_wrapS,_wrapT);
		s_textures.push_back(texture);
	}

	// It's easier to have automatic update(). For now, we don't register any unwanted effects, but one day,
	// we will possibly switch to manual update() and remove next line.
	// (potentially unwanted: video rendered several times per frame may be different each time it is rendered)
	buffer->update();

	if (texture->version!=_buffer->version)
		texture->reset(_buildMipMaps,_compress);
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

}; // namespace
