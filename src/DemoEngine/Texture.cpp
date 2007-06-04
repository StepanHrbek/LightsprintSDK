// --------------------------------------------------------------------------
// DemoEngine
// Generic texture code: load & save using FreeImage.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include "Lightsprint/DemoEngine/Texture.h"
#include "FreeImage.h"
#pragma comment(lib,"FreeImage.lib")

namespace de
{

/////////////////////////////////////////////////////////////////////////////
//
// Texture

unsigned Texture::getBytesPerPixel(Texture::Format format)
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

/////////////////////////////////////////////////////////////////////////////
//
// FreeImage

unsigned char* loadFreeImage(const char *filename,bool cube,bool flipV,bool flipH,unsigned& width,unsigned& height,Texture::Format& format)
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
				format = Texture::TF_RGBF;
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
					format = Texture::TF_RGBA;
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

/////////////////////////////////////////////////////////////////////////////
//
// Texture load
//
// Universal code that loads texture into system memory, using FreeImage,
// and then calls virtual Texture::reset().
// Handles cube textures in 1 or 6 images.

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
bool reload2d(Texture* texture, const char *filename, bool flipV, bool flipH, bool buildMipmaps)
{
	unsigned width = 0;
	unsigned height = 0;
	Texture::Format format = Texture::TF_NONE;
	unsigned char* pixels = loadFreeImage(filename,false,flipV,flipH,width,height,format);
	if(!pixels)
	{
		return false;
	}
	else
	{
		texture->reset(width,height,format,pixels,buildMipmaps);
		return true;
	}
}

// cube map loader
bool reloadCube(Texture* texture, const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH, bool buildMipmaps)
{
	unsigned width = 0;
	unsigned height = 0;
	Texture::Format format = Texture::TF_NONE;
	unsigned char* pixels = NULL;
	bool sixFiles = filenameMask && strstr(filenameMask,"%s");
	if(!sixFiles)
	{
		// LOAD PIXELS FROM SINGLE FILE.HDR
		pixels = loadFreeImage(filenameMask,false,flipV,flipH,width,height,format);
		if(!pixels) return false;
		shuffleCrossToCube(pixels,width,height,Texture::getBytesPerPixel(format));
	}
	else
	{
		// LOAD PIXELS FROM SIX FILES
		unsigned char* sides[6] = {NULL,NULL,NULL,NULL,NULL,NULL};
		for(unsigned side=0;side<6;side++)
		{
			char buf[1000];
			_snprintf(buf,999,filenameMask,cubeSideName[side]);
			buf[999] = 0;

			unsigned tmpWidth, tmpHeight;
			Texture::Format tmpFormat;

			sides[side] = loadFreeImage(buf,true,flipV,flipH,tmpWidth,tmpHeight,tmpFormat);
			if(!sides[side])
				return false;

			if(!side)
			{
				width = tmpWidth;
				height = tmpHeight;
				format = tmpFormat;
			}
			else
			{
				if(tmpWidth!=width || tmpHeight!=height || tmpFormat!=format || width!=height)
					return false;
			}
			//unsigned int type;
			//type = (channels == 3) ? GL_RGB : GL_RGBA;
			//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
			//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
		}

		// pack 6 images into 1 array
		// RGBA is expected here - warning: not satisfied when loading cube with 6 files and 96bit pixels
		pixels = new unsigned char[width*height*Texture::getBytesPerPixel(format)*6];
		for(unsigned side=0;side<6;side++)
		{
			memcpy(pixels+width*height*Texture::getBytesPerPixel(format)*side,sides[side],width*height*Texture::getBytesPerPixel(format));
			SAFE_DELETE_ARRAY(sides[side]);
		}
	}

	// load cube from 1 array
	texture->reset(width,height,format,pixels,buildMipmaps);
	return true;
}

bool Texture::reload(const char *filename,const char* cubeSideName[6],bool flipV,bool flipH,bool buildMipmaps)
{
	bool reloaded = cubeSideName
		? reloadCube(this,filename,cubeSideName,flipV,flipH,buildMipmaps)
		: reload2d(this,filename,flipV,flipH,buildMipmaps);
	if(!reloaded)
	{
		printf("Failed to load %s.\n",filename);
	}
	return reloaded;
}


/////////////////////////////////////////////////////////////////////////////
//
// Texture save

bool Texture::save(const char *filename, const char* cubeSideName[6])
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	BOOL bSuccess = FALSE;

	// default cube side names
	const char* cubeSideNameBackup[6] = {"0","1","2","3","4","5"};
	if(!cubeSideName)
		cubeSideName = cubeSideNameBackup;

	unsigned bytesPerPixel = getBytesPerPixel(getFormat());
	const unsigned char* rawData = lock();
	if(rawData)
	{
		FIBITMAP* dib = FreeImage_Allocate(getWidth(),getHeight(),bytesPerPixel*8);
		if(dib)
		{
			BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
			if(fipixels)
			{
				// process all sides
				for(unsigned side=0;side<6;side++)
				{
					if(!side || isCube())
					{
						// every one image must succeed
						bSuccess = false;
						// fill it with texture data
						memcpy(fipixels,rawData+side*getWidth()*getHeight()*bytesPerPixel,getWidth()*getHeight()*bytesPerPixel);
						// try to guess the file format from the file extension
						fif = FreeImage_GetFIFFromFilename(filename);
						if(fif!=FIF_UNKNOWN && FreeImage_FIFSupportsWriting(fif))
						{
							// generate single side filename
							char filenameCube[1000];
							_snprintf(filenameCube,999,filename,cubeSideName[side]);
							filenameCube[999] = 0;
							// write native number of bits
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
								// can't write native bits, try 24bit (e.g. jpg can't write 32bit but can 24bit)
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
			FreeImage_Unload(dib);
		}
		unlock();
	}

	return (bSuccess == TRUE) ? true : false;
}

}; // namespace
