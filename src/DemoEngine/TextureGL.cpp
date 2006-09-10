#include <iostream>
#include <stdio.h>
#include <string.h>
//#include <jpeglib.h>
#include <GL/glew.h>
#include "TextureGL.h"


/////////////////////////////////////////////////////////////////////////////
//
// TextureGL

TextureGL::TextureGL(unsigned char *data, int nWidth, int nHeight, int nType,
				 int mag, int min, int wrapS, int wrapT)
{
	width = nWidth;
	height = nHeight;
	unsigned int type = nType;
	channels = (type == GL_RGB) ? 3 : 4;
	if(!data) data = new unsigned char[nWidth*nHeight*channels]; //!!! eliminovat
	pixels = data;
	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gluBuild2DMipmaps(GL_TEXTURE_2D, channels, width, height, type, GL_UNSIGNED_BYTE, pixels); //!!! eliminovat
	//glTexImage2D(GL_TEXTURE_2D,0,type,width,height,0,type,GL_UNSIGNED_BYTE,pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag); 

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

TextureGL::~TextureGL()
{
	glDeleteTextures(1, &id);
	delete[] pixels;
}

void TextureGL::bindTexture() const
{
	glBindTexture(GL_TEXTURE_2D, id);
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


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::create(unsigned char *data, int width, int height, int type,int mag,int min,int wrapS,int wrapT)
{
	return new TextureGL(data,width,height,type,mag,min,wrapS,wrapT);
}
