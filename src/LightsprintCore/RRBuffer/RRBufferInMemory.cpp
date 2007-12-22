// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "RRBufferInMemory.h"
#include "Lightsprint/RRDebug.h"

#define FLOAT2BYTE(f) CLAMPED(int(f*256),0,255)
#define BYTE2FLOAT(b) ((b)*0.003921568627450980392156862745098f)

namespace rr
{

unsigned getBytesPerPixel(RRBufferFormat format);

/////////////////////////////////////////////////////////////////////////////
//
// RRBufferInMemory

RRBufferInMemory::RRBufferInMemory(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, const unsigned char *_data)
{
	width = 0;
	height = 0;
	depth = 0;
	data = NULL;
	RRBufferInMemory::reset(_type,_width,_height,_depth,_format,_data);
}

bool RRBufferInMemory::reset(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, const unsigned char* _data)
{
	unsigned bytesTotal = _width*_height*_depth*getBytesPerPixel(_format);

	// copy data
	if(!data || !_data || width!=_width || height!=_height || depth!=_depth || format!=_format)
	{
		delete[] data;
		//data = _data ? new unsigned char[bytesTotal] : NULL;
		data = (_data || _format!=BF_DEPTH) ? new unsigned char[bytesTotal] : NULL;
	}
	if(data && data!=_data)
	{
		if(_data)
			memcpy(data,_data,bytesTotal);
		else
			memset(data,0,bytesTotal);
	}

	type = _type;
	width = _width;
	height = _height;
	depth = _depth;
	format = _format;
	
	return true;
}

unsigned getBytesPerPixel(RRBufferFormat format);

unsigned RRBufferInMemory::getElementBits() const
{
	return getBytesPerPixel(format)*8;
}

void RRBufferInMemory::setElement(unsigned index, const RRVec3& element)
{
	if(!data)
	{
		RRReporter::report(WARN,"setElement() called on an uninitialized buffer.\n");
		return;
	}
	if(index>=width*height*depth)
	{
		RRReporter::report(WARN,"getElement(%d) out of range, buffer size %d*%d*%d=%d.\n",index,width,height,depth,width*height*depth);
		return;
	}
	switch(format)
	{
		case BF_RGB:
			data[3*index+0] = FLOAT2BYTE(element[0]);
			data[3*index+1] = FLOAT2BYTE(element[1]);
			data[3*index+2] = FLOAT2BYTE(element[2]);
			break;
		case BF_RGBA:
			data[4*index+0] = FLOAT2BYTE(element[0]);
			data[4*index+1] = FLOAT2BYTE(element[1]);
			data[4*index+2] = FLOAT2BYTE(element[2]);
			data[4*index+3] = 255;
			break;
		case BF_RGBF:
			((RRVec3*)data)[index] = element;
			break;
		case BF_RGBAF:
			((RRVec4*)data)[index] = element;
			((RRVec4*)data)[index][3] = 1;
			break;
	}
}

RRVec4 RRBufferInMemory::getElement(unsigned index) const
{
	if(!data)
	{
		RRReporter::report(WARN,"getElement() called on uninitialized buffer.\n");
		return RRVec4(0);
	}
	if(index>=width*height*depth)
	{
		RRReporter::report(WARN,"getElement(%d) out of range, buffer size %d*%d*%d=%d.\n",index,width,height,depth,width*height*depth);
		return RRVec4(0);
	}
	unsigned ofs = index * getElementBits()/8;
	RRVec4 result;
	switch(format)
	{
		case BF_RGB:
			result[0] = BYTE2FLOAT(data[ofs+0]);
			result[1] = BYTE2FLOAT(data[ofs+1]);
			result[2] = BYTE2FLOAT(data[ofs+2]);
			result[3] = 1;
			break;
		case BF_RGBA:
			result[0] = BYTE2FLOAT(data[ofs+0]);
			result[1] = BYTE2FLOAT(data[ofs+1]);
			result[2] = BYTE2FLOAT(data[ofs+2]);
			result[3] = BYTE2FLOAT(data[ofs+3]);
			break;
		case BF_RGBF:
			result = *((RRVec3*)(data+ofs));
			result[3] = 1;
			break;
		case BF_RGBAF:
			result = *((RRVec4*)(data+ofs));
			break;
	}
	RR_ASSERT(result[0]>=0);
	RR_ASSERT(result[1]>=0);
	RR_ASSERT(result[2]>=0);
	return result;
}

RRVec4 RRBufferInMemory::getElement(const RRVec3& direction) const
{
	unsigned coord[3];
	switch(type)
	{
		case BT_VERTEX_BUFFER:
		case BT_1D_TEXTURE:
		case BT_2D_TEXTURE:
		case BT_3D_TEXTURE:
			{
				coord[0] = (unsigned)(direction[0] * width) % width;
				coord[1] = (unsigned)(direction[1] * height) % height;
				coord[2] = (unsigned)(direction[2] * depth) % depth;
				break;
			}
		case BT_CUBE_TEXTURE:
			{
				assert(width==height);
				assert(depth==6);
				// find major axis
				RRVec3 d = direction.abs();
				unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 ); // 0..2
				// find xy
				coord[0] = (unsigned) CLAMPED((int)((direction[ axis   ?0:1]/direction[axis]+1)*(0.5f*width )),0,(int)width -1); // 0..width
				coord[1] = (unsigned) CLAMPED((int)((direction[(axis<2)?2:1]/direction[axis]+1)*(0.5f*height)),0,(int)height-1); // 0..height
				// find side
				coord[2] = 2*axis + ((direction[axis]<0)?1:0); // 0..5
				break;
			}
	}
	return getElement(coord[0]+coord[1]*width+coord[2]*width*height);
}

RRBufferInMemory::~RRBufferInMemory()
{
	delete[] data;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer* RRBuffer::create(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, const unsigned char *data)
{
	return new RRBufferInMemory(type,width,height,depth,format,data);
}

RRBuffer* RRBuffer::createSky(RRVec4 color)
{
	RRVec4 data[6] = {color,color,color,color,color,color};
	return new RRBufferInMemory(BT_CUBE_TEXTURE,1,1,6,BF_RGBAF,(unsigned char*)data);
}

RRBuffer* RRBuffer::createSky(const RRVec4& upper, const RRVec4& lower)
{
	RRVec4 data[24] = {
		upper,upper,lower,lower,
		upper,upper,lower,lower,
		upper,upper,upper,upper,
		lower,lower,lower,lower,//bottom
		upper,upper,lower,lower,
		upper,upper,lower,lower
		};
	return new RRBufferInMemory(BT_CUBE_TEXTURE,2,2,6,BF_RGBAF,(unsigned char*)data);
}

RRBuffer* RRBuffer::load(const char *filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	RRBuffer* texture = new RRBufferInMemory(BT_1D_TEXTURE,1,1,1,BF_RGBA,NULL);
	if(!texture->reload(filename,cubeSideName,flipV,flipH))
	{
		SAFE_DELETE(texture);
	}
	return texture;
}

}; // namespace
