// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "RRBufferInMemory.h"
#include "Lightsprint/RRDebug.h"

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
// RRBufferInMemory

RRBufferInMemory::RRBufferInMemory()
{
	type = BT_VERTEX_BUFFER;
	width = 0;
	height = 0;
	depth = 0;
	format = BF_RGB;
	scaled = false;
	data = NULL;
	version = rand();
}

bool RRBufferInMemory::reset(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, bool _scaled, const unsigned char* _data)
{
	// check params
	if ((_format==BF_RGB || _format==BF_RGBA || _format==BF_RGBF || _format==BF_RGBAF || _format==BF_DEPTH) && (
		(_type==BT_VERTEX_BUFFER && (_width && _height==1 && _depth==1)) ||
		//(_type==BT_1D_TEXTURE && (_width && _height==1 && _depth==1)) ||
		(_type==BT_2D_TEXTURE && (_width && _height && _depth==1)) ||
		//(_type==BT_3D_TEXTURE && (_width && _height && _depth)) ||
		(_type==BT_CUBE_TEXTURE && (_width && _height==_width && _depth==6))
		))
	{
		// ok
	}
	else
	{
		RRReporter::report(WARN,"Invalid parameters in RRBuffer::create(,%d,%d,%d,,,)%s\n",_width,_height,_depth,(_type==BT_VERTEX_BUFFER && !_width)?", object with 0 vertices?":".");
		return false;
	}
	if (!_data && data && _type==type && _width==width && _height==height && _depth==depth && _format==format)
	{
		// optimalization: quit if buffer already has requested properties
		scaled = _scaled;
		return true;
	}
	if ((_format==BF_RGB || _format==BF_RGBA) && !_scaled)
	{
//		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"If it's not for bent normals, integer buffer won't be precise enough for physical (linear) scale data. Switch to floats or custom scale.\n"));
	}
	if ((_format==BF_RGBF || _format==BF_RGBAF) && _scaled && _width>2) // ignore it for tiny buffers like 2*2*6 cube from createSky()
	{
//		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Float buffer used for data in custom scale (screen colors). Maybe you can save space by using integer buffer.\n"));
	}

	unsigned bytesTotal = _width*_height*_depth*getBytesPerPixel(_format);

	// copy data
	if (!data || !_data || width!=_width || height!=_height || depth!=_depth || format!=_format)
	{
		RR_SAFE_DELETE_ARRAY(data);
		try
		{
			// pointer value 1 = don't allocate buffer, caller promises he will never use it
			data = ((_data || _format!=BF_DEPTH) && _data!=(unsigned char*)1) ? new unsigned char[bytesTotal] : NULL;
		}
		catch(std::bad_alloc e)
		{
			RRReporter::report(ERRO,"Not enough memory, %dMB buffer not created.\n",bytesTotal/1024/1024);
			return false;
		}
	}
	if (data && data!=_data)
	{
		if (_data)
			memcpy(data,_data,bytesTotal);
		else
			memset(data,0,bytesTotal);
	}

	type = _type;
	width = _width;
	height = _height;
	depth = _depth;
	format = _format;
	scaled = _scaled;
	version++;
	
	return true;
}

unsigned getBytesPerPixel(RRBufferFormat format);

unsigned RRBufferInMemory::getElementBits() const
{
	return getBytesPerPixel(format)*8;
}

unsigned RRBufferInMemory::getMemoryOccupied() const
{
	return sizeof(*this)+getWidth()*getHeight()*getDepth()*(getElementBits()/8);
}

void RRBufferInMemory::setElement(unsigned index, const RRVec4& element)
{
	if (!data)
	{
		RRReporter::report(WARN,"setElement() called on an uninitialized buffer.\n");
		return;
	}
	if (index>=width*height*depth)
	{
		RRReporter::report(WARN,"setElement(%d) out of range, buffer size %d*%d*%d=%d.\n",index,width,height,depth,width*height*depth);
		return;
	}
	//RR_ASSERT(element[0]>=0); // bent normals may be negative
	switch(format)
	{
		case BF_RGB:
			data[3*index+0] = RR_FLOAT2BYTE(element[0]);
			data[3*index+1] = RR_FLOAT2BYTE(element[1]);
			data[3*index+2] = RR_FLOAT2BYTE(element[2]);
			break;
		case BF_RGBA:
			data[4*index+0] = RR_FLOAT2BYTE(element[0]);
			data[4*index+1] = RR_FLOAT2BYTE(element[1]);
			data[4*index+2] = RR_FLOAT2BYTE(element[2]);
			data[4*index+3] = RR_FLOAT2BYTE(element[3]);
			break;
		case BF_RGBF:
			((RRVec3*)data)[index] = element;
			break;
		case BF_RGBAF:
			((RRVec4*)data)[index] = element;
			break;
		default:
			RRReporter::report(WARN,"Unexpected buffer format.\n");
			break;
	}
	version++;
}

