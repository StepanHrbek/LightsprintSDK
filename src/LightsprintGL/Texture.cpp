// --------------------------------------------------------------------------
// LightsprintGL
// Texture, OpenGL extension to RRBuffer.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/RRDebug.h"
#include "FBO.h"
#include <vector>

namespace rr_gl
{

FBO* Texture::globalFBO = NULL;
unsigned Texture::numPotentialFBOUsers = 0;

Texture::Texture(rr::RRBuffer* _buffer, bool _buildMipmaps, int magn, int mini, int wrapS, int wrapT)
{
	if(!_buffer)
		rr::RRReporter::report(rr::ERRO,"Creating texture from NULL buffer.\n");
	buffer = _buffer;
	ownBuffer = false;

	numPotentialFBOUsers++;

	glGenTextures(1, &id);
	reset(_buildMipmaps);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, (_buildMipmaps&&(mini==GL_LINEAR))?GL_LINEAR_MIPMAP_LINEAR:mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, (buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, (buffer->getType()==rr::BT_CUBE_TEXTURE)?GL_CLAMP_TO_EDGE:wrapT);
}

void Texture::reset(bool _buildMipmaps)
{
	const unsigned char* data = buffer->lock(rr::BL_READ);
	switch(buffer->getType())
	{
		//case rr::BT_1D_TEXTURE: cubeOr2d = GL_TEXTURE_1D; break;
		case rr::BT_2D_TEXTURE: cubeOr2d = GL_TEXTURE_2D; break;
		//case rr::BT_3D_TEXTURE: cubeOr2d = GL_TEXTURE_3D; break;
		case rr::BT_CUBE_TEXTURE: cubeOr2d = GL_TEXTURE_CUBE_MAP; break;
		default: rr::RRReporter::report(rr::ERRO,"Texture of invalid type created.\n"); break;
	}

	GLenum glinternal; // GL_RGB, GL_RGBA
	GLenum glformat; // GL_RGB, GL_RGBA
	GLenum gltype; // GL_UNSIGNED_BYTE, GL_FLOAT
	switch(buffer->getFormat())
	{
		case rr::BF_RGB: glinternal = glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBA: glinternal = glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; break;
		case rr::BF_RGBF: glinternal = /*GL_RGB16F_ARB;*/ glformat = GL_RGB; gltype = GL_FLOAT; break;
		case rr::BF_RGBAF: glinternal = /*GL_RGBA16F_ARB;*/ glformat = GL_RGBA; gltype = GL_FLOAT; break;
		case rr::BF_DEPTH: glinternal = GL_DEPTH_COMPONENT; glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; break;
		default: rr::RRReporter::report(rr::ERRO,"Texture of unknown format created.\n"); break;
	}
	/* print histogram
	switch(buffer->getFormat())
	{
		case rr::BF_RGBF:
			if(data)
			{
				unsigned group[4]={0,0,0};
				for(unsigned i=0;i<buffer->getWidth()*buffer->getHeight()*buffer->getDepth()*3;i++)
				{
					group[(((float*)data)[i]<0)?0:((((float*)data)[i]>1)?3:(((float*)data)[i]?2:1))]++;
				}
				rr::RRReporter::report(rr::INF1,"Texture::reset RGBF (-inf..0)=%d <0>=%d (0..1>=%d (1..inf)=%d\n",group[0],group[1],group[2],group[3]);
			}
			break;
		case rr::BF_RGBAF:
			if(data)
			{
				unsigned group[4]={0,0,0,0};
				for(unsigned i=0;i<buffer->getWidth()*buffer->getHeight()*buffer->getDepth()*4;i++)
				{
					group[(((float*)data)[i]<0)?0:((((float*)data)[i]>1)?3:(((float*)data)[i]?2:1))]++;
				}
				rr::RRReporter::report(rr::INF1,"Texture::reset RGBAF (-inf..0)=%d <0>=%d (0..1>=%d (1..inf)=%d\n",group[0],group[1],group[2],group[3]);
			}
			break;
	}*/
	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(glformat==GL_DEPTH_COMPONENT)
	{
		// depthmap -> init with no data
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
	}
	else
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube -> init with data
		for(unsigned side=0;side<6;side++)
		{
			const unsigned char* sideData = data?data+side*buffer->getWidth()*buffer->getHeight()*(buffer->getElementBits()/8):NULL;
			RR_ASSERT(!_buildMipmaps || sideData);
			if(_buildMipmaps && sideData)
				gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,glinternal,buffer->getWidth(),buffer->getHeight(),glformat,gltype,sideData);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,sideData);
		}
	}
	else
	{
		// 2d -> init with data
		RR_ASSERT(!_buildMipmaps || data);
		if(_buildMipmaps && data)
			gluBuild2DMipmaps(GL_TEXTURE_2D,glinternal,buffer->getWidth(),buffer->getHeight(),glformat,gltype,data);
		else
			glTexImage2D(GL_TEXTURE_2D,0,glinternal,buffer->getWidth(),buffer->getHeight(),0,glformat,gltype,data);
	}

	// for shadow2D() instead of texture2D()
	if(buffer->getFormat()==rr::BF_DEPTH)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}
	else
	{
		//!!!
	}

	if(data) buffer->unlock();
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
	if(buffer->getFormat()==rr::BF_DEPTH)
	{
		GLint bits = 0;
		bindTexture();
		glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_DEPTH_SIZE,&bits);
		//printf("texture depth precision = %d\n", bits);

		// workaround for Radeon HD bug (Catalyst 7.9)
		if(bits==8) bits = 16;

		return bits;
	}
	else
	{
		return getBuffer()->getElementBits();
	}
}

