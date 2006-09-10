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
// TextureFromDisk

class RR_API TextureFromDisk : public TextureGL
{
public:
	class xFileNotFound {};
	class xNotSuchFormat {};
	class xNotASupportedFormat {};

	TextureFromDisk(char *filename, int mag=GL_LINEAR, int min = GL_LINEAR_MIPMAP_LINEAR,
		int wrapS = GL_REPEAT, int wrapT = GL_REPEAT);
protected:
	void changeNameConvention(char *filename) const;
	unsigned char *loadData(char *filename);
	//unsigned char *loadJpeg(char *filename);
	//unsigned char *decodeJpeg(struct jpeg_decompress_struct *cinfo);
	unsigned char *loadBmp(char *filename);
	unsigned char *loadBmpData(FILE *file);
	unsigned char *loadTga(char *filename);
	void checkFileOpened(FILE *file);

	const static char *JPG_EXT;
	const static char *BMP_EXT;
};

TextureFromDisk::TextureFromDisk(char *filename, int mag, int min, int wrapS, int wrapT)
	: TextureGL(NULL,1,1,GL_RGB)
{
	unsigned int type;

	pixels = loadData(filename);

	glGenTextures(1, &id);

	bindTexture();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	type = (channels == 3) ? GL_RGB : GL_RGBA;
	// Very strange : before the first type was channels. If don't work try
	// changing it back.
	gluBuild2DMipmaps(GL_TEXTURE_2D, type, width, height, type, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

void TextureFromDisk::changeNameConvention(char *filename) const
{
	while(*filename)
	{
		if(*filename == '\\')
			*filename = '/';
		filename++;
	}
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
		data = loadBmp(name);
	}
	catch(xNotSuchFormat e)
	{
		try
		{
			data = loadTga(name);
		}      
		catch(xNotSuchFormat e)
		{
			/*  try
			{
			data = loadJpeg(filename);
			}
			catch(xNotSuchFormat e)*/
			{
				throw xNotASupportedFormat();
			}
			e=e;
		}
		e=e;
	}
	pixels = data;
	return data;
}

/*unsigned char *Texture::loadJpeg(char *filename)
{
struct jpeg_decompress_struct cinfo;
unsigned char *data;
FILE *file = fopen(filename, "rb");

checkFileOpened(file); 
jpeg_error_mgr jerr;
cinfo.err = jpeg_std_error(&jerr);
jpeg_create_decompress(&cinfo);
jpeg_stdio_src(&cinfo, file);

data = decodeJpeg(&cinfo);

jpeg_destroy_decompress(&cinfo);
fclose(file);

return data; 
}

unsigned char *Texture::decodeJpeg(struct jpeg_decompress_struct *cinfo)
{
unsigned char *data;
if(!jpeg_read_header(cinfo, TRUE))
throw xNotSuchFormat();

jpeg_start_decompress(cinfo);

channels = cinfo->num_components;
width = cinfo->image_width;
height = cinfo->image_height;

int rowSpan = cinfo->image_width * cinfo->num_components;

data = new unsigned char[rowSpan * height];
unsigned char** rowPtr = new unsigned char*[height];

for (int i = 0; i < height; i++)
rowPtr[i] = &(data[i * rowSpan]);

int rowsRead = 0;
while (cinfo->output_scanline < cinfo->output_height) 
{
rowsRead += jpeg_read_scanlines(cinfo, 
&rowPtr[rowsRead], cinfo->output_height - rowsRead);
}
delete [] rowPtr;
jpeg_finish_decompress(cinfo);
return data;
}*/

unsigned char *TextureFromDisk::loadBmp(char *filename)
{
	short numBits;
	int i, num, fileSize, headerSize;
	unsigned char h1, h2;
	FILE *file = fopen(filename, "rb");

	channels = 3;

	checkFileOpened(file);
	fread(&h1, sizeof(h1), 1, file);
	fread(&h2, sizeof(h2), 1, file);
	if(h1 != 'B' || h2 != 'M')
	{
		fclose(file);
		throw xNotSuchFormat();
	}

	fread(&fileSize, sizeof(fileSize), 1, file);
	fread(&num, sizeof(num), 1, file);
	fread(&headerSize, sizeof(headerSize), 1, file);
	fread(&num, sizeof(num), 1, file);

	fread(&width, sizeof(width), 1, file);
	fread(&height, sizeof(height), 1, file);
	fread(&numBits, sizeof(numBits), 1, file);
	fread(&numBits, sizeof(numBits), 1, file);
	if(numBits != 24)
	{
		fclose(file);
		throw xNotASupportedFormat();
	}
	for(i = 0; i < 6; i++)
		fread(&num, sizeof(num), 1, file);

	return loadBmpData(file);
}

unsigned char *TextureFromDisk::loadBmpData(FILE *file)
{
	int i, j;
	unsigned char temp;
	unsigned char bred, bblue, bgreen;
	unsigned char *data, *pTemp;

	data = new unsigned char[width * height * channels];

	pTemp = data;
	for(i = 0; i < height; i++)
	{
		for(j = 0; j < width; j++)
		{
			fread(&bred, sizeof(bred), 1, file);
			fread(&bgreen, sizeof(bgreen), 1, file);
			fread(&bblue, sizeof(bblue), 1, file);

			*pTemp++ = bblue;
			*pTemp++ = bgreen;
			*pTemp++ = bred;
		}
		for(j = 0; j < width % 4; j++)
			fread(&temp, sizeof(temp), 1, file);
	}
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

	checkFileOpened(pFile);  

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

void TextureFromDisk::checkFileOpened(FILE *file)
{
	if(!file)
		throw xFileNotFound();
}

const char *TextureFromDisk::JPG_EXT = ".jpg";
const char *TextureFromDisk::BMP_EXT = ".bmp";


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::create(unsigned char *data, int width, int height, int type,int mag,int min,int wrapS,int wrapT)
{
	return new TextureGL(data,width,height,type,mag,min,wrapS,wrapT);
}

Texture* Texture::load(char *filename,int mag,int min,int wrapS,int wrapT)
{
	try {
		return new TextureFromDisk(filename,mag,min,wrapS,wrapT);
	} catch(...) {
		return NULL;
	}
}