RRVec4 RRBufferInMemory::getElement(unsigned index) const
{
	if (!data)
	{
		RR_LIMITED_TIMES(10,RRReporter::report(WARN,"getElement() called on an uninitialized buffer.\n"));
		return RRVec4(0);
	}
	if (index>=width*height*depth)
	{
		RRReporter::report(WARN,"getElement(%d) out of range, buffer size %d*%d*%d=%d.\n",index,width,height,depth,width*height*depth);
		return RRVec4(0);
	}
	unsigned ofs = index * getElementBits()/8;
	RRVec4 result;
	switch(format)
	{
		case BF_RGB:
			result[0] = RR_BYTE2FLOAT(data[ofs+0]);
			result[1] = RR_BYTE2FLOAT(data[ofs+1]);
			result[2] = RR_BYTE2FLOAT(data[ofs+2]);
			result[3] = 1;
			break;
		case BF_RGBA:
			result[0] = RR_BYTE2FLOAT(data[ofs+0]);
			result[1] = RR_BYTE2FLOAT(data[ofs+1]);
			result[2] = RR_BYTE2FLOAT(data[ofs+2]);
			result[3] = RR_BYTE2FLOAT(data[ofs+3]);
			break;
		case BF_RGBF:
			result = *((RRVec3*)(data+ofs));
			result[3] = 1;
			break;
		case BF_RGBAF:
			result = *((RRVec4*)(data+ofs));
			break;
		default:
			RRReporter::report(WARN,"Unexpected buffer format.\n");
			break;
	}
	//RR_ASSERT(result[0]>=0); // bent normals may be negative
	return result;
}

RRVec4 RRBufferInMemory::getElement(const RRVec3& direction) const
{
	unsigned coord[3];
	switch(type)
	{
		case BT_CUBE_TEXTURE:
			{
				RR_ASSERT(width==height);
				RR_ASSERT(depth==6);
				// find major axis
				RRVec3 d = direction.abs();
				unsigned axis = (d[0]>=d[1] && d[0]>=d[2]) ? 0 : ( (d[1]>=d[0] && d[1]>=d[2]) ? 1 : 2 ); // 0..2
				// find side
				coord[2] = 2*axis + ((direction[axis]<0)?1:0); // 0..5
				// find xy
				static const int sx[6] = {-1,-1,+1,-1,+1,+1};
				static const int  x[6] = { 2, 2, 0, 0, 0, 0};
				static const int  y[6] = { 1, 1, 2, 2, 1, 1};
				static const int sy[6] = {-1,+1,+1,+1,-1,+1};
				coord[0] = (unsigned) RR_CLAMPED((int)((sx[coord[2]]*direction[x[coord[2]]]/direction[axis]+1)*(0.5f*width )),0,(int)width -1); // 0..width
				coord[1] = (unsigned) RR_CLAMPED((int)((sy[coord[2]]*direction[y[coord[2]]]/direction[axis]+1)*(0.5f*height)),0,(int)height-1); // 0..height
				break;
			}
		default:
			{
				coord[0] = (unsigned)(direction[0] * width) % width;
				coord[1] = (unsigned)(direction[1] * height) % height;
				coord[2] = (unsigned)(direction[2] * depth) % depth;
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

RRBuffer* RRBuffer::create(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, bool _scaled, const unsigned char *_data)
{
	RRBuffer* buffer = new RRBufferInMemory();
	if (!buffer->reset(_type,_width,_height,_depth,_format,_scaled,_data))
		RR_SAFE_DELETE(buffer);
	return buffer;
}

RRBuffer* RRBuffer::createSky(const RRVec4& upper, const RRVec4& lower, bool scaled)
{
	if (upper==lower)
	{
		RRVec4 data[6] = {upper,upper,upper,upper,upper,upper};
		return create(BT_CUBE_TEXTURE,1,1,6,BF_RGBAF,scaled,(unsigned char*)data);
	}
	else
	{
		RRVec4 data[24] = {
			upper,upper,lower,lower,
			upper,upper,lower,lower,
			upper,upper,upper,upper,
			lower,lower,lower,lower,//bottom
			upper,upper,lower,lower,
			upper,upper,lower,lower
		};
		return create(BT_CUBE_TEXTURE,2,2,6,BF_RGBAF,scaled,(unsigned char*)data);
	}
}

}; // namespace
