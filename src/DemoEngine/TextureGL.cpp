#include <stdio.h>
#include <string.h>
#include <GL/glew.h>
#include "TextureGL.h"
#include "FBO.h"

/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

unsigned TextureGL::numInstances = 0;

TextureGL::TextureGL(unsigned char *data, int awidth, int aheight, bool acube, int type,
				 int mag, int min, int wrapS, int wrapT)
{
	numInstances++;

	width = awidth;
	height = aheight;
	cubeOr2d = acube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
	channels = (type == GL_RGB) ? 3 : 4;
	if(!data && type!=GL_DEPTH_COMPONENT) data = new unsigned char[width*height*channels]; //!!! eliminovat
	pixels = data;
	glGenTextures(1, &id);

	glBindTexture(cubeOr2d, id);
	if(type==GL_DEPTH_COMPONENT)
	{
		// depthmap -> init with no data
		glTexImage2D(GL_TEXTURE_2D,0,type,width,height,0,type,GL_UNSIGNED_BYTE,NULL);
	}
	else
	if(acube)
	{
		// cube -> don't init
	}
	else
	{
		// 2d -> init with data, build mipmaps
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		gluBuild2DMipmaps(GL_TEXTURE_2D, channels, width, height, type, GL_UNSIGNED_BYTE, pixels);
	}

	glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, min); 
	glTexParameteri(cubeOr2d, GL_TEXTURE_MAG_FILTER, mag); 

	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(cubeOr2d, GL_TEXTURE_WRAP_T, wrapT);
}

void TextureGL::bindTexture() const
{
	glBindTexture(cubeOr2d, id);
}

bool TextureGL::getPixel(float x01, float y01, float* rgb) const
{
	if(!pixels) return false;
	unsigned x = unsigned(x01 * (width)) % width;
	unsigned y = unsigned(y01 * (height)) % height;
	unsigned ofs = (x+y*width)*channels;
	rgb[0] = pixels[ofs+((channels==1)?0:0)]/255.0f;
	rgb[1] = pixels[ofs+((channels==1)?0:1)]/255.0f;
	rgb[2] = pixels[ofs+((channels==1)?0:2)]/255.0f;
	return true;
}

static FBO* fbo = NULL;

void TextureGL::renderingToBegin()
{
	if(!fbo) fbo = new FBO();
	fbo->setRenderTarget(id,0);
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
