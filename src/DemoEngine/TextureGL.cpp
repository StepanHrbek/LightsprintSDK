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
#include "FBO.h"

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

unsigned TextureGL::numInstances = 0;

TextureGL::TextureGL(unsigned char *adata, int awidth, int aheight, bool acube, int atype,
				 int magn, int mini, int wrapS, int wrapT)
{
	numInstances++;

	// never changes in life of texture
	cubeOr2d = acube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
	channels = (atype==GL_DEPTH_COMPONENT)? 1 : ((atype == GL_RGB) ? 3 : 4);
	glGenTextures(1, &id);
	glBindTexture(cubeOr2d, id);

	// changes only in reset()
	width = 0;
	height = 0;
	glformat = 0;
	gltype = 0;
	bytesPerPixel = 0;
	pixels = NULL;
	TextureGL::reset(awidth,aheight,(channels==1)?TF_NONE:((channels==3)?TF_RGB:TF_RGBA),adata,false);

	// changes anywhere
	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, mini);
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, magn);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, acube?GL_CLAMP_TO_EDGE:wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, acube?GL_CLAMP_TO_EDGE:wrapT);
}

unsigned getBytesPerPixel(Texture::Format format)
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

	switch(format)
	{
		case TF_RGB: glformat = GL_RGB; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 3; break;
		case TF_RGBA: glformat = GL_RGBA; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 4; break;
		case TF_RGBF: glformat = GL_RGB; gltype = GL_FLOAT; bytesPerPixel = 12; break;
		case TF_RGBAF: glformat = GL_RGBA; gltype = GL_FLOAT; bytesPerPixel = 16; break;
		case TF_NONE: glformat = GL_DEPTH_COMPONENT; gltype = GL_UNSIGNED_BYTE; bytesPerPixel = 4; break;
	}

	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(glformat==GL_DEPTH_COMPONENT)
	{
		// depthmap -> init with no data
		glTexImage2D(GL_TEXTURE_2D,0,glformat,width,height,0,glformat,gltype,pixels);
	}
	else
	if(cubeOr2d==GL_TEXTURE_CUBE_MAP)
	{
		// cube -> init with data
		for(unsigned side=0;side<6;side++)
		{
			unsigned char* sideData = pixels?pixels+side*width*height*bytesPerPixel:NULL;
			if(buildMipmaps)
				gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,glformat,width,height,glformat,gltype,sideData);
			else
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,glformat,width,height,0,glformat,gltype,sideData);
		}
	}
	else
	{
		// 2d -> init with data
		if(buildMipmaps)
			gluBuild2DMipmaps(GL_TEXTURE_2D,glformat,width,height,glformat,gltype,pixels);
		else
			glTexImage2D(GL_TEXTURE_2D,0,glformat,width,height,0,glformat,gltype,pixels);
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
		unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 );
		// find side
		unsigned side = 2*axis + ((d[axis]<0)?1:0);
		// find xy
		d[0] = direction[0] / direction[axis];
		d[1] = direction[1] / direction[axis];
		d[2] = direction[2] / direction[axis];
		float xy[2] = {d[axis?0:1],d[(axis<2)?2:1]}; // -1..1 range
		//!!! pro ruzny strany mozna prohodit x<->y nebo negovat
		xy[0] = (xy[0]+1)*(0.5f*width); // 0..size range
		xy[1] = (xy[1]+1)*(0.5f*height); // 0..size range
		unsigned x = (unsigned) CLAMPED((int)xy[0],0,(int)width-1);
		unsigned y = (unsigned) CLAMPED((int)xy[1],0,(int)height-1);
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
	assert(ofs*bytesPerPixel<bytesTotal);
	ofs *= channels;
	switch(format)
	{
		case TF_RGB:
			rgba[0] = pixels[ofs+((channels==1)?0:0)]/255.0f;
			rgba[1] = pixels[ofs+((channels==1)?0:1)]/255.0f;
			rgba[2] = pixels[ofs+((channels==1)?0:2)]/255.0f;
			rgba[3] = 1;
			break;
		case TF_RGBA:
			rgba[0] = pixels[ofs+((channels==1)?0:0)]/255.0f;
			rgba[1] = pixels[ofs+((channels==1)?0:1)]/255.0f;
			rgba[2] = pixels[ofs+((channels==1)?0:2)]/255.0f;
			rgba[3] = pixels[ofs+((channels==1)?0:3)]/255.0f;
			break;
		case TF_RGBF:
			rgba[0] = ((float*)pixels)[ofs+((channels==1)?0:0)];
			rgba[1] = ((float*)pixels)[ofs+((channels==1)?0:1)];
			rgba[2] = ((float*)pixels)[ofs+((channels==1)?0:2)];
			rgba[3] = 1;
			break;
		case TF_RGBAF:
			rgba[0] = ((float*)pixels)[ofs+((channels==1)?0:0)];
			rgba[1] = ((float*)pixels)[ofs+((channels==1)?0:1)];
			rgba[2] = ((float*)pixels)[ofs+((channels==1)?0:2)];
			rgba[3] = ((float*)pixels)[ofs+((channels==1)?0:3)];
			break;
	}
	return true;
}

static FBO* fbo = NULL;

bool TextureGL::renderingToBegin(unsigned side)
{
	if(!fbo) fbo = new FBO();
	if(side>=6 || (cubeOr2d!=GL_TEXTURE_CUBE_MAP && side))
	{
		assert(0);
		return false;
	}
	return fbo->setRenderTarget(id,0,(cubeOr2d==GL_TEXTURE_CUBE_MAP)?GL_TEXTURE_CUBE_MAP_POSITIVE_X+side:GL_TEXTURE_2D);
}

void TextureGL::renderingToEnd()
{
	fbo->restoreDefaultRenderTarget();
}

TextureGL::~TextureGL()
{
	glDeleteTextures(1, &id);
	delete[] pixels;

	numInstances--;
	if(!numInstances)
	{
		delete fbo;
		fbo = NULL;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::create(unsigned char *data, int width, int height, bool cube, int type,int mag,int min,int wrapS,int wrapT)
{
	return new TextureGL(data,width,height,cube,type,mag,min,wrapS,wrapT);
}

}; // namespace
