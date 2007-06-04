// --------------------------------------------------------------------------
// DemoEngine
// Texture, OpenGL 2.0 object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include "TextureGL.h"
#include "TextureShadowMap.h"
#include "FBO.h"

namespace de
{

FBO* TextureGL::globalFBO = NULL;
unsigned TextureGL::numPotentialFBOUsers = 0;

/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

TextureGL::TextureGL(unsigned char *adata, int awidth, int aheight, bool acube, Format aformat, int magn, int mini, int wrapS, int wrapT)
{
	numPotentialFBOUsers++;

	textureInMemory = new TextureInMemory(adata,awidth,aheight,acube,aformat,mini==GL_LINEAR_MIPMAP_LINEAR);

	// never changes in life of texture
	cubeOr2d = acube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
	glGenTextures(1, &id);
	glBindTexture(cubeOr2d, id);

	// changes only in reset()
	glformat = 0;
	gltype = 0;
	bytesPerPixel = 0;
	TextureGL::reset(awidth,aheight,aformat,adata,false);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, acube?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, acube?GL_CLAMP_TO_EDGE:wrapT);
}

bool TextureGL::reset(unsigned _width, unsigned _height, Format _format, unsigned char* _data, bool _buildMipmaps)
{
	textureInMemory->reset(_width,_height,_format,_data,_buildMipmaps);

	GLenum glinternal;

	switch(_format)
	{
		case TF_RGB: glinternal = glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 3; break;
		case TF_RGBA: glinternal = glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 4; break;
		case TF_RGBF: glinternal = GL_RGB16F_ARB; glformat = GL_RGB; gltype = GL_FLOAT; bytesPerPixel = 12; break;
		case TF_RGBAF: glinternal = GL_RGBA16F_ARB; glformat = GL_RGBA; gltype = GL_FLOAT; bytesPerPixel = 16; break;
		case TF_NONE: glinternal = glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 4; break;
	}

	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(glformat==GL_DEPTH_COMPONENT)
	{
		// depthmap -> init with no data
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,_width,_height,0,glformat,gltype,_data);
	}
	else
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube -> init with data
		for(unsigned side=0;side<6;side++)
		{
			unsigned char* sideData = _data?_data+side*_width*_height*bytesPerPixel:NULL;
			assert(!_buildMipmaps || sideData);
			if(_buildMipmaps && sideData)
				gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,glinternal,_width,_height,glformat,gltype,sideData);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,_width,_height,0,glformat,gltype,sideData);
		}
	}
	else
	{
		// 2d -> init with data
		assert(!_buildMipmaps || _data);
		if(_buildMipmaps && _data)
			gluBuild2DMipmaps(GL_TEXTURE_2D,glinternal,_width,_height,glformat,gltype,_data);
		else
			glTexImage2D(GL_TEXTURE_2D,0,glinternal,_width,_height,0,glformat,gltype,_data);
	}

	return true;
}


// Purpose of this class:
//  If you call save() on this texture, it will save backbuffer.
class BackbufferSaver : public TextureGL
{
public:
	BackbufferSaver() : TextureGL(NULL,1,1,false,TF_RGBA)
	{
		int rect[4];
		glGetIntegerv(GL_VIEWPORT,rect);
		reset(rect[2],rect[3],TF_RGBA,NULL,false);
	}
	virtual bool renderingToBegin(unsigned side = 0)
	{
		return true;
	}
	virtual void renderingToEnd()
	{
	}
};

bool Texture::saveBackbuffer(const char* filename)
{
	BackbufferSaver backbuffer;
	return backbuffer.save(filename,NULL);
}

// warning: degrades float data to bytes
const unsigned char* TextureGL::lock()
{
	// 1. copy data from GL to system memory
	unsigned numSides = isCube()?6:1;
	unsigned char* newData = new unsigned char[numSides*getWidth()*getHeight()*4];
	for(unsigned side=0;side<numSides;side++)
	{
		bool ok = renderingToBegin(side);
		assert(ok);
		glReadPixels(0,0,getWidth(),getHeight(),GL_BGRA,GL_UNSIGNED_BYTE,newData+side*getWidth()*getHeight()*getBytesPerPixel(getFormat()));
	}
	renderingToEnd();
	textureInMemory->reset(getWidth(),getHeight(),TF_RGBA,newData,false);
	delete[] newData;

	// 2. lock system memory
	return textureInMemory->lock();
}

void TextureGL::unlock()
{
	textureInMemory->unlock();
}

void TextureGL::bindTexture() const
{
	glBindTexture(cubeOr2d, id);
}

bool TextureGL::getPixel(float x, float y, float z, float rgba[4]) const
{
	return textureInMemory->getPixel(x,y,z,rgba);
}

bool TextureGL::renderingToBegin(unsigned side)
{
	if(!globalFBO) globalFBO = new FBO();
	if(side>=6 || (cubeOr2d!=GL_TEXTURE_CUBE_MAP && side))
	{
		assert(0);
		return false;
	}
	globalFBO->setRenderTargetColor(id,(cubeOr2d==GL_TEXTURE_CUBE_MAP)?GL_TEXTURE_CUBE_MAP_POSITIVE_X+side:GL_TEXTURE_2D);
	return globalFBO->isStatusOk();
}

void TextureGL::renderingToEnd()
{
	globalFBO->setRenderTargetColor(0,GL_TEXTURE_2D);
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
		for(unsigned i=0;i<6;i++)
			globalFBO->setRenderTargetColor(0,GL_TEXTURE_CUBE_MAP_POSITIVE_X+i);
	globalFBO->restoreDefaultRenderTarget();
}

TextureGL::~TextureGL()
{
	delete textureInMemory;

	glDeleteTextures(1, &id);

	numPotentialFBOUsers--;
	if(!numPotentialFBOUsers)
	{
		SAFE_DELETE(globalFBO);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::create(unsigned char *data, int width, int height, bool cube, Format format, int mag,int min,int wrapS,int wrapT)
{
	return new TextureGL(data,width,height,cube,format,mag,min,wrapS,wrapT);
}

Texture* Texture::load(const char *filename,const char* cubeSideName[6],bool flipV,bool flipH,int mag,int min,int wrapS,int wrapT)
{
	TextureGL* texture = new TextureGL(NULL,1,1,cubeSideName?true:false,Texture::TF_RGBA,mag,min,wrapS,wrapT);
	bool loaded = texture->reload(filename,cubeSideName,flipV,flipH);
	if(!loaded)
	{
		SAFE_DELETE(texture);
		printf("Failed to load %s.\n",filename);
	}
	return texture;
}

}; // namespace
