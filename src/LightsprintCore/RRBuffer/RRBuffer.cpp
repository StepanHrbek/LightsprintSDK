// --------------------------------------------------------------------------
// Generic buffer code: load & save using FreeImage.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <cstring>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "FreeImage.h"

#pragma comment(lib,"FreeImage.lib")

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define FLOAT2BYTE(f) CLAMPED(int(f*256),0,255)
#define BYTE2FLOAT(b) ((b)*0.003921568627450980392156862745098f)

#define SCALED_DETECTED_FROM_FILE true // textures are always loaded as custom scale data

namespace rr
{

unsigned getBytesPerPixel(RRBufferFormat format)
{
	switch(format)
	{
		case BF_RGB: return 3;
		case BF_RGBA: return 4;
		case BF_RGBF: return 12;
		case BF_RGBAF: return 16;
		case BF_DEPTH: return 4;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer::RRBuffer()
{
	customData = NULL;
}

unsigned RRBuffer::getElementBits() const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElementBits() called.\n"));
	return 0;
}

void RRBuffer::setElement(unsigned index, const RRVec3& element)
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::setElement() called.\n"));
}

RRVec4 RRBuffer::getElement(unsigned index) const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElement(const RRVec3& coord) const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

unsigned char* RRBuffer::lock(RRBufferLock lock)
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::lock() called.\n"));
	return NULL;
}

void RRBuffer::unlock()
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::unlock() called.\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// FreeImage

unsigned char* loadFreeImage(const char *filename,bool cube,bool flipV,bool flipH,unsigned& width,unsigned& height,RRBufferFormat& outFormat)
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
				outFormat = BF_RGBF;
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
					outFormat = BF_RGBA;
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
// RRBuffer load
//
// Universal code that loads texture into system memory, using FreeImage,
// and then calls virtual RRBuffer::reset().
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

