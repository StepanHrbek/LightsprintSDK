// --------------------------------------------------------------------------
// Generic buffer code: load & save using FreeImage.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRLight.h"
#include "../squish/squish.h"
#include "../RRDynamicSolver/gather.h" // TexelFlags

namespace rr
{

static RRBuffer::Reloader* s_reload = NULL;
static RRBuffer::Loader* s_load = NULL;
static RRBuffer::Saver* s_save = NULL;

RRBuffer::RRBuffer()
{
	customData = NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer interface

void RRBuffer::setElement(unsigned index, const RRVec4& element)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::setElement() called.\n"));
}

RRVec4 RRBuffer::getElement(unsigned index) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElementAtPosition(const RRVec3& coord) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElementAtPosition() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElementAtDirection(const RRVec3& dir) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElementAtDirection() called.\n"));
	return RRVec4(0);
}

unsigned char* RRBuffer::lock(RRBufferLock lock)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::lock() called.\n"));
	return NULL;
}

void RRBuffer::unlock()
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::unlock() called.\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer tools

RRBuffer* RRBuffer::createCopy()
{
	unsigned char* data = lock(BL_READ);
	RRBuffer* copy = RRBuffer::create(getType(),getWidth(),getHeight(),getDepth(),getFormat(),getScaled(),data);
	unlock();
	return copy;
}

bool RRBuffer::copyElementsTo(RRBuffer* destination, const RRScaler* scaler) const
{
	const RRBuffer* source = this; 
	if (!source || !destination)
	{
		RR_ASSERT(0); // invalid inputs
		return false;
	}
	unsigned w = destination->getWidth();
	unsigned h = destination->getHeight();
	unsigned d = destination->getDepth();
	if (source->getWidth()!=w || source->getHeight()!=h || source->getDepth()!=d)
	{
		RR_ASSERT(0); // invalid inputs
		return false;
	}
	unsigned size = w*h*d;
	const RRScaler* toCust = (!source->getScaled() && destination->getScaled()) ? scaler : NULL;
	const RRScaler* toPhys = (source->getScaled() && !destination->getScaled()) ? scaler : NULL;
	for (unsigned i=0;i<size;i++)
	{
		RRVec4 color = source->getElement(i);
		if (toCust) toCust->getCustomScale(color); else
		if (toPhys) toPhys->getPhysicalScale(color);
		destination->setElement(i,color);
	}
	return true;
}

void RRBuffer::setFormat(RRBufferFormat newFormat)
{
	if (newFormat==getFormat())
	{
		return;
	}
	if (getFormat()==BF_DXT1 || getFormat()==BF_DXT3 || getFormat()==BF_DXT5)
	{
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),BF_RGBA,getScaled(),NULL);
		int flags = 0;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
		};
		// compressed copy -> decompressed this
		squish::DecompressImage(lock(BL_DISCARD_AND_WRITE),getWidth(),getHeight(),copy->lock(BL_READ),flags);
		unlock();
		copy->unlock();
		delete copy;
		setFormat(newFormat);
	}
	else
	if (newFormat==BF_DXT1 || newFormat==BF_DXT3 || newFormat==BF_DXT5)
	{
		setFormat(BF_RGBA);
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),NULL);
		int flags = 0;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
		};
		// uncompressed copy -> compressed this
		squish::CompressImage(copy->lock(BL_READ),getWidth(),getHeight(),lock(BL_DISCARD_AND_WRITE),flags);
		unlock();
		copy->unlock();
		delete copy;
	}
	else
	{
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),NULL);
		unsigned numElements = getWidth()*getHeight()*getDepth();
		for (unsigned i=0;i<numElements;i++)
		{
			RRVec4 color = copy->getElement(i);
			setElement(i,color);
		}
		delete copy;
	}
}

void RRBuffer::setFormatFloats()
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
			setFormat(BF_RGBF);
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
		case BF_RGBA:
			setFormat(BF_RGBAF);
			break;
	}
}

void RRBuffer::clear(RRVec4 clearColor)
{
	unsigned numElements = getWidth()*getHeight()*getDepth();
	for (unsigned i=0;i<numElements;i++)
	{
		setElement(i,clearColor);
	}
}

