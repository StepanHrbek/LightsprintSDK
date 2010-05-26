// --------------------------------------------------------------------------
// Texture, OpenGL extension to RRBuffer.
// Copyright (C) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <climits> // UINT_MAX
#include <cstdlib> // rand
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/RRDebug.h"
#include "FBO.h"
#include "Workaround.h"
#include <vector>

namespace rr_gl
{

// single FBO instance used by renderingToBegin()
// automatically created when needed, destructed with last texture instances
FBO* globalFBO = NULL;
unsigned numPotentialFBOUsers = 0;

Texture::Texture(rr::RRBuffer* _buffer, bool _buildMipmaps, bool _compress, int magn, int mini, int wrapS, int wrapT)
{
	if (!_buffer)
		rr::RRReporter::report(rr::ERRO,"Creating texture from NULL buffer.\n");

	buffer = _buffer ? _buffer->createReference() : NULL;

	numPotentialFBOUsers++;

	if (buffer->getDuration())
	{
		// this is video, let's disable compression and mipmaps
		_compress = false;
		_buildMipmaps = false;
	}

	glGenTextures(1, &id);
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

	if (buffer->version==version)
	{
		// buffer did not change since last reset
		// (we ignore possibility that _buildMipmaps or _compress changed)
		return;
	}

	const unsigned char* data = buffer->lock(rr::BL_READ);
	switch(buffer->getType())
	{
		//case rr::BT_1D_TEXTURE: cubeOr2d = GL_TEXTURE_1D; break;
		case rr::BT_2D_TEXTURE: cubeOr2d = GL_TEXTURE_2D; break;
		//case rr::BT_3D_TEXTURE: cubeOr2d = GL_TEXTURE_3D; break;
		case rr::BT_CUBE_TEXTURE: cubeOr2d = GL_TEXTURE_CUBE_MAP; break;
		default: rr::RRReporter::report(rr::ERRO,"Texture of invalid type created.\n"); break;
	}

	if (// it would be slow to mipmap or compress video
		buffer->getDuration()
		// this might help some drivers (I experienced very rare crash in 8800 driver while creating 1x1 compressed mipmapped texture)
		|| (buffer->getWidth()==1 && buffer->getHeight()==1)
		)
	{
		_buildMipmaps = false;
		_compress = false;
	}

	GLenum glinternal; // GL_RGB, GL_RGBA
	GLenum glformat; // GL_RGB, GL_RGBA
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

bool Texture::renderingToBegin(unsigned side)
{
	if (!buffer) return false;

	if (!globalFBO)
	{
		globalFBO = new FBO();
	}
	if (side>=6 || (cubeOr2d!=GL_TEXTURE_CUBE_MAP && side))
	{
		RR_ASSERT(0);
		return false;
	}
	if (buffer->getFormat()==rr::BF_DEPTH)
		globalFBO->setRenderTargetDepth(id);
	else
		globalFBO->setRenderTargetColor(id,(cubeOr2d==GL_TEXTURE_CUBE_MAP)?GL_TEXTURE_CUBE_MAP_POSITIVE_X+side:GL_TEXTURE_2D);
	return globalFBO->isStatusOk();
}

void Texture::renderingToEnd()
{
	if (!buffer) return;

	if (buffer->getFormat()==rr::BF_DEPTH)
		globalFBO->setRenderTargetDepth(0);
	else
		globalFBO->setRenderTargetColor(0,GL_TEXTURE_2D);
	if (cubeOr2d==GL_TEXTURE_CUBE_MAP)
		for (unsigned i=0;i<6;i++)
			globalFBO->setRenderTargetColor(0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+i);
	globalFBO->restoreDefaultRenderTarget();
	version = rand();
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
	numPotentialFBOUsers--;
	if (!numPotentialFBOUsers)
	{
		RR_SAFE_DELETE(globalFBO);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// ShadowMap

static GLenum filtering()
{
	return Workaround::needsUnfilteredShadowmaps()?GL_NEAREST:GL_LINEAR;
}

Texture* Texture::createShadowmap(unsigned width, unsigned height)
{
	if (width==0 || height==0)
	{
		rr::RRReporter::report(rr::ERRO,"Attempt to create %dx%d shadowmap.\n",width,height);
		return NULL;
	}
	rr::RRBuffer* buffer = rr::RRBuffer::create(rr::BT_2D_TEXTURE,width,height,1,rr::BF_DEPTH,true,NULL);
	if (!buffer)
		return NULL;
	Texture* texture = new Texture(buffer,false,false, filtering(), filtering(), GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
	delete buffer; // Texture already created its own reference, so here we just remove our reference
	return texture;
}


/////////////////////////////////////////////////////////////////////////////
//
// Buffer -> Texture

static std::vector<Texture*> g_textures;

Texture* getTexture(const rr::RRBuffer* _buffer, bool _buildMipMaps, bool _compress, int _magn, int _mini, int _wrapS, int _wrapT)
{
	if (!_buffer)
		return NULL;
	rr::RRBuffer* buffer = const_cast<rr::RRBuffer*>(_buffer); //!!! customData is modified in const object
	Texture*& texture = *(Texture**)(&buffer->customData);
	if (!texture)
	{
		texture = new Texture(buffer,_buildMipMaps,_compress,_magn,_mini,_wrapS,_wrapT);
		g_textures.push_back(texture);
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
	for (unsigned i=0;i<g_textures.size();i++)
	{
		delete g_textures[i];
	}
	g_textures.clear();

	extern void releaseAllBuffers1x1();
	releaseAllBuffers1x1();
}

}; // namespace