struct VBUHeader
{
	RRBufferFormat format:16; // 3 bytes
	unsigned scaled:16; // 1 byte
	unsigned numVertices; // 4 bytes
	VBUHeader(RRBuffer* buffer)
	{
		memset(this,0,sizeof(*this));
		if(buffer)
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
bool reloadVertexBuffer(RRBuffer* texture, const char *filename)
{
	// open
	FILE* f = fopen(filename,"rb");
	if(!f) return false;
	// get filesize
	fseek(f,0,SEEK_END);
	unsigned datasize = ftell(f)-sizeof(VBUHeader);
	fseek(f,0,SEEK_SET);
	// read header
	VBUHeader header(NULL);
	fread(&header,sizeof(header),1,f);
	if(header.getDataSize()!=datasize)
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
bool reload2d(RRBuffer* texture, const char *filename, bool flipV, bool flipH)
{
	unsigned width = 0;
	unsigned height = 0;
	RRBufferFormat format = BF_DEPTH;
	unsigned char* pixels = loadFreeImage(filename,false,flipV,flipH,width,height,format);
	if(!pixels)
	{
		return false;
	}
	else
	{
		texture->reset(BT_2D_TEXTURE,width,height,1,format,SCALED_DETECTED_FROM_FILE,pixels);
		delete[] pixels;
		return true;
	}
}

// cube map loader
bool reloadCube(RRBuffer* texture, const char *filenameMask, const char *cubeSideName[6], bool flipV, bool flipH)
{
	unsigned width = 0;
	unsigned height = 0;
	RRBufferFormat format = BF_DEPTH;
	unsigned char* pixels = NULL;
	bool sixFiles = filenameMask && strstr(filenameMask,"%s");
	if(!sixFiles)
	{
		// LOAD PIXELS FROM SINGLE FILE.HDR
		pixels = loadFreeImage(filenameMask,false,flipV,flipH,width,height,format);
		if(!pixels) return false;
		shuffleCrossToCube(pixels,width,height,getBytesPerPixel(format));
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
			RRBufferFormat tmpFormat;

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
		pixels = new unsigned char[width*height*getBytesPerPixel(format)*6];
		for(unsigned side=0;side<6;side++)
		{
			memcpy(pixels+width*height*getBytesPerPixel(format)*side,sides[side],width*height*getBytesPerPixel(format));
			SAFE_DELETE_ARRAY(sides[side]);
		}
	}

	// load cube from 1 array
	texture->reset(BT_CUBE_TEXTURE,width,height,6,format,SCALED_DETECTED_FROM_FILE,pixels);
	delete[] pixels;
	return true;
}

bool RRBuffer::reload(const char *filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	bool reloaded = (strstr(filename,".vbu") || strstr(filename,".VBU"))
		? reloadVertexBuffer(this,filename)
		: (cubeSideName
		? reloadCube(this,filename,cubeSideName,flipV,flipH)
		: reload2d(this,filename,flipV,flipH) );
	if(!reloaded)
	{
		rr::RRReporter::report(rr::ERRO,"Failed to reload %s.\n",filename);
	}
	return reloaded;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer save

bool RRBuffer::save(const char *filename, const char* cubeSideName[6])
{
	bool result = false;

	// preliminary, may change due to cubeSideName replacement
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(filename);

	// default cube side names
	const char* cubeSideNameBackup[6] = {"0","1","2","3","4","5"};
	if(!cubeSideName)
		cubeSideName = cubeSideNameBackup;

	const unsigned char* rawData = lock(BL_READ);
	if(rawData)
	{
		// save vertex buffer
		if(getType()==BT_VERTEX_BUFFER)
		{
			VBUHeader header(this);
			FILE* f = fopen(filename,"wb");
			if(f)
			{
				fwrite(&header,sizeof(header),1,f);
				unsigned written = (unsigned)fwrite(rawData,getElementBits()/8,getWidth(),f);
				fclose(f);
				result = written == getWidth();
			}
			goto ende;
		}

		// get src format
		unsigned srcbypp = getBytesPerPixel(getFormat());
		unsigned srcbipp = 8*srcbypp;
		FREE_IMAGE_TYPE fit;
		switch(getFormat())
		{
			case BF_RGB: fit = FIT_BITMAP; break;
			case BF_RGBA: fit = FIT_BITMAP; break;
			case BF_RGBF: fit = FIT_RGBF; break;
			case BF_RGBAF: fit = FIT_RGBAF; break;
			default: assert(0); goto ende;
		}

		// select dst format
		unsigned tryTable[4][4] = {
			{24,32,96,128}, // what dst to try for src=24
			{32,24,128,96}, // what dst to try for src=32
			{96,128,24,32}, // what dst to try for src=96
			{128,96,32,24}, // what dst to try for src=128
			};
		unsigned dstbipp;
		RR_ASSERT(BF_RGB==0 && BF_RGBA==1 && BF_RGBF==2 && BF_RGBAF==3);
		if(!FreeImage_FIFSupportsExportBPP(fif, dstbipp=tryTable[getFormat()][0]))
		if(!FreeImage_FIFSupportsExportBPP(fif, dstbipp=tryTable[getFormat()][1]))
		if(!FreeImage_FIFSupportsExportBPP(fif, dstbipp=tryTable[getFormat()][2]))
		if(!FreeImage_FIFSupportsExportBPP(fif, dstbipp=tryTable[getFormat()][3]))
		{
			RRReporter::report(WARN,"Save not supported for %s format.\n",filename);
			goto ende;
		}
		fit = (dstbipp==128) ? FIT_RGBAF : ((dstbipp==96)?FIT_RGBF:FIT_BITMAP);
		unsigned dstbypp = (dstbipp+7)/8;

		FIBITMAP* dib = FreeImage_AllocateT(fit,getWidth(),getHeight(),dstbipp);
		if(dib)
		{
			BYTE* fipixels = (BYTE*)FreeImage_GetBits(dib);
			if(fipixels)
			{
				// process all sides
				for(unsigned side=0;side<6;side++)
				{
					if(!side || getType()==BT_CUBE_TEXTURE)
					{
						// every one image must succeed
						result = false;
						// fill it with texture data
						/*if(dstbypp==srcbypp)
						{
							// use native format
							memcpy(fipixels,rawData+side*getWidth()*getHeight()*dstbypp,getWidth()*getHeight()*dstbypp);
						}
						else*/
						{
							// convert format
							// FreeImage doesn't support all necessary conversions
							unsigned char* src = (unsigned char*)(rawData+side*getWidth()*getHeight()*srcbypp);
							unsigned char* dst = (unsigned char*)fipixels;
							unsigned numPixels = getWidth()*getHeight();
							bool swaprb = dstbipp<=32;//(srcbipp>32) != (dstbipp>32);
							for(unsigned i=0;i<numPixels;i++)
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
									case 32:
										pixel[0] = BYTE2FLOAT(src[0]);
										pixel[1] = BYTE2FLOAT(src[1]);
										pixel[2] = BYTE2FLOAT(src[2]);
										pixel[3] = BYTE2FLOAT(src[3]);
										break;
									case 24:
										pixel[0] = BYTE2FLOAT(src[0]);
										pixel[1] = BYTE2FLOAT(src[1]);
										pixel[2] = BYTE2FLOAT(src[2]);
										pixel[3] = 1;
										break;
								}
								src += srcbypp;
								// swap r<->b
								if(swaprb)
								{
									float tmp = pixel[0];
									pixel[0] = pixel[2];
									pixel[2] = tmp;
								}
								// write dst pixel
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
										dst[0] = FLOAT2BYTE(pixel[0]);
										dst[1] = FLOAT2BYTE(pixel[1]);
										dst[2] = FLOAT2BYTE(pixel[2]);
										dst[3] = FLOAT2BYTE(pixel[3]);
										break;
									case 24:
										dst[0] = FLOAT2BYTE(pixel[0]);
										dst[1] = FLOAT2BYTE(pixel[1]);
										dst[2] = FLOAT2BYTE(pixel[2]);
										break;
								}
								dst += dstbypp;
							}
						}

						// generate single side filename
						char filenameCube[1000];
						_snprintf(filenameCube,999,filename,cubeSideName[side]);
						filenameCube[999] = 0;

						// save single side
						result = FreeImage_Save(fif, dib, filenameCube)!=0;
						// if any one of 6 images fails, don't try other and report fail
						if(!result) break;
					}
				}
			}
			FreeImage_Unload(dib);
		}
	}

ende:
	if(rawData)
	{
		unlock();
	}

	return result;
}

}; // namespace
