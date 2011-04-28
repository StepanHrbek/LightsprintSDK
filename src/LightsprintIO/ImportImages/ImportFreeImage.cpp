// --------------------------------------------------------------------------
// Image load & save using FreeImage.
// Copyright (C) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_IMAGES

// This file is the only connection between Lightsprint and FreeImage.
// If you don't use LightsprintIO, add this .cpp to your project
// and textures from disk will be opened by FreeImage.
// Remove it from project and textures from disk won't be opened.

// You can use any other image library if you implement two simple callbacks,
// load and save, and call RRBuffer::registerLoader().

#define _CRT_SECURE_NO_WARNINGS
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "ImportFreeImage.h"
#include "FreeImage.h"

#pragma comment(lib,"FreeImage.lib")

using namespace rr;

static unsigned getBytesPerPixel(RRBufferFormat format)
{
	switch(format)
	{
		case BF_RGB: return 3;
		case BF_BGR: return 3;
		case BF_RGBA: return 4;
		case BF_RGBF: return 12;
		case BF_RGBAF: return 16;
		case BF_DEPTH: return 4;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
// FreeImage load

static unsigned char* loadFreeImage(const RRString& filename,bool flipV,bool flipH,unsigned& width,unsigned& height,RRBufferFormat& outFormat,bool& outScaled)
{
	// uncomment if you wish to skip loading from network
//	if (filename && filename[0]=='\\' && filename[1]=='\\') return NULL;

	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	unsigned char* pixels = NULL;

	// check the file signature and deduce its format
#ifdef _WIN32
	fif = FreeImage_GetFileTypeU(filename.w_str(), 0);
#else
	fif = FreeImage_GetFileType(filename.c_str(), 0);
#endif
	// no signature? try to guess the file format from the file extension
	if (fif == FIF_UNKNOWN)
	{
#ifdef _WIN32
		fif = FreeImage_GetFIFFromFilenameU(filename.w_str());
#else
		fif = FreeImage_GetFIFFromFilename(filename.c_str());
#endif
	}
	// check that the plugin has reading capabilities
	if (fif!=FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
	{
		// load the file
#ifdef _WIN32
		FIBITMAP* dib1 = FreeImage_LoadU(fif, filename.w_str());
#else
		FIBITMAP* dib1 = FreeImage_Load(fif, filename.c_str());
#endif
		if (dib1)
		{
			unsigned bpp1 = FreeImage_GetBPP(dib1);
			outScaled = bpp1<64; // high bpp images are usually in physical scale
			if (bpp1==128)
			{
				// RGBAF
				if (flipV)
					FreeImage_FlipVertical(dib1);
				if (flipH)
					FreeImage_FlipHorizontal(dib1);
				// read size
				width = FreeImage_GetWidth(dib1);
				height = FreeImage_GetHeight(dib1);
				outFormat = BF_RGBAF;
				pixels = new unsigned char[16*width*height];
				float* fipixels = (float*)FreeImage_GetBits(dib1);
				memcpy(pixels,fipixels,width*height*16);
			}
			else
			if (bpp1==96)
			{
				// RGBF, conversion to 32bit doesn't work
				if (flipV)
					FreeImage_FlipVertical(dib1);
				if (flipH)
					FreeImage_FlipHorizontal(dib1);
				// read size
				width = FreeImage_GetWidth(dib1);
				height = FreeImage_GetHeight(dib1);
				outFormat = BF_RGBF;
				pixels = new unsigned char[12*width*height];
				float* fipixels = (float*)FreeImage_GetBits(dib1);
				memcpy(pixels,fipixels,width*height*12);
			}
			else
			if (FreeImage_IsTransparent(dib1))
			{
				// try conversion to 32bit BGRA
				FIBITMAP* dib2 = FreeImage_ConvertTo32Bits(dib1);
				if (dib2)
				{
					if (flipV)
						FreeImage_FlipVertical(dib2);
					if (flipH)
						FreeImage_FlipHorizontal(dib2);
					// read size
					width = FreeImage_GetWidth(dib2);
					height = FreeImage_GetHeight(dib2);
					outFormat = BF_RGBA;
					// convert BGRA to RGBA
					pixels = new unsigned char[4*width*height];
					BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib2);
					unsigned pitch = FreeImage_GetPitch(dib2);
					for (unsigned j=0;j<height;j++)
					{
						for (unsigned i=0;i<width;i++)
						{
#ifdef RR_BIG_ENDIAN
							pixels[j*4*width+4*i+0] = fipixels[j*pitch+4*i+0];
							pixels[j*4*width+4*i+1] = fipixels[j*pitch+4*i+1];
							pixels[j*4*width+4*i+2] = fipixels[j*pitch+4*i+2];
							pixels[j*4*width+4*i+3] = fipixels[j*pitch+4*i+3];
#else
							pixels[j*4*width+4*i+0] = fipixels[j*pitch+4*i+2];
							pixels[j*4*width+4*i+1] = fipixels[j*pitch+4*i+1];
							pixels[j*4*width+4*i+2] = fipixels[j*pitch+4*i+0];
							pixels[j*4*width+4*i+3] = fipixels[j*pitch+4*i+3];
#endif
						}
					}
					// cleanup
					FreeImage_Unload(dib2);
				}
			}
			else
			{
				// try conversion to 24bit BGR
				FIBITMAP* dib2 = FreeImage_ConvertTo24Bits(dib1);
				if (dib2)
				{
					if (flipV)
						FreeImage_FlipVertical(dib2);
					if (flipH)
						FreeImage_FlipHorizontal(dib2);
					// read size
					width = FreeImage_GetWidth(dib2);
					height = FreeImage_GetHeight(dib2);
					outFormat = BF_RGB;
					// convert BGR to RGB
					pixels = new unsigned char[3*width*height];
					BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib2);
					unsigned pitch = FreeImage_GetPitch(dib2);
					for (unsigned j=0;j<height;j++)
					{
						for (unsigned i=0;i<width;i++)
						{
#ifdef RR_BIG_ENDIAN
							pixels[j*3*width+3*i+0] = fipixels[j*pitch+3*i+0];
							pixels[j*3*width+3*i+1] = fipixels[j*pitch+3*i+1];
							pixels[j*3*width+3*i+2] = fipixels[j*pitch+3*i+2];
#else
							pixels[j*3*width+3*i+0] = fipixels[j*pitch+3*i+2];
							pixels[j*3*width+3*i+1] = fipixels[j*pitch+3*i+1];
							pixels[j*3*width+3*i+2] = fipixels[j*pitch+3*i+0];
#endif
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
// RRBuffer load

static void shuffleBlock(unsigned char*& dst, const unsigned char* pixelsOld, unsigned iofs, unsigned jofs, unsigned blockWidth, unsigned blockHeight, unsigned widthOld, unsigned bytesPerPixel, bool flip=false)
{
	if (flip)
		for (unsigned j=blockHeight;j--;)
		{
			for (unsigned i=blockWidth;i--;)
			{
				memcpy(dst,pixelsOld+((jofs+j)*widthOld+iofs+i)*bytesPerPixel,bytesPerPixel);
				dst += bytesPerPixel;
			}
		}
	else
		for (unsigned j=0;j<blockHeight;j++)
		{
			memcpy(dst,pixelsOld+((jofs+j)*widthOld+iofs)*bytesPerPixel,blockWidth*bytesPerPixel);
			dst += blockWidth*bytesPerPixel;
		}
}

// Shuffles pixels in array so that cross shaped 3:4 or 4:3 2d image turns into 6 planes of cubemap.
// Returns true on success.
static bool shuffleCrossToCube(unsigned char*& pixelsOld, unsigned& widthOld, unsigned& heightOld, unsigned bytesPerPixel)
{
	// expected input is cross-shaped 3:4 or 4:3 image
	if (widthOld*3!=heightOld*4 && widthOld*4!=heightOld*3)
	{
		return false;
	}

	// alloc new
	unsigned widthNew = RR_MIN(widthOld,heightOld)/3;
	unsigned heightNew = RR_MAX(widthOld,heightOld)/4;
	unsigned char* pixelsNew = new unsigned char[widthNew*heightNew*bytesPerPixel*6];

	// shuffle from old to new
	bool wide = widthOld>heightOld;
	unsigned char* dst = pixelsNew;
	if (wide)
	{
		shuffleBlock(dst,pixelsOld,widthNew*3,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X+
		shuffleBlock(dst,pixelsOld,widthNew*1,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X-
		shuffleBlock(dst,pixelsOld,widthNew*2,0*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y+
		shuffleBlock(dst,pixelsOld,widthNew*2,2*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y-
		shuffleBlock(dst,pixelsOld,widthNew*2,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Z+
		shuffleBlock(dst,pixelsOld,widthNew*0,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Z-
	}
	else
	{
		shuffleBlock(dst,pixelsOld,widthNew*2,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X+
		shuffleBlock(dst,pixelsOld,widthNew*0,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // X-
		shuffleBlock(dst,pixelsOld,widthNew*1,0*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y+
		shuffleBlock(dst,pixelsOld,widthNew*1,2*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Y-
		shuffleBlock(dst,pixelsOld,widthNew*1,1*heightNew,widthNew,heightNew,widthOld,bytesPerPixel); // Z+
		shuffleBlock(dst,pixelsOld,widthNew*1,3*heightNew,widthNew,heightNew,widthOld,bytesPerPixel,true); // Z-
	}

	// replace old
	delete[] pixelsOld;
	pixelsOld = pixelsNew;
	widthOld = widthNew;
	heightOld = heightNew;
	return true;
}

struct VBUHeader
{
	RRBufferFormat format:16; // 3 bytes
	unsigned scaled:16; // 1 byte
	unsigned numVertices; // 4 bytes
	VBUHeader(RRBuffer* buffer)
	{
		memset(this,0,sizeof(*this));
		if (buffer)
		{
			format = buffer->getFormat();
			scaled = buffer->getScaled()?1:0;
			numVertices = buffer->getWidth();
		}
	}
	unsigned getDataSize()
	{
		return getBytesPerPixel(format) * numVertices;
	}
};

// vertex buffer loader
static bool reloadVertexBuffer(RRBuffer* texture, const RRString& filename)
{
	// open
#ifdef _WIN32
	FILE* f = _wfopen(filename.w_str(),L"rb");
#else
	FILE* f = fopen(filename.c_str(),"rb");
#endif
	if (!f) return false;
	// get filesize
	fseek(f,0,SEEK_END);
	unsigned datasize = ftell(f)-sizeof(VBUHeader);
	fseek(f,0,SEEK_SET);
	// read header
	VBUHeader header(NULL);
	fread(&header,sizeof(header),1,f);
	if (header.getDataSize()!=datasize)
	{
		fclose(f);
		return false;
	}
	// read data
	unsigned char* data = new unsigned char[datasize];
	fread(data,1,datasize,f);
	fclose(f);
	texture->reset(BT_VERTEX_BUFFER,header.numVertices,1,1,header.format,header.scaled?true:false,data);
	delete[] data;
	return true;
}

// 2D map loader
static bool reload2d(RRBuffer* texture, const RRString& filename)
{
	unsigned width = 0;
	unsigned height = 0;
	RRBufferFormat format = BF_DEPTH;
	bool scaled = true;
	unsigned char* pixels = loadFreeImage(filename,false,false,width,height,format,scaled);
	if (!pixels)
	{
		return false;
	}
	else
	{
		texture->reset(BT_2D_TEXTURE,width,height,1,format,scaled,pixels);
		delete[] pixels;
		return true;
	}
}

// cube map loader
static bool reloadCube(RRBuffer* texture, const RRString& filenameMask, const char* cubeSideName[6])
{
	unsigned width = 0;
	unsigned height = 0;
	RRBufferFormat format = BF_DEPTH;
	bool scaled = true;
	unsigned char* pixels = NULL;
	bool sixFiles = wcsstr(filenameMask.w_str(),L"%s")!=NULL;
	if (!sixFiles)
	{
		// LOAD PIXELS FROM SINGLE FILE.HDR
		pixels = loadFreeImage(filenameMask,true,true,width,height,format,scaled);
		if (!pixels)
			return false;
		if (!shuffleCrossToCube(pixels,width,height,getBytesPerPixel(format)))
		{
			// In early days, we returned only cubes.
			//delete[] pixels;
			//RRReporter::report(WARN,"%s is 2d image, not a cubemap. Hint: provide 3:4 or 4:3 image or sequence of 6 images.\n",filenameMask);
			//return false;

			// Today we return also 2d image, hoping that it is 360*180 degree panorama.
			texture->reset(BT_2D_TEXTURE,width,height,1,format,scaled,pixels);
			texture->flip(true,true,false);
			delete[] pixels;
			return true;
		}
	}
	else
	{
		// LOAD PIXELS FROM SIX FILES
		unsigned char* sides[6] = {NULL,NULL,NULL,NULL,NULL,NULL};
		for (unsigned side=0;side<6;side++)
		{
			std::wstring buf = RR_RR2STDW(filenameMask);
			buf.replace(buf.find(L"%s"),2,RRString(cubeSideName[side]).w_str());

			unsigned tmpWidth, tmpHeight;
			RRBufferFormat tmpFormat;

			sides[side] = loadFreeImage(RR_STDW2RR(buf),true,true,tmpWidth,tmpHeight,tmpFormat,scaled);
			if (!sides[side])
				return false;

			if (!side)
			{
				width = tmpWidth;
				height = tmpHeight;
				format = tmpFormat;
			}
			else
			{
				if (tmpWidth!=width || tmpHeight!=height || tmpFormat!=format || width!=height)
					return false;
			}
			//unsigned int type;
			//type = (channels == 3) ? GL_RGB : GL_RGBA;
			//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
			//gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X+side, type, width, height, type, GL_UNSIGNED_BYTE, pixels);
		}

		// pack 6 images into 1 array
		// RGBA is expected here - warning: not satisfied when loading cube with 6 files and 96bit pixels
		pixels = new unsigned char[width*height*getBytesPerPixel(format)*6];
		for (unsigned side=0;side<6;side++)
		{
			memcpy(pixels+width*height*getBytesPerPixel(format)*side,sides[side],width*height*getBytesPerPixel(format));
			RR_SAFE_DELETE_ARRAY(sides[side]);
		}
	}

	// load cube from 1 array
	texture->reset(BT_CUBE_TEXTURE,width,height,6,format,scaled,pixels);
	delete[] pixels;
	return true;
}

static RRBuffer* load(const RRString& filename, const char* cubeSideName[6])
{
	RRBuffer* buffer = RRBuffer::create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
	bool reloaded = (wcsstr(filename.w_str(),L".vbu") || wcsstr(filename.w_str(),L".VBU"))
		? reloadVertexBuffer(buffer,filename)
		: (cubeSideName
			? reloadCube(buffer,filename,cubeSideName)
			: reload2d(buffer,filename) );
	buffer->filename = filename;
	if (!reloaded)
		RR_SAFE_DELETE(buffer);
	return buffer;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer save

BOOL FIFSupportsExportBPP(FREE_IMAGE_FORMAT fif, int bpp)
{
	switch(bpp)
	{
		case 96: return FreeImage_FIFSupportsExportType(fif, FIT_RGBF);
		case 128: return FreeImage_FIFSupportsExportType(fif, FIT_RGBAF);
		default: return FreeImage_FIFSupportsExportBPP(fif, bpp);
	}
}

bool save(RRBuffer* buffer, const RRString& filename, const char* cubeSideName[6], const RRBuffer::SaveParameters* saveParameters)
{
	bool result = false;

	// preliminary, may change due to cubeSideName replacement
#ifdef _WIN32
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(filename.w_str());
#else
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(filename.c_str());
#endif

	// default cube side names
	const char* cubeSideNameBackup[6] = {"0","1","2","3","4","5"};
	if (!cubeSideName)
		cubeSideName = cubeSideNameBackup;

	const unsigned char* rawData = buffer->lock(BL_READ);
	if (rawData)
	{
		// save vertex buffer
		if (buffer->getType()==BT_VERTEX_BUFFER)
		{
			VBUHeader header(buffer);
#ifdef _WIN32
			FILE* f = _wfopen(filename.w_str(),L"wb");
#else
			FILE* f = fopen(filename.c_str(),"wb");
#endif
			if (f)
			{
				fwrite(&header,sizeof(header),1,f);
				unsigned written = (unsigned)fwrite(rawData,buffer->getElementBits()/8,buffer->getWidth(),f);
				fclose(f);
				result = written == buffer->getWidth();
			}
			goto ende;
		}

		// get src format
		unsigned srcbypp = getBytesPerPixel(buffer->getFormat());
		unsigned srcbipp = 8*srcbypp;
		FREE_IMAGE_TYPE fit;
		switch(buffer->getFormat())
		{
			case BF_RGB: fit = FIT_BITMAP; break;
			case BF_BGR: fit = FIT_BITMAP; break;
			case BF_RGBA: fit = FIT_BITMAP; break;
			case BF_RGBF: fit = FIT_RGBF; break;
			case BF_RGBAF: fit = FIT_RGBAF; break;
			default: RR_ASSERT(0); goto ende;
		}

		// select dst format
		unsigned tryTable[5][4] = {
			{24,32,96,128}, // RGB   what dst to try for src=24
			{24,32,96,128}, // BGR   -"-
			{32,24,128,96}, // RGBA  what dst to try for src=32
			{96,128,24,32}, // RGBF  what dst to try for src=96
			{128,96,32,24}, // RGBAF what dst to try for src=128
			};
		unsigned dstbipp;
		RR_ASSERT(BF_RGB==0 && BF_BGR==1 && BF_RGBA==2 && BF_RGBF==3 && BF_RGBAF==4);
		if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[buffer->getFormat()][0]))
		if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[buffer->getFormat()][1]))
		if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[buffer->getFormat()][2]))
		if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[buffer->getFormat()][3]))
		{
			RRReporter::report(WARN,"Save not supported for %ls format.\n",filename.w_str());
			goto ende;
		}
		fit = (dstbipp==128) ? FIT_RGBAF : ((dstbipp==96)?FIT_RGBF:FIT_BITMAP);
		unsigned dstbypp = (dstbipp+7)/8;

		FIBITMAP* dib = FreeImage_AllocateT(fit,buffer->getWidth(),buffer->getHeight(),dstbipp);
		if (dib)
		{
			BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
			if (fipixels)
			{
				// process all sides
				for (unsigned side=0;side<6;side++)
				{
					if (!side || buffer->getType()==BT_CUBE_TEXTURE)
					{
						// every one image must succeed
						result = false;
						// fill it with texture data
						/*if (dstbypp==srcbypp)
						{
							// use native format
							memcpy(fipixels,rawData+side*getWidth()*getHeight()*dstbypp,getWidth()*getHeight()*dstbypp);
						}
						else*/
						{
							// convert format
							// FreeImage doesn't support all necessary conversions
							unsigned char* src = (unsigned char*)(rawData+side*buffer->getWidth()*buffer->getHeight()*srcbypp);
							unsigned char* dst = (unsigned char*)fipixels;
							unsigned width = buffer->getWidth();
							unsigned numPixels = width*buffer->getHeight();
							bool swaprb = dstbipp<=32;//(srcbipp>32) != (dstbipp>32);
							if (buffer->getFormat()==BF_BGR)
								swaprb = !swaprb;
							for (unsigned i=0;i<numPixels;i++)
							{
								// read src pixel
								float pixel[4];
								switch(srcbipp)
								{
									case 128:
										pixel[0] = ((float*)src)[0];
										pixel[1] = ((float*)src)[1];
										pixel[2] = ((float*)src)[2];
										pixel[3] = ((float*)src)[3];
										break;
									case 96:
										pixel[0] = ((float*)src)[0];
										pixel[1] = ((float*)src)[1];
										pixel[2] = ((float*)src)[2];
										pixel[3] = 1;
										break;
#ifdef RR_BIG_ENDIAN
									case 32:
										pixel[0] = RR_BYTE2FLOAT(src[2]);
										pixel[1] = RR_BYTE2FLOAT(src[1]);
										pixel[2] = RR_BYTE2FLOAT(src[0]);
										pixel[3] = RR_BYTE2FLOAT(src[3]);
										break;
									case 24:
										pixel[0] = RR_BYTE2FLOAT(src[2]);
										pixel[1] = RR_BYTE2FLOAT(src[1]);
										pixel[2] = RR_BYTE2FLOAT(src[0]);
										pixel[3] = 1;
										break;
#else // RR_BIG_ENDIAN
									case 32:
										pixel[0] = RR_BYTE2FLOAT(src[0]);
										pixel[1] = RR_BYTE2FLOAT(src[1]);
										pixel[2] = RR_BYTE2FLOAT(src[2]);
										pixel[3] = RR_BYTE2FLOAT(src[3]);
										break;
									case 24:
										pixel[0] = RR_BYTE2FLOAT(src[0]);
										pixel[1] = RR_BYTE2FLOAT(src[1]);
										pixel[2] = RR_BYTE2FLOAT(src[2]);
										pixel[3] = 1;
										break;
#endif // RR_BIG_ENDIAN
								}
								src += srcbypp;
								// swap r<->b
								if (swaprb)
								{
									float tmp = pixel[0];
									pixel[0] = pixel[2];
									pixel[2] = tmp;
								}
								// write dst pixel
								if ((i%width)==0) dst += 3-(((unsigned long)dst+3)&3); // compensate for freeimage's scanline padding
								switch(dstbipp)
								{
									case 128:
										((float*)dst)[0] = pixel[0];
										((float*)dst)[1] = pixel[1];
										((float*)dst)[2] = pixel[2];
										((float*)dst)[3] = pixel[3];
										break;
									case 96:
										((float*)dst)[0] = pixel[0];
										((float*)dst)[1] = pixel[1];
										((float*)dst)[2] = pixel[2];
										break;
									case 32:
										dst[0] = RR_FLOAT2BYTE(pixel[0]);
										dst[1] = RR_FLOAT2BYTE(pixel[1]);
										dst[2] = RR_FLOAT2BYTE(pixel[2]);
										dst[3] = RR_FLOAT2BYTE(pixel[3]);
										break;
									case 24:
										dst[0] = RR_FLOAT2BYTE(pixel[0]);
										dst[1] = RR_FLOAT2BYTE(pixel[1]);
										dst[2] = RR_FLOAT2BYTE(pixel[2]);
										break;
								}
								dst += dstbypp;
							}
						}

						// generate single side filename
						std::wstring filenameCube = RR_RR2STDW(filename);
						int ofs = (int)filenameCube.find(L"%s");
						if (ofs>=0)
							filenameCube.replace(ofs,2,RRString(cubeSideName[side]).w_str());

						// save single side
						int flags = 0;
						if (saveParameters)
						{
							if (saveParameters->jpegQuality<=(10+25)/2) flags = JPEG_QUALITYBAD; else
							if (saveParameters->jpegQuality<=(25+50)/2) flags = JPEG_QUALITYAVERAGE; else
							if (saveParameters->jpegQuality<=(50+75)/2) flags = JPEG_QUALITYNORMAL; else
							if (saveParameters->jpegQuality<=(75+100)/2) flags = JPEG_QUALITYGOOD; else
							flags = JPEG_QUALITYSUPERB;
						}
#ifdef _WIN32
						result = FreeImage_SaveU(fif, dib, filenameCube.c_str(), flags)!=0;
#else
						result = FreeImage_Save(fif, dib, filenameCube.c_str(), flags)!=0;
#endif
						// if any one of 6 images fails, don't try other and report fail
						if (!result) break;
					}
				}
			}
			FreeImage_Unload(dib);
		}
	}

ende:
	if (rawData)
	{
		buffer->unlock();
	}

	return result;
}


/////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderImages()
{
	RRBuffer::registerLoader(load);
	RRBuffer::registerSaver(save);
}

#endif // SUPPORT_IMAGES

