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

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// TextureFromDisk

void shuffleBlock(unsigned char*& dst, const unsigned char* pixelsOld, unsigned iofs, unsigned jofs, unsigned blockWidth, unsigned blockHeight, unsigned widthOld, unsigned bytesPerPixel, bool flip=false)
{
	if(flip)
		for(unsigned j=blockHeight;j--;)
		{
			for(unsigned i=blockWidth;i--;)
			{
				memcpy(dst,pixelsOld+((jofs+j)*widthOld+iofs+i)*bytesPerPixel,bytesPerPixel);
				dst += bytesPerPixel;
			}
		}
	else
		for(unsigned j=0;j<blockHeight;j++)
		{
			memcpy(dst,pixelsOld+((jofs+j)*widthOld+iofs)*bytesPerPixel,blockWidth*bytesPerPixel);
			dst += blockWidth*bytesPerPixel;
		}
}

void shuffleCrossToCube(unsigned char*& pixelsOld, unsigned& widthOld, unsigned& heightOld, unsigned bytesPerPixel)
{
	// alloc new
	unsigned widthNew = MIN(widthOld,heightOld)/3;
	unsigned heightNew = MAX(widthOld,heightOld)/4;
	unsigned char* pixelsNew = new unsigned char[widthNew*heightNew*bytesPerPixel*6];

	// shuffle from old to new
	unsigned char* dst = pixelsNew;
	shuffleBlock(dst,pixelsOld,widthNew*2,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X+
	shuffleBlock(dst,pixelsOld,widthNew*0,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X-
	shuffleBlock(dst,pixelsOld,widthNew*1,0*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y+
	shuffleBlock(dst,pixelsOld,widthNew*1,2*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y-
	shuffleBlock(dst,pixelsOld,widthNew*1,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Z+
	if(widthOld>heightOld)
		shuffleBlock(dst,pixelsOld,widthNew*3,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Z-
	else
		shuffleBlock(dst,pixelsOld,widthNew*1,3*heightNew,widthNew,heightNew,widthOld,bytesPerPixel,true); // Z-

	// replace old
	delete[] pixelsOld;
	pixelsOld = pixelsNew;
	widthOld = widthNew;
	heightOld = heightNew;
}

// 2D map loader
TextureFromDisk::TextureFromDisk(
	const char *filename, bool flipV, bool flipH,
	int mag, int min, int wrapS, int wrapT)
	: TextureGL(NULL,1,1,false,GL_RGBA,mag,min,wrapS,wrapT)
{
	SAFE_DELETE_ARRAY(pixels);

	cubeOr2d = GL_TEXTURE_2D;

#ifdef USE_FREEIMAGE
	pixels = loadFreeImage(filename,false,flipV,flipH,width,height,channels);
#else
	pixels = loadData(filename,width,height,channels);
#endif
	if(!pixels) throw xFileNotFound();

	TextureGL::reset(width,height,(channels==1)?TF_NONE:((channels==3)?TF_RGBF:TF_RGBA),pixels,true);

	//unsigned int type;
	//type = (channels == 3) ? GL_RGB : GL_RGBA;
	//gluBuild2DMipmaps(GL_TEXTURE_2D, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
}

// cube map loader
TextureFromDisk::TextureFromDisk(
	const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH,
	int magn, int mini)
: TextureGL(NULL,1,1,true,GL_RGBA,magn,mini)
{
	SAFE_DELETE_ARRAY(pixels);

	cubeOr2d = GL_TEXTURE_CUBE_MAP;

#ifdef USE_FREEIMAGE
	bool sixFiles = filenameMask && strstr(filenameMask,"%s");
	if(!sixFiles)
	{
		// LOAD PIXELS FROM SINGLE FILE.HDR
		pixels = loadFreeImage(filenameMask,false,flipV,flipH,width,height,channels);
		if(!pixels) throw xFileNotFound();
		shuffleCrossToCube(pixels,width,height,(channels==3)?12:4);
		format = (channels==1)?TF_NONE:((channels==3)?TF_RGBF:TF_RGBA);

		// GLU autogenerate mipmaps breaks float textures, disable mipmaps for float textures
		if(channels==3 && mini==GL_LINEAR_MIPMAP_LINEAR)
		{
			mini=GL_LINEAR;
			glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, mini);
		}
	}
	else
#endif
	{
		// LOAD PIXELS FROM SIX FILES
		unsigned char* sides[6] = {NULL,NULL,NULL,NULL,NULL,NULL};
		for(unsigned side=0;side<6;side++)
		{
			char buf[1000];
			_snprintf(buf,999,filenameMask,cubeSideName[side]);
			buf[999] = 0;

			unsigned tmpWidth, tmpHeight, tmpChannels;

#ifdef USE_FREEIMAGE
			sides[side] = loadFreeImage(buf,true,flipV,flipH,tmpWidth,tmpHeight,tmpChannels);
#else
			sides[side] = loadData(buf,tmpWidth,tmpHeight,tmpChannels);
#endif
			if(!sides[side]) throw xFileNotFound();

			if(!side)
			{
				width = tmpWidth;
				height = tmpHeight;
				channels = tmpChannels;
			}
			else
			{
				if(tmpWidth!=width || tmpHeight!=height || tmpChannels!=channels || width!=height)
					throw xFileNotFound();
			}
			//unsigned int type;
			//type = (channels == 3) ? GL_RGB : GL_RGBA;
			//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
			//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
		}

		// pack 6 images into 1 array
		// RGBA is expected here - warning: not satisfied when loading cube with 6 files and 96bit pixels
		pixels = new unsigned char[width*height*channels*6];
		for(unsigned side=0;side<6;side++)
		{
			memcpy(pixels+width*height*channels*side,sides[side],width*height*channels);
			SAFE_DELETE_ARRAY(sides[side]);
		}

		format = (channels==1)?TF_NONE:((channels==3)?TF_RGBF:TF_RGBA);
	}

	// load cube from 1 array
	TextureGL::reset(width,height,format,pixels,mini==GL_LINEAR_MIPMAP_LINEAR);
}


/////////////////////////////////////////////////////////////////////////////
//
// FreeImage

#ifdef USE_FREEIMAGE

// if it returns channels=4 -> 32bit RGBA
// if it returns channels=3 -> 96bit RGBF
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
		FIBITMAP* dib1 = FreeImage_Load(fif, filename);
		if(dib1)
		{
			unsigned bpp1 = FreeImage_GetBPP(dib1);
			if(bpp1==96)
			{
				// RGBF, conversion to 32bit doesn't work
				FreeImage_FlipVertical(dib1);
				// read size
				width = FreeImage_GetWidth(dib1);
				height = FreeImage_GetHeight(dib1);
				channels = 3;
				pixels = new unsigned char[12*width*height];
				BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib1);
				memcpy(pixels,fipixels,width*height*12);
			}
			else
			{
				// try conversion to 32bit BGRA
				FIBITMAP* dib2 = FreeImage_ConvertTo32Bits(dib1);
				if(dib2)
				{
					if(flipV)
						FreeImage_FlipVertical(dib2);
					if(flipH)
						FreeImage_FlipHorizontal(dib2);
					// read size
					width = FreeImage_GetWidth(dib2);
					height = FreeImage_GetHeight(dib2);
					channels = 4;
					// convert BGRA to RGBA
					pixels = new unsigned char[4*width*height];
					BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib2);
					unsigned pitch = FreeImage_GetPitch(dib2);
					for(unsigned j=0;j<height;j++)
					{
						for(unsigned i=0;i<width;i++)
						{
							pixels[j*4*width+4*i+0] = fipixels[j*pitch+4*i+2];
							pixels[j*4*width+4*i+1] = fipixels[j*pitch+4*i+1];
							pixels[j*4*width+4*i+2] = fipixels[j*pitch+4*i+0];
							pixels[j*4*width+4*i+3] = fipixels[j*pitch+4*i+3];
						}
					}
					// cleanup
					FreeImage_Unload(dib2);
				}
			}
			// cleanup
			FreeImage_Unload(dib1);
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
						if(fif!=FIF_UNKNOWN && FreeImage_FIFSupportsWriting(fif))
						{
							// generate single side filename
							char filenameCube[1000];
							_snprintf(filenameCube,999,filename,cubeSideName[side]);
							filenameCube[999] = 0;
							// write 32 or 24bit
							WORD bpp = FreeImage_GetBPP(dib);
							if(FreeImage_FIFSupportsExportBPP(fif, bpp))
							{
								// ok, we can save the file
								bSuccess = FreeImage_Save(fif, dib, filenameCube);
								// if any one of 6 images fails, don't try other and report fail
								if(!bSuccess) break;
							}
							else
							if(FreeImage_FIFSupportsExportBPP(fif, 24))
							{
								// can't write 32bit, try 24bit
								FIBITMAP* dib24 = FreeImage_ConvertTo24Bits(dib);
								// ok, we can save the file
								bSuccess = FreeImage_Save(fif, dib24, filenameCube);
								// cleanup
								FreeImage_Unload(dib24);
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

// Purpose of this class:
//  If you call save() on this texture, it will save backbuffer.
class BackbufferSaver : public TextureGL
{
public:
	BackbufferSaver() : TextureGL(NULL,1,1,false,GL_RGBA)
	{
		int rect[4];
		glGetIntegerv(GL_VIEWPORT,rect);
		width = rect[2];
		height = rect[3];
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
		printf("Failed to load %s.\n",filename);
		return NULL;
	}
}

}; // namespace