void RRBuffer::invert()
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
			{
				unsigned numElements = getWidth()*getHeight()*getDepth();
				for (unsigned i=0;i<numElements;i++)
				{
					RRVec4 color = getElement(i);
					color = RRVec4(1)-color;
					setElement(i,color);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"invert() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::multiplyAdd(RRVec4 multiplier, RRVec4 addend)
{
	if (multiplier==RRVec4(1) && addend==RRVec4(0))
	{
		return;
	}
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
			{
				unsigned numElements = getWidth()*getHeight()*getDepth();
				for (unsigned i=0;i<numElements;i++)
				{
					RRVec4 color = getElement(i);
					color = color*multiplier+addend;
					setElement(i,color);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"multiplyAdd() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::flip(bool flipX, bool flipY, bool flipZ)
{
	if (!flipX && !flipY && !flipZ)
	{
		return;
	}
	switch (getFormat())
	{
		case BF_RGB:
		case BF_BGR:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
			{
				// slow getElement path, faster path can be written using lock and direct access
				unsigned xmax = getWidth();
				unsigned ymax = getHeight();
				unsigned zmax = getDepth();
				for (unsigned x=0;x<xmax;x++)
				for (unsigned y=0;y<ymax;y++)
				for (unsigned z=0;z<zmax;z++)
				{
					unsigned e1 = x+xmax*(y+ymax*z);
					unsigned e2 = (flipX?xmax-1-x:x)+xmax*((flipY?ymax-1-y:y)+ymax*(flipZ?zmax-1-z:z));
					if (e1<e2)
					{
						RRVec4 color1 = getElement(e1);
						RRVec4 color2 = getElement(e2);
						setElement(e1,color2);
						setElement(e2,color1);
					}
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"flip() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::brightnessGamma(rr::RRVec4 brightness, rr::RRVec4 gamma)
{
	if (brightness==RRVec4(1) && gamma==RRVec4(1))
	{
		return;
	}
	// slow getElement path, faster path can be written using lock and direct access
	int numElements = getWidth()*getHeight()*getDepth();
#pragma omp parallel for
	for (int i=0;i<numElements;i++)
	{
		rr::RRVec4 element = getElement(i);
		for (unsigned j=0;j<4;j++)
			element[j] = pow(element[j]*brightness[j],gamma[j]);
		setElement(i,element);
	}
}

void RRBuffer::getMinMax(RRVec4* _mini, RRVec4* _maxi)
{
	// slow getElement path, faster path can be written using lock and direct access
	unsigned numElements = getWidth()*getHeight()*getDepth();
	RRVec4 mini(1e20f);
	RRVec4 maxi(-1e20f);
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i);
		for (unsigned j=0;j<4;j++)
		{
			mini[j] = RR_MIN(mini[j],color[j]);
			maxi[j] = RR_MAX(maxi[j],color[j]);
		}
	}
	if (_mini) *_mini = mini;
	if (_maxi) *_maxi = maxi;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRBuffer tools for lightmaps

bool RRBuffer::lightmapSmooth(float _sigma, bool _wrap)
{
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (_sigma<0)
		return false;
	if (_sigma==0)
		return true;

	// copy image from buffer to temp
	unsigned width = getWidth();
	unsigned height = getHeight();
	unsigned size = width*height;
	void* buf;
	buf = malloc(size*(sizeof(RRVec4)*2+1));
	if (!buf)
	{
		RRReporter::report(WARN,"Allocation of %dMB failed in lightmapSmooth().\n",size*(sizeof(RRVec4)*2+1)/1024/1024);
		return false;
	}
	RRVec4* source = (RRVec4*)buf;
	RRVec4* destination = source+size;
	for (unsigned i=0;i<size;i++)
	{
		source[i] = getElement(i);
	}

	// fill blurFlags, what neighbors to blur with
	// 8bits per texel = should it blur with 8 neighbors?
	// bits:
	//  ^ j
	//  |
	//  5 6 7
	//  3   4
	//  0 1 2  --> i
	unsigned char* blurFlags = (unsigned char*)(destination+size);
	for (unsigned j=0;j<height;j++)
	for (unsigned i=0;i<width;i++)
	{
		unsigned k = i+j*width;
		unsigned centerFlags = FLOAT_TO_TEXELFLAGS(source[k][3]);
		unsigned blurBit = 1;
		unsigned blurFlagsK = 0;
		for (int jj=-1;jj<2;jj++)
		for (int ii=-1;ii<2;ii++)
			if (ii||jj)
			{
				// calculate neighbor coordinates x,y
				int x = i+ii;
				int y = j+jj;
				if (_wrap)
				{
					x = (x+width)%width;
					y = (y+height)%height;
				}
				// if in range, set good neighbor flags
				if (x>=0 && x<(int)width && y>=0 && y<(int)height)
				{
					unsigned neighborFlags = FLOAT_TO_TEXELFLAGS(source[x+y*width][3]);
					if (1
						&& (ii>=0 || (centerFlags&EDGE_X0 && neighborFlags&EDGE_X1))
						&& (ii<=0 || (centerFlags&EDGE_X1 && neighborFlags&EDGE_X0))
						&& (jj>=0 || (centerFlags&EDGE_Y0 && neighborFlags&EDGE_Y1))
						&& (jj<=0 || (centerFlags&EDGE_Y1 && neighborFlags&EDGE_Y0))
						)
						blurFlagsK |= blurBit;
				}
				blurBit *= 2;
			}
		blurFlags[k] = blurFlagsK;
	}

	// alpha=1 in temp
	for (unsigned i=0;i<size;i++)
	{
		source[i][3] = (source[i][3]>0) ? 1.f : 0.f;
	}

	// blur temp
	if (_sigma>10)
		_sigma = 10;
	float sigma2sum = _sigma*_sigma;
	while (sigma2sum>0)
	{
		// calculate amount of blur for this pass
		float sigma2 = RR_MIN(sigma2sum,0.34f);
		RRVec4 kernel(1,expf(-0.5f/sigma2),expf(-1/sigma2),expf(-2/sigma2));
		sigma2sum -= sigma2;

		// dst=blur(src)
		#pragma omp parallel for schedule(static)
		for (int j=0;j<(int)height;j++)
		for (int i=0;i<(int)width;i++)
		{
			unsigned k = i+j*width;
			RRVec4 c = source[k];
			if (c[3]!=0)
			{
				unsigned flags = blurFlags[k];
				c = c * kernel[0]
					+ source[((i-1+width)%width)+((j-1+height)%height)*width] * ((flags&1  )?kernel[2]:0)
					+ source[( i               )+((j-1+height)%height)*width] * ((flags&2  )?kernel[1]:0)
					+ source[((i+1      )%width)+((j-1+height)%height)*width] * ((flags&4  )?kernel[2]:0)
					+ source[((i-1+width)%width)+( j                 )*width] * ((flags&8  )?kernel[1]:0)
					+ source[((i+1      )%width)+( j                 )*width] * ((flags&16 )?kernel[1]:0)
					+ source[((i-1+width)%width)+((j+1       )%height)*width] * ((flags&32 )?kernel[2]:0)
					+ source[( i               )+((j+1       )%height)*width] * ((flags&64 )?kernel[1]:0)
					+ source[((i+1      )%width)+((j+1       )%height)*width] * ((flags&128)?kernel[2]:0)
					;
				c /= c[3];
			}
			destination[k] = c;
		}

		// src=dst
		RRVec4* temp = source;
		source = destination;
		destination = temp;
	}

	// copy temp back to buffer, preserve alpha
	for (unsigned i=0;i<size;i++)
		if (source[i][3]>0)
			setElement(i,RRVec4(source[i][0],source[i][1],source[i][2],getElement(i)[3]));
	delete[] buf;
	return true;
}

bool RRBuffer::lightmapGrowForBilinearInterpolation(bool _wrap)
{
	bool notEmpty = false;
	unsigned width = getWidth();
	unsigned height = getHeight();
	for (unsigned j=0;j<height;j++)
	for (unsigned i=0;i<width;i++)
	{
		// we are processing texel i,j
		if (getElement(i+j*width)[3]>0.002f)
		{
			// not empty, keep it unchanged
			notEmpty = true;
		}
		else
		{
			// empty, change it to average of good neighbors
			RRVec4 sum(0);
			for (int jj=-1;jj<2;jj++)
			for (int ii=-1;ii<2;ii++)
			{
				// calculate neighbor coordinates x,y
				int x = i+ii;
				int y = j+jj;
				if (_wrap)
				{
					x = (x+width)%width;
					y = (y+height)%height;
				}
				else
				{
					if (x<0 || x>=(int)width || y<0 || y>=(int)height)
						continue;
				}
				// read neighbor
				RRVec4 c = getElement(x+y*width);
				unsigned texelFlags = FLOAT_TO_TEXELFLAGS(c[3]);
				// is it good one?
				if (0
					|| (ii<+1 && jj<+1 && (texelFlags&QUADRANT_X1Y1))
					|| (ii>-1 && jj>-1 && (texelFlags&QUADRANT_X0Y0))
					|| (ii<+1 && jj>-1 && (texelFlags&QUADRANT_X1Y0))
					|| (ii>-1 && jj<+1 && (texelFlags&QUADRANT_X0Y1))
					)
				{
					// sum it
					float weight = (ii && jj)?1:1.6f;
					sum += RRVec4(c[0],c[1],c[2],1)*weight;
				}
			}
			if (sum[3])
				setElement(i+j*width,RRVec4(sum[0]/sum[3],sum[1]/sum[3],sum[2]/sum[3],
					0.001f // small enough to be invisible for texelFlags, but big enough to be >0, to prevent growForeground() and fillBackground() from overwriting this texel
					));
		}
	}
	return notEmpty;
}

bool RRBuffer::lightmapGrow(unsigned _numSteps, bool _wrap)
{
	if (!this)
	{
		RR_ASSERT(0);
		return false;
	}
	if (_numSteps==0)
		return true;
	if (getFormat()==BF_DXT1 || getFormat()==BF_DXT3 || getFormat()==BF_DXT5)
	{
		// can't manipulate compressed buffer
		return false;
	}
	if (getFormat()!=BF_RGBA && getFormat()!=BF_RGBAF)
	{
		// no alpha -> everything is foreground -> no space to grow
		return true;
	}

	// copy image from buffer to temp
	unsigned width = getWidth();
	unsigned height = getHeight();
	unsigned size = width*height;
	RRVec4* buf;
	buf = new (std::nothrow) RRVec4[size*2];
	if (!buf)
	{
		RRReporter::report(WARN,"Allocation of %dMB failed in lightmapGrow().\n",size*2*sizeof(RRVec4)/1024/1024);
		return false;
	}
	RRVec4* source = buf;
	RRVec4* destination = buf+size;
	for (unsigned i=0;i<size;i++)
	{
		RRVec4 c = getElement(i);
		source[i] = c[3]>0 ? RRVec4(c[0],c[1],c[2],1) : RRVec4(0);
	}

	// grow temp
	while (_numSteps--)
	{
		bool changed = false;
		if (_wrap)
		{
			// faster version with wrap
			#pragma omp parallel for schedule(static)
			for (int i=0;i<(int)size;i++)
			{
				RRVec4 c = source[i];
				if (c[3]==0)
				{
					RRVec4 d = (source[(i+1)%size] + source[(i-1)%size] + source[(i+width)%size] + source[(i-width)%size]) * 1.6f
						+ (source[(i+1+width)%size] + source[(i+1-width)%size] + source[(i-1+width)%size] + source[(i-1-width)%size]);
					if (d[3]!=0)
					{
						c = d/d[3];
						changed = true;
					}
				}
				destination[i] = c;
			}
		}
		else
		{
			// slower version without wrap
			#pragma omp parallel for schedule(static)
			for (int i=0;i<(int)size;i++)
			{
				RRVec4 c = source[i];
				if (c[3]==0)
				{
					int x = i%width;
					int y = i/width;
					int w = (int)width;
					int h = (int)height;
					RRVec4 d = (source[RR_MIN(x+1,w-1) + y*w] + source[RR_MAX(x-1,0) + y*w] + source[x + RR_MIN(y+1,h-1)*w] + source[x + RR_MAX(y-1,0)*w]) * 1.6f
						+ (source[RR_MIN(x+1,w-1) + RR_MIN(y+1,h-1)*w] + source[RR_MIN(x+1,w-1) + RR_MAX(y-1,0)*w] + source[RR_MAX(x-1,0) + RR_MIN(y+1,h-1)*w] + source[RR_MAX(x-1,0) + RR_MAX(y-1,0)*w]);
					if (d[3]!=0)
					{
						c = d/d[3];
						changed = true;
					}
				}
				destination[i] = c;
			}
		}
		if (!changed)
			_numSteps = 0; // stop growing if it has no effect
		RRVec4* temp = source;
		source = destination;
		destination = temp;
	}

	// copy temp back to buffer
	for (unsigned i=0;i<size;i++)
		if (source[i][3]>0 && getElement(i)[3]==0)
			setElement(i,RRVec4(source[i][0],source[i][1],source[i][2],0.001f));
	delete[] buf;
	return true;
}

void RRBuffer::lightmapFillBackground(RRVec4 backgroundColor)
{
	unsigned numElements = getWidth()*getHeight()*getDepth();
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i);
		setElement(i,(color[3]<0)?backgroundColor:color);
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageCache
// - needs exceptions, implemented in exceptions.cpp

RRBuffer* load_noncached(const char *_filename, const char* _cubeSideName[6])
{
	if (!_filename || !_filename[0])
	{
		return NULL;
	}
	// try reloader for images (don't call texture->reload(), it would report failure)
	if (s_reload)
	{
		RRBuffer* texture = RRBuffer::create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
		if (s_reload(texture,_filename,_cubeSideName))
		{
			texture->filename = _filename;
			return texture;
		}
		delete texture;
	}
	// try loader for videos
	if (s_load)
	{
		return s_load(_filename);
	}
	return NULL;
}

extern RRBuffer* load_cached(const char* filename, const char* cubeSideName[6]);


/////////////////////////////////////////////////////////////////////////////
//
// save & load

RRBuffer::Reloader* RRBuffer::setReloader(Reloader* reloader)
{
	Reloader* result = s_reload;
	s_reload = reloader;
	return result;
}

RRBuffer::Loader* RRBuffer::setLoader(Loader* loader)
{
	Loader* result = s_load;
	s_load = loader;
	return result;
}

RRBuffer::Saver* RRBuffer::setSaver(Saver* saver)
{
	Saver* result = s_save;
	s_save = saver;
	return result;
}

bool RRBuffer::save(const char *_filename, const char* _cubeSideName[6], const SaveParameters* _parameters)
{
	if (s_save && this && _filename && _filename[0] && s_save(this,_filename,_cubeSideName,_parameters))
	{
		filename = _filename;
		return true;
	}
	return false;
}

bool RRBuffer::reload(const char *_filename, const char* _cubeSideName[6])
{
	if (!_filename || !_filename[0])
	{
		return false;
	}
	if (!this)
	{
		RRReporter::report(WARN,"Attempted NULL->reload().\n");
		return false;
	}
	if (!s_reload)
	{
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Can't reload(), no reloader.\n"));
		return false;
	}
	if (!s_reload(this,_filename,_cubeSideName))
	{
		RRReporter::report(WARN,"Failed to reload(%s).\n",_filename);
		return false;
	}
	filename = _filename;
	return true;
}

RRBuffer* RRBuffer::load(const char *_filename, const char* _cubeSideName[6])
{
	if (!_filename || !_filename[0])
	{
		return NULL;
	}
	if (!s_load && !s_reload)
	{
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Can't load images, register loader or reloader first, see LightsprintIO.\n"));
		return NULL;
	}
	// cached version
	RRBuffer* result = load_cached(_filename,_cubeSideName);
	if (!result)
	{
		RRReporter::report(WARN,"Failed to load(%s).\n",_filename);
		return NULL;
	}
	return result;

	// non-cached version
	//RRBuffer* texture = create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
	//if (!texture->reload(filename,cubeSideName))
	//{
	//	RR_SAFE_DELETE(texture);
	//}
	//return texture;
}

static const char** selectCubeSideNames(char *_filename)
{
	const unsigned numConventions = 3;
	static const char* cubeSideNames[numConventions][6] =
	{
		{"bk","ft","up","dn","rt","lf"}, // Quake
		{"BK","FT","UP","DN","RT","LF"}, // Quake
		{"negative_x","positive_x","positive_y","negative_y","positive_z","negative_z"} // codemonsters.de
	};
	// inserts %s if it is one of 6 images
	size_t filenameLen = strlen(_filename);
	for (unsigned c=0;c<numConventions;c++)
	{
		for (unsigned s=0;s<6;s++)
		{
			const char* suffix = cubeSideNames[c][s];
			size_t suffixLen = strlen(suffix);
			if (filenameLen>=suffixLen+4 && strncmp(_filename+filenameLen-suffixLen-4,suffix,suffixLen)==0 && _filename[filenameLen-4]=='.')
			{
				_filename[filenameLen-suffixLen-4] = '%';
				_filename[filenameLen-suffixLen-3] = 's';
				_filename[filenameLen-suffixLen-2] = '.';
				_filename[filenameLen-suffixLen-1] = _filename[filenameLen-3];
				_filename[filenameLen-suffixLen  ] = _filename[filenameLen-2];
				_filename[filenameLen-suffixLen+1] = _filename[filenameLen-1];
				_filename[filenameLen-suffixLen+2] = 0;
				return cubeSideNames[c];
			}
		}
	}
	return cubeSideNames[0];
}

RRBuffer* RRBuffer::loadCube(const char *_filename)
{
	if (!_filename || !_filename[0])
	{
		return NULL;
	}
	char* filename = _strdup(_filename);
	const char** cubeSideNames = selectCubeSideNames(filename);
	RRBuffer* result = load(filename,cubeSideNames);
	free(filename);
	return result;
}

bool RRBuffer::reloadCube(const char *_filename)
{
	if (!_filename || !_filename[0])
	{
		return false;
	}
	char* filename = _strdup(_filename);
	const char** cubeSideNames = selectCubeSideNames(filename);
	bool result = reload(filename,cubeSideNames);
	free(filename);
	return result;
}

}; // namespace
