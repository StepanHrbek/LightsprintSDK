// --------------------------------------------------------------------------
// DemoEngine
// Texture implementation that loads image from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#define USE_FREEIMAGE // comment out to remove dependency on FreeImage (only .tga will be supported)

#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include "TextureFromDisk.h"
#ifdef USE_FREEIMAGE
#include "FreeImage.h"
#pragma comment(lib,"FreeImage.lib")
#endif

#define SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureFromDisk

// 2D map loader
TextureFromDisk::TextureFromDisk(
	const char *filename, bool flipV, bool flipH,
	int mag, int min, int wrapS, int wrapT)
	: TextureGL(NULL,1,1,false,GL_RGBA,mag,min,wrapS,wrapT)
{
	unsigned int type;

#ifdef USE_FREEIMAGE
	pixels = loadFreeImage(filename,false,flipV,flipH,width,height,channels);
#else
	pixels = loadData(filename,width,height,channels);
#endif
	if(!pixels) throw xFileNotFound();
	type = (channels == 3) ? GL_RGB : GL_RGBA;
	gluBuild2DMipmaps(GL_TEXTURE_2D, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
}

// cube map loader
TextureFromDisk::TextureFromDisk(
	const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH,
	int mag, int min)
: TextureGL(NULL,1,1,true,GL_RGBA,mag,min)
{
	unsigned int type;

	for(unsigned side=0;side<6;side++)
	{
		char buf[1000];
		_snprintf(buf,999,filenameMask,cubeSideName[side]);
		buf[999] = 0;
#ifdef USE_FREEIMAGE
		pixels = loadFreeImage(buf,true,flipV,flipH,width,height,channels);
#else
		pixels = loadData(buf,width,height,channels);
#endif
		if(!pixels) throw xFileNotFound();
		type = (channels == 3) ? GL_RGB : GL_RGBA;
		//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
		gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
		SAFE_DELETE_ARRAY(pixels);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// FreeImage

#ifdef USE_FREEIMAGE

unsigned char *TextureFromDisk::loadFreeImage(const char *filename,bool cube,bool flipV,bool flipH,unsigned& width,unsigned& height,unsigned& channels)
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	unsigned char* pixels = NULL;

	// check the file signature and deduce its format
	fif = FreeImage_GetFileType(filename, 0);
	// no signature? try to guess the file format from the file extension
	if(fif == FIF_UNKNOWN)
	{
		fif = FreeImage_GetFIFFromFilename(filename);
	}
	// check that the plugin has reading capabilities
	if(fif!=FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
	{
		// load the file
		FIBITMAP *dib = FreeImage_Load(fif, filename);
		if(dib)
		{
			// convert the file to 32bit BGRA
			dib = FreeImage_ConvertTo32Bits(dib);
			if(dib)
			{
				if(flipV)
					FreeImage_FlipVertical(dib);
				if(flipH)
					FreeImage_FlipHorizontal(dib);
				// read size
				width = FreeImage_GetWidth(dib);
				height = FreeImage_GetHeight(dib);
				channels = 4;
				// convert BGRA to RGBA
				pixels = new unsigned char[4*width*height];
				BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
				for(unsigned i=0;i<width*height;i++)
				{
#define PROCESS(a) a //CLAMPED((int)(a*2),0,255) // quake korekce
					pixels[4*i+0] = PROCESS(fipixels[4*i+2]);
					pixels[4*i+1] = PROCESS(fipixels[4*i+1]);
					pixels[4*i+2] = PROCESS(fipixels[4*i+0]);
					pixels[4*i+3] = fipixels[4*i+3];
				}
			}
			// cleanup
			FreeImage_Unload(dib);
		}
	}
	return pixels;
}

bool TextureGL::save(const char *filename, const char* cubeSideName[6])
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	BOOL bSuccess = FALSE;

	// default cube side names
	const char* cubeSideNameBackup[6] = {"0","1","2","3","4","5"};
	if(!cubeSideName)
		cubeSideName = cubeSideNameBackup;

	FIBITMAP* dib = FreeImage_Allocate(getWidth(),getHeight(),32);
	if(dib)
	{
		BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
		if(fipixels)
		{
			// process all sides
			for(unsigned side=0;side<6;side++)
			{
				if(!side || cubeOr2d==GL_TEXTURE_CUBE_MAP)
				{
					// every one image must succeed
					bSuccess = false;
					// fill it with texture data
					if(renderingToBegin(side))
					{
						glReadPixels(0,0,getWidth(),getHeight(),GL_BGRA,GL_UNSIGNED_BYTE,fipixels);
						renderingToEnd();
						// convert RGBA to BGRA
						//for(unsigned i=0;i<width*height;i++)
						//{
						//	BYTE tmp = fipixels[4*i+0];
						//	fipixels[4*i+0] = fipixels[4*i+2];
						//	fipixels[4*i+2] = tmp;
						//}
						// try to guess the file format from the file extension
						fif = FreeImage_GetFIFFromFilename(filename);
						if(fif != FIF_UNKNOWN )
						{
							// check that the plugin has sufficient writing and export capabilities ...
							WORD bpp = FreeImage_GetBPP(dib);
							if(FreeImage_FIFSupportsWriting(fif) && FreeImage_FIFSupportsExportBPP(fif, bpp))
							{
								// generate single side filename
								char filenameCube[1000];
								_snprintf(filenameCube,999,filename,cubeSideName[side]);
								filenameCube[999] = 0;
								// ok, we can save the file
								bSuccess = FreeImage_Save(fif, dib, filenameCube);
								// if any one of 6 images fails, don't try other and report fail
								if(!bSuccess) break;
							}
						}
					}
				}
			}
		}
		FreeImage_Unload(dib);
	}
	return (bSuccess == TRUE) ? true : false;
}

#else // !USE_FREEIMAGE

/////////////////////////////////////////////////////////////////////////////
//
// TGA

unsigned char *TextureFromDisk::loadData(const char *filename,unsigned& width,unsigned& height,unsigned& channels)
{
	// opens tga instead of jpg
	char name[1000];
	strncpy(name,filename,999);
	size_t len = strlen(name);
	if(len>3) strcpy(name+len-3,"tga");

	unsigned char *data;
	try
	{
		data = loadTga(name,width,height,channels);
	}      
	catch(xNotSuchFormat e)
	{
		throw xNotASupportedFormat();
		e=e;
	}
	return data;
}

unsigned char *TextureFromDisk::loadTga(const char *filename,unsigned& width,unsigned& height,unsigned& channels)
{
	unsigned char TGA_RGB = 2, TGA_A = 3, TGA_RLE = 10;

	unsigned char *data;
	unsigned char length = 0;
	unsigned char imageType = 0;
	unsigned char bits = 0;
	FILE *pFile = fopen(filename, "rb");
	unsigned stride = 0;
	unsigned i = 0;
	unsigned short tmp;

	width = 0;
	height = 0;
	channels = 0;

	if(!pFile) throw xFileNotFound();

	fread(&length, sizeof(unsigned char), 1, pFile);
	fseek(pFile,1,SEEK_CUR); 
	fread(&imageType, sizeof(unsigned char), 1, pFile);
	fseek(pFile, 9, SEEK_CUR); 

	fread(&tmp, 2, 1, pFile); width = tmp;
	fread(&tmp, 2, 1, pFile); height = tmp;
	fread(&bits, 1, 1, pFile);

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
			for(unsigned y = 0; y < height; y++)
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
			for(unsigned i = 0; i < width*height; i++)
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

	unsigned char temp;
	for(unsigned i = 0; i < height / 2; i++)
		for(unsigned j = 0; j < width*channels; j++)
		{
			temp = data[i * width*channels + j];
			data[i*width*channels + j] = data[(height-1-i)*width*channels + j];
			data[(height-1-i) * width*channels + j] = temp;
		}

	return data;
}

bool TextureGL::save(const char *filename)
{
	return false;
}

#endif


/////////////////////////////////////////////////////////////////////////////
//
// Texture

Texture* Texture::load(const char *filename,const char* cubeSideName[6],bool flipV,bool flipH,int mag,int min,int wrapS,int wrapT)
{
	try 
	{
		if(cubeSideName)
			return new TextureFromDisk(filename,cubeSideName,flipV,flipH,mag,min);
		else
			return new TextureFromDisk(filename,flipV,flipH,mag,min,wrapS,wrapT);
	}
	catch(...) 
	{
		return NULL;
	}
}

}; // namespace
