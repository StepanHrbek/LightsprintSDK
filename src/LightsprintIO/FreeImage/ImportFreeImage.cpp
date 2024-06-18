// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Image load & save using FreeImage.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_FREEIMAGE

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
#include "Lightsprint/RRLight.h" // RRColorSpace
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
		case BF_LUMINANCE: return 1;
		case BF_LUMINANCEF: return 4;
		default: return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// FreeImage load

static unsigned char* loadFreeImage(const RRString& filename,bool flipV,bool flipH,unsigned& width,unsigned& height,RRBufferFormat& outFormat,bool& outScaled)
{
	// uncomment if you wish to skip loading from network
//	if (filename && filename[0]=='\\' && filename[1]=='\\') return nullptr;

	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	unsigned char* pixels = nullptr;

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
			if (flipV)
				FreeImage_FlipVertical(dib1);
			if (flipH)
				FreeImage_FlipHorizontal(dib1);
			unsigned bpp1 = FreeImage_GetBPP(dib1);
			unsigned colorType = FreeImage_GetColorType(dib1);
			outScaled = bpp1<64; // high bpp images are usually in physical scale
			if (bpp1==128)
			{
				// RGBAF
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
				// read size
				width = FreeImage_GetWidth(dib1);
				height = FreeImage_GetHeight(dib1);
				outFormat = BF_RGBF;
				pixels = new unsigned char[12*width*height];
				float* fipixels = (float*)FreeImage_GetBits(dib1);
				memcpy(pixels,fipixels,width*height*12);
			}
			else
			if (colorType==FIC_MINISBLACK || colorType==FIC_MINISWHITE)
			{
				// try conversion to LUMINANCE
				FIBITMAP* dib2 = (bpp1>8) ? FreeImage_ConvertToType(dib1,FIT_FLOAT) : FreeImage_ConvertToGreyscale(dib1);
				if (dib2)
				{
					// read size
					width = FreeImage_GetWidth(dib2);
					height = FreeImage_GetHeight(dib2);
					// convert
					if (bpp1>8)
					{
						outFormat = BF_LUMINANCEF;
						pixels = new unsigned char[4*width*height];
						float* fipixels = (float*)FreeImage_GetBits(dib2);
						memcpy(pixels,fipixels,width*height*4);
					}
					else
					{
						outFormat = BF_LUMINANCE;
						pixels = new unsigned char[width*height];
						BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib2);
						unsigned pitch = FreeImage_GetPitch(dib2);
						for (unsigned j=0;j<height;j++)
						{
							for (unsigned i=0;i<width;i++)
								pixels[j*width+i] = (colorType==FIC_MINISBLACK) ? fipixels[j*pitch+i] : 255-fipixels[j*pitch+i];
						}
					}
					// cleanup
					FreeImage_Unload(dib2);
				}
			}
			else
			if (FreeImage_IsTransparent(dib1))
			{
				// try conversion to 32bit BGRA
				FIBITMAP* dib2 = FreeImage_ConvertTo32Bits(dib1);
				if (dib2)
				{
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
	VBUHeader header(nullptr);
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

static RRBuffer* loadFreeImage(const RRString& filename)
{
	RRBuffer* buffer = RRBuffer::create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,nullptr);
	bool reloaded = (wcsstr(filename.w_str(),L".vbu") || wcsstr(filename.w_str(),L".VBU"))
		? reloadVertexBuffer(buffer,filename)
		: reload2d(buffer,filename);
	buffer->filename = filename; // [#36] exact filename we just opened or failed to open (we don't have a locator -> no attempts to open similar names)
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
		case 33: return FreeImage_FIFSupportsExportType(fif, FIT_FLOAT);
		case 96: return FreeImage_FIFSupportsExportType(fif, FIT_RGBF);
		case 128: return FreeImage_FIFSupportsExportType(fif, FIT_RGBAF);
		default: return FreeImage_FIFSupportsExportBPP(fif, bpp);
	}
}