bool Texture::renderingToBegin(unsigned side)
{
	if(!globalFBO) globalFBO = new FBO();
	if(side>=6 || (cubeOr2d!=GL_TEXTURE_CUBE_MAP && side))
	{
		RR_ASSERT(0);
		return false;
	}
	if(buffer->getFormat()==rr::BF_DEPTH)
		globalFBO->setRenderTargetDepth(id);
	else
		globalFBO->setRenderTargetColor(id,(cubeOr2d==GL_TEXTURE_CUBE_MAP)?GL_TEXTURE_CUBE_MAP_POSITIVE_X+side:GL_TEXTURE_2D);
	return globalFBO->isStatusOk();
}

void Texture::renderingToEnd()
{
	if(buffer->getFormat()==rr::BF_DEPTH)
		globalFBO->setRenderTargetDepth(0);
	else
		globalFBO->setRenderTargetColor(0,GL_TEXTURE_2D);
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
		for(unsigned i=0;i<6;i++)
			globalFBO->setRenderTargetColor(0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+i);
	globalFBO->restoreDefaultRenderTarget();
}

Texture::~Texture()
{
	if(buffer && buffer->customData==this)
		buffer->customData = NULL;
	if(ownBuffer)
		delete buffer;
	glDeleteTextures(1, &id);
	numPotentialFBOUsers--;
	if(!numPotentialFBOUsers)
	{
		SAFE_DELETE(globalFBO);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// ShadowMap

// AMD doesn't work properly with GL_LINEAR on shadowmaps, it needs GL_NEAREST
static GLenum filtering()
{
	static GLenum mode = GL_LINEAR;
	static bool inited = 0;
	if(!inited)
	{
		char* renderer = (char*)glGetString(GL_RENDERER);
		if(renderer && (strstr(renderer,"Radeon")||strstr(renderer,"RADEON")||strstr(renderer,"FireGL"))) mode = GL_NEAREST;
		inited = 1;
	}
	return mode;
}

Texture* Texture::createShadowmap(unsigned width, unsigned height)
{
	Texture* texture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,width,height,1,rr::BF_DEPTH,true,NULL),false, filtering(), filtering(), GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
	texture->ownBuffer = true;
	return texture;
}


/////////////////////////////////////////////////////////////////////////////
//
// Buffer -> Texture

static std::vector<Texture*> g_textures;

Texture* getTexture(const rr::RRBuffer* _buffer, bool buildMipMaps, int magn, int mini, int wrapS, int wrapT)
{
	if(!_buffer) return NULL;
	rr::RRBuffer* buffer = (rr::RRBuffer*)_buffer; //!!! hack
	if(!buffer->customData)
	{
		Texture* texture = new Texture(buffer,buildMipMaps,magn,mini,wrapS,wrapT);
		buffer->customData = texture;
		g_textures.push_back(texture);
	}
	return (Texture*)(buffer->customData);
}

void deleteTextures()
{
	for(unsigned i=0;i<g_textures.size();i++)
	{
		delete g_textures[i];
	}
	g_textures.clear();
}

}; // namespace
