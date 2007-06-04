// --------------------------------------------------------------------------
// DemoEngine
// Texture implementation that loads image from disk.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <GL/glew.h>
#include "TextureFromDisk.h"
#include "FreeImage.h"
#pragma comment(lib,"FreeImage.lib")

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
	: TextureGL(NULL,1,1,false,TF_RGBA,mag,min,wrapS,wrapT)
{
	SAFE_DELETE_ARRAY(pixels);

	cubeOr2d = GL_TEXTURE_2D;

	pixels = loadFreeImage(filename,false,flipV,flipH,width,height,format);
	if(!pixels) throw xFileNotFound();

	TextureGL::reset(width,height,format,pixels,true);

	//unsigned int type;
	//type = (channels == 3) ? GL_RGB : GL_RGBA;
	//gluBuild2DMipmaps(GL_TEXTURE_2D, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
}

// cube map loader
TextureFromDisk::TextureFromDisk(
	const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH,
	int magn, int mini)
: TextureGL(NULL,1,1,true,TF_RGBA,magn,mini)
{
	SAFE_DELETE_ARRAY(pixels);

	cubeOr2d = GL_TEXTURE_CUBE_MAP;

	bool sixFiles = filenameMask && strstr(filenameMask,"%s");
	if(!sixFiles)
	{
		// LOAD PIXELS FROM SINGLE FILE.HDR
		pixels = loadFreeImage(filenameMask,false,flipV,flipH,width,height,format);
		if(!pixels) throw xFileNotFound();
		shuffleCrossToCube(pixels,width,height,getBytesPerPixel(format));

		// GLU autogenerate mipmaps breaks float textures, disable mipmaps for float textures
		if((format==TF_RGBF || format==TF_RGBAF) && mini==GL_LINEAR_MIPMAP_LINEAR)
		{
			mini=GL_LINEAR;
			glTexParameteri(cubeOr2d, GL_TEXTURE_MIN_FILTER, mini);
		}
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
			Format tmpFormat;

			sides[side] = loadFreeImage(buf,true,flipV,flipH,tmpWidth,tmpHeight,tmpFormat);
			if(!sides[side]) throw xFileNotFound();

			if(!side)
			{
				width = tmpWidth;
				height = tmpHeight;
				format = tmpFormat;
			}
			else
			{
				if(tmpWidth!=width || tmpHeight!=height || tmpFormat!=format || width!=height)
					throw xFileNotFound();
			}
			//unsigned int type;
			//type = (channels == 3) ? GL_RGB : GL_RGBA;
			//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
			//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
		}

		// pack 6 images into 1 array
		// RGBA is expected here - warning: not satisfied when loading cube with 6 files and 96bit pixels
		pixels = new unsigned char[width*height*getBytesPerPixel(format)*6];
		for(unsigned side=0;side<6;side++)
		{
			memcpy(pixels+width*height*getBytesPerPixel(format)*side,sides[side],width*height*getBytesPerPixel(format));
			SAFE_DELETE_ARRAY(sides[side]);
		}
	}

	// load cube from 1 array
	TextureGL::reset(width,height,format,pixels,mini==GL_LINEAR_MIPMAP_LINEAR);
}


/////////////////////////////////////////////////////////////////////////////
//
// FreeImage

unsigned char *TextureFromDisk::loadFreeImage(const char *filename,bool cube,bool flipV,bool flipH,unsigned& width,unsigned& height,Format& format)
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
				format = TF_RGBF;
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
					format = TF_RGBA;
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
	BackbufferSaver() : TextureGL(NULL,1,1,false,TF_RGBA)
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