bool saveFreeImage(RRBuffer* buffer, const RRString& filename, const RRBuffer::SaveParameters* saveParameters)
{
	bool result = false;

#ifdef _WIN32
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilenameU(filename.w_str());
#else
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(filename.c_str());
#endif

	// save vertex buffer
	if (wcsstr(filename.w_str(),L".vbu") || wcsstr(filename.w_str(),L".VBU"))
	{
		if (buffer->getType()!=BT_VERTEX_BUFFER)
		{
			rr::RRReporter::report(rr::WARN,"Attempt to save non-vertex-buffer to .vbu format (vertex buffers only).\n");
			return result;
		}
		const unsigned char* rawData = buffer->lock(BL_READ);
		if (rawData)
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
			buffer->unlock();
		}
		return result;
	}

	// get src format
	rr::RRBufferFormat srcFormat = buffer->getFormat();
	//unsigned srcbypp = (buffer->getElementBits()+7)/8;
	if (srcFormat<BF_RGB || srcFormat>BF_LUMINANCEF)
		return result;

	// select dst format
	unsigned tryTable[][4] =
	{
		{24,32,96,128}, // RGB   what dst to try for src=24
		{24,32,96,128}, // BGR   -"-
		{32,24,128,96}, // RGBA  what dst to try for src=32
		{96,128,24,32}, // RGBF  what dst to try for src=96
		{128,96,32,24}, // RGBAF what dst to try for src=128
		{33,8,24,32}, // DEPTH
		{0,0,0,0}, // DXT1
		{0,0,0,0}, // DXT3
		{0,0,0,0}, // DXT5
		{8,33,24,32}, // LUMINANCE
		{33,96,128,8}, // LUMINANCEF
	};
	unsigned dstbipp; // 33 means 32bit float, while 32 means 32bit rgba
	if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[srcFormat][0]))
	if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[srcFormat][1]))
	if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[srcFormat][2]))
	if (!FIFSupportsExportBPP(fif, dstbipp=tryTable[srcFormat][3]))
	{
		// Don't warn, there's still chance other saver will work (.rrbuffer)
		//RRReporter::report(WARN,"Save not supported for %ls format.\n",filename.w_str());
		return result;
	}
	FREE_IMAGE_TYPE fit = (dstbipp==128)?FIT_RGBAF:( (dstbipp==96)?FIT_RGBF:( (dstbipp==33)?FIT_FLOAT: FIT_BITMAP));
	dstbipp &= 0xfe; // 33->32, remove flag
	unsigned dstbypp = (dstbipp+7)/8;

	// is conversion to sRGB necesasary?
	rr::RRColorSpace* colorSpace = (!buffer->getScaled() && fit==FIT_BITMAP) ? rr::RRColorSpace::create_sRGB() : nullptr;

	FIBITMAP* dib = FreeImage_AllocateT(fit,buffer->getWidth(),buffer->getHeight(),dstbipp);
	if (dib)
	{
		BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
		if (fipixels)
		{
			// process all sides
			unsigned elementIndex = 0;
			for (unsigned side=0;side<6;side++)
			{
				if (!side || buffer->getType()==BT_CUBE_TEXTURE)
				{
					// every one image must succeed
					result = false;
					// fill it with texture data
					// convert format
					// FreeImage doesn't support all necessary conversions
					unsigned char* dst = (unsigned char*)fipixels;
					unsigned width = buffer->getWidth();
					unsigned numPixels = width*buffer->getHeight();
					bool swaprb = dstbipp<=32;
					if (srcFormat==BF_BGR)
						swaprb = !swaprb;
#ifdef RR_BIG_ENDIAN
					if (srcFormat==BF_RGB || srcFormat==BF_BGR || srcFormat==BF_RGBA)
						swaprb = !swaprb;
#endif
					for (unsigned i=0;i<numPixels;i++)
					{
						// read src pixel
						rr::RRVec4 pixel = buffer->getElement(elementIndex++,nullptr);
						// swap r<->b
						if (swaprb)
						{
							float tmp = pixel[0];
							pixel[0] = pixel[2];
							pixel[2] = tmp;
						}
						// convert to sRGB
						if (colorSpace)
							colorSpace->fromLinear(pixel);
						// write dst pixel
						if ((i%width)==0) dst += 3-(((intptr_t)dst+3)&3); // compensate for freeimage's scanline padding
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
								if (fit==FIT_BITMAP)
								{
									dst[0] = RR_FLOAT2BYTE(pixel[0]);
									dst[1] = RR_FLOAT2BYTE(pixel[1]);
									dst[2] = RR_FLOAT2BYTE(pixel[2]);
									dst[3] = RR_FLOAT2BYTE(pixel[3]);
								}
								else
								{
									((float*)dst)[0] = (pixel[0]+pixel[1]+pixel[2])*0.333333333f;
								}
								break;
							case 24:
								dst[0] = RR_FLOAT2BYTE(pixel[0]);
								dst[1] = RR_FLOAT2BYTE(pixel[1]);
								dst[2] = RR_FLOAT2BYTE(pixel[2]);
								break;
							case 8:
								dst[0] = RR_FLOAT2BYTE((pixel[0]+pixel[1]+pixel[2])*0.333333333f);
								break;
						}
						dst += dstbypp;
					}

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
					result = FreeImage_SaveU(fif, dib, filename.w_str(), flags) != 0;
#else
					result = FreeImage_Save(fif, dib, filename.c_str(), flags)!=0;
#endif
					// if any one of 6 images fails, don't try other and report fail
					if (!result) break;

					// if we are saving 6 sides, we don't have mechanism to generate 6 different filenames here,
					// stop after saving first side
					break;
				}
			}
		}
		FreeImage_Unload(dib);
	}

	delete colorSpace;

	return result;
}


/////////////////////////////////////////////////////////////////////////////
//
// main

// converts "jpg,gif" to "*.jpg;*.gif"
std::string convertExtensionList(const char* freeImageFormat)
{
	std::string result;
	bool add = true;
	for (;*freeImageFormat;freeImageFormat++)
	{
		if (*freeImageFormat==',')
		{
			result += ';';
			add = true;
		}
		else
		{
			if (add)
				result += "*.";
			add = false;
			result += *freeImageFormat;
		}
	}
	return result;
}

void registerLoaderFreeImage()
{
	// FreeImage formats
	for (FREE_IMAGE_FORMAT fif = FREE_IMAGE_FORMAT(0); fif<FreeImage_GetFIFCount(); fif = FREE_IMAGE_FORMAT(fif+1))
	{
		if (FreeImage_FIFSupportsReading(fif))
			RRBuffer::registerLoader(convertExtensionList(FreeImage_GetFIFExtensionList(fif)).c_str(),loadFreeImage);
		if (FreeImage_FIFSupportsWriting(fif))
			RRBuffer::registerSaver(convertExtensionList(FreeImage_GetFIFExtensionList(fif)).c_str(),saveFreeImage);
	}
	// our own legacy format from early Lightsprint SDK versions, will be removed
	RRBuffer::registerLoader("*.vbu",loadFreeImage);
	RRBuffer::registerSaver("*.vbu",saveFreeImage);
}

#endif // SUPPORT_FREEIMAGE

