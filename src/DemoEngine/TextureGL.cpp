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

TextureGL::TextureGL(unsigned char *adata, int awidth, int aheight, bool acube, Format aformat,
				 int magn, int mini, int wrapS, int wrapT)
{
	numPotentialFBOUsers++;

	// never changes in life of texture
	cubeOr2d = acube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
	glGenTextures(1, &id);
	glBindTexture(cubeOr2d, id);

	// changes only in reset()
	width = 0;
	height = 0;
	glformat = 0;
	gltype = 0;
	bytesPerPixel = 0;
	pixels = NULL;
	TextureGL::reset(awidth,aheight,aformat,adata,false);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, acube?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, acube?GL_CLAMP_TO_EDGE:wrapT);
}

unsigned TextureGL::getBytesPerPixel(Texture::Format format)
{
	switch(format)
	{
		case Texture::TF_RGB: return 3;
		case Texture::TF_RGBA: return 4;
		case Texture::TF_RGBF: return 12;
		case Texture::TF_RGBAF: return 16;
		case Texture::TF_NONE: return 4;
	}
	return 0;
}

bool TextureGL::reset(unsigned awidth, unsigned aheight, Format aformat, unsigned char* adata, bool buildMipmaps)
{
	bytesTotal = awidth*aheight*getBytesPerPixel(aformat)*((cubeOr2d==GL_TEXTURE_CUBE_MAP)?6:1);

	// copy data
	if(!pixels || !adata || width!=awidth || height!=aheight || format!=aformat)
	{
		delete[] pixels;
		pixels = adata ? new unsigned char[bytesTotal] : NULL;
	}
	if(pixels && pixels!=adata)
	{
		memcpy(pixels,adata,bytesTotal);
	}

	width = awidth;
	height = aheight;
	format = aformat;

	GLenum glinternal;

	switch(format)
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
		glTexImage2D(GL_TEXTURE_2D,0,glinternal,width,height,0,glformat,gltype,pixels);
	}
	else
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube -> init with data
		for(unsigned side=0;side<6;side++)
		{
			unsigned char* sideData = pixels?pixels+side*width*height*bytesPerPixel:NULL;
			if(buildMipmaps)
				gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,glinternal,width,height,glformat,gltype,sideData);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glinternal,width,height,0,glformat,gltype,sideData);
		}
	}
	else
	{
		// 2d -> init with data
		if(buildMipmaps)
			gluBuild2DMipmaps(GL_TEXTURE_2D,glinternal,width,height,glformat,gltype,pixels);
		else
			glTexImage2D(GL_TEXTURE_2D,0,glinternal,width,height,0,glformat,gltype,pixels);
	}

	return true;
}

void TextureGL::bindTexture() const
{
	glBindTexture(cubeOr2d, id);
}

bool TextureGL::getPixel(float ax, float ay, float az, float rgba[4]) const
{
	if(!pixels) return false;
	unsigned ofs;
	if(cubeOr2d==GL_TEXTURE_2D)
	{
		// 2d lookup
		unsigned x = unsigned(ax * (width)) % width;
		unsigned y = unsigned(ay * (height)) % height;
		ofs = x+y*width;
	}
	else
	{
		// cube lookup
		assert(width==height);
		// find major axis
		float direction[3] = {ax,ay,az};
		float d[3] = {fabs(ax),fabs(ay),fabs(az)};
		unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 ); // 0..2
		// find side
		unsigned side = 2*axis + ((direction[axis]<0)?1:0); // 0..5
		// find xy
		unsigned x = (unsigned) CLAMPED((int)((direction[ axis   ?0:1]/direction[axis]+1)*(0.5f*width )),0,(int)width -1); // 0..width
		unsigned y = (unsigned) CLAMPED((int)((direction[(axis<2)?2:1]/direction[axis]+1)*(0.5f*height)),0,(int)height-1); // 0..height
		// read texel
		assert(x<width);
		assert(y<height);
		ofs = width*height*side+width*y+x;
		if(ofs>=width*height*6)
		{
			assert(0);
			return false;
		}
	}
	ofs *= bytesPerPixel;
	assert(ofs<bytesTotal);
	switch(format)
	{
		case TF_RGB:
			rgba[0] = pixels[ofs+0]/255.0f;
			rgba[1] = pixels[ofs+1]/255.0f;
			rgba[2] = pixels[ofs+2]/255.0f;
			rgba[3] = 1;
			break;
		case TF_RGBA:
			rgba[0] = pixels[ofs+0]/255.0f;
			rgba[1] = pixels[ofs+1]/255.0f;
			rgba[2] = pixels[ofs+2]/255.0f;
			rgba[3] = pixels[ofs+3]/255.0f;
			break;
		case TF_RGBF:
			rgba[0] = ((float*)(pixels+ofs))[0];
			rgba[1] = ((float*)(pixels+ofs))[1];
			rgba[2] = ((float*)(pixels+ofs))[2];
			rgba[3] = 1;
			break;
		case TF_RGBAF:
			rgba[0] = ((float*)(pixels+ofs))[0];
			rgba[1] = ((float*)(pixels+ofs))[1];
			rgba[2] = ((float*)(pixels+ofs))[2];
			rgba[3] = ((float*)(pixels+ofs))[3];
			break;
	}
	return true;
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
	glDeleteTextures(1, &id);
	delete[] pixels;

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

}; // namespace
