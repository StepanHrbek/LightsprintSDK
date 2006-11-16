// --------------------------------------------------------------------------
// DemoEngine
// Texture implementation that loads image from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include "TextureFromDisk.h"


/////////////////////////////////////////////////////////////////////////////
//
// TextureFromDisk

TextureFromDisk::TextureFromDisk(char *filename, int mag, int min, int wrapS, int wrapT)
	: TextureGL(NULL,1,1,false,GL_RGB)
{
	unsigned int type;

	pixels = loadData(filename);

	glGenTextures(1, &id);

	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	type = (channels == 3) ? GL_RGB : GL_RGBA;
	gluBuild2DMipmaps(GL_TEXTURE_2D, type, width, height, type, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

unsigned char *TextureFromDisk::loadData(char *filename)
{
	// opens tga instead of jpg
	char name[1000];
	strncpy(name,filename,999);
	size_t len = strlen(name);
	if(len>3) strcpy(name+len-3,"tga");

	unsigned char *data;
	try
	{
		data = loadTga(name);
	}      
	catch(xNotSuchFormat e)
	{
		throw xNotASupportedFormat();
		e=e;
	}
	pixels = data;
	return data;
}

unsigned char *TextureFromDisk::loadTga(char *filename)
{
	unsigned char TGA_RGB = 2, TGA_A = 3, TGA_RLE = 10;

	unsigned char *data;
	short width = 0, height = 0;
	unsigned char length = 0;
	unsigned char imageType = 0;
	unsigned char bits = 0;
	FILE *pFile = fopen(filename, "rb");
	int channels = 0;
	int stride = 0;
	int i = 0;

	if(!pFile) throw xFileNotFound();

	fread(&length, sizeof(unsigned char), 1, pFile);
	fseek(pFile,1,SEEK_CUR); 
	fread(&imageType, sizeof(unsigned char), 1, pFile);
	fseek(pFile, 9, SEEK_CUR); 

	fread(&width,  sizeof(width), 1, pFile);
	fread(&height, sizeof(height), 1, pFile);
	fread(&bits,   sizeof(unsigned char), 1, pFile);

	fseek(pFile, length + 1, SEEK_CUR); 

	if(imageType != TGA_RGB && imageType != TGA_A && imageType != TGA_RLE)
	{
		fclose(pFile);
		throw xNotSuchFormat();
	}

	if(imageType != TGA_RLE)
	{
		if(bits == 24 || bits == 32)
		{
			channels = bits / 8;
			stride = channels * width;
			data = new unsigned char[stride * height];
			for(int y = 0; y < height; y++)
			{
				unsigned char *pLine = &(data[stride * y]);
				fread(pLine, stride, 1, pFile);
				for(i = 0; i < stride; i += channels)
				{
					int temp = pLine[i];
					pLine[i] = pLine[i + 2];
					pLine[i + 2] = temp;
				}
			}
		}
		else if(bits == 16)
		{
			unsigned short pixels = 0;
			int r=0, g=0, b=0;
			channels = 3;
			stride = channels * width;
			data = new unsigned char[stride * height];
			for(int i = 0; i < width*height; i++)
			{
				fread(&pixels, sizeof(unsigned short), 1, pFile);
				b = (pixels & 0x1f) << 3;
				g = ((pixels >> 5) & 0x1f) << 3;
				r = ((pixels >> 10) & 0x1f) << 3;

				data[i * 3 + 0] = r;
				data[i * 3 + 1] = g;
				data[i * 3 + 2] = b;
			}
		}	
		else
		{
			fclose(pFile);
			throw xNotSuchFormat();
		}
	}
	else
	{
		unsigned char rleID = 0;
		int colorsRead = 0;
		channels = bits / 8;
		stride = channels * width;

		data = new unsigned char[stride * height];
		unsigned char *pColors = new unsigned char[channels];

		while(i < width*height)
		{
			fread(&rleID, sizeof(unsigned char), 1, pFile);
			if(rleID < 128)
			{
				rleID++;
				while(rleID)
				{
					fread(pColors, sizeof(unsigned char) * channels, 1, pFile);

					data[colorsRead + 0] = pColors[2];
					data[colorsRead + 1] = pColors[1];
					data[colorsRead + 2] = pColors[0];

					if(bits == 32)
						data[colorsRead + 3] = pColors[3];

					i++;
					rleID--;
					colorsRead += channels;
				}
			}
			else
			{
				rleID -= 127;
				fread(pColors, sizeof(unsigned char) * channels, 1, pFile);
				while(rleID)
				{
					data[colorsRead + 0] = pColors[2];
					data[colorsRead + 1] = pColors[1];
					data[colorsRead + 2] = pColors[0];

					if(bits == 32)
						data[colorsRead + 3] = pColors[3];

					i++;
					rleID--;
					colorsRead += channels;
				}

			}
		}
	}

	fclose(pFile);

	this->channels = channels;
	this->width = width;
	this->height = height;

	unsigned char temp;
	for(int i = 0; i < height / 2; i++)
		for(int j = 0; j < width*channels; j++)
		{
			temp = data[i * width*channels + j];
			data[i*width*channels + j] = data[(height-1-i)*width*channels + j];
			data[(height-1-i) * width*channels + j] = temp;
		}

	return data;
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::load(char *filename,int mag,int min,int wrapS,int wrapT)
{
	try {
		return new TextureFromDisk(filename,mag,min,wrapS,wrapT);
	} catch(...) {
		return NULL;
	}
}
