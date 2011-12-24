// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (c) 2006-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "RRBufferInMemory.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

static unsigned getBitsPerPixel(RRBufferFormat format)
{
	switch (format)
	{
		case BF_RGB: return 24;
		case BF_BGR: return 24;
		case BF_RGBA: return 32;
		case BF_RGBF: return 96;
		case BF_RGBAF: return 128;
		case BF_DEPTH: return 32;
		case BF_DXT1: return 4;
		case BF_DXT3: return 8;
		case BF_DXT5: return 8;
	}
	return 0;
}

static unsigned getBufferSize(RRBufferFormat _format, unsigned _width, unsigned _height, unsigned _depth)
{
	switch (_format)
	{
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			// align size up, DXT image consists of 4x4 blocks
			_width = (_width+3)&0xfffffffc;
			_height = (_height+3)&0xfffffffc;
			break;
	}
	return _width * _height * _depth * getBitsPerPixel(_format) / 8;
}

unsigned RRBuffer::getElementBits() const
{
	return getBitsPerPixel(getFormat());
}

unsigned RRBuffer::getBufferBytes() const
{
	return getBufferSize(getFormat(),getWidth(),getHeight(),getDepth());
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBufferInMemory

void* RRBufferInMemory::operator new(std::size_t n)
{
	return malloc(n);
};

RRBufferInMemory::RRBufferInMemory()
{
	refCount = 1;
	type = BT_VERTEX_BUFFER;
	width = 0;
	height = 0;
	depth = 0;
	format = BF_RGB;
	scaled = false;
	data = NULL;
	version = rand();
}

RRBuffer* RRBufferInMemory::createReference()
{
	if (this)
		refCount++;
	return this;
}

static void* g_classHeader;

RRBufferInMemory::~RRBufferInMemory()
{
	// when you delete buffer, this is called first
	if (--refCount)
	{
		// skip destructor
		filename._skipDestructor();
		// backup first 4 or 8 bytes
		memcpy(&g_classHeader,this,sizeof(void*));
	}
	else
	{
		// destruct
		delete[] data;
	}
}

void RRBufferInMemory::operator delete(void* p, std::size_t n)
{
	// when you delete buffer, this is called last
	if (p)
	{
		RRBufferInMemory* b = (RRBufferInMemory*)p;
		if (b->refCount)
		{
			// fix instance after destructor (restore first 4 or 8 bytes)
			memcpy(b,&g_classHeader,sizeof(void*));
			// however, if last reference remains, try to delete it from cache
			if (b->refCount==1)
				b->deleteFromCache();
		}
		else
		{
			// delete instance after destructor
			free(p);
		}
	}
};


unsigned RRBufferInMemory::getBufferBytes() const
{
	return data ? RRBuffer::getBufferBytes() : 0;
}

bool RRBufferInMemory::reset(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, bool _scaled, const unsigned char* _data)
{
	// check params
	if ((_format==BF_RGB || _format==BF_BGR || _format==BF_RGBA || _format==BF_RGBF || _format==BF_RGBAF || _format==BF_DEPTH || _format==BF_DXT1 || _format==BF_DXT3 || _format==BF_DXT5) && (
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
	if (((!_data && data) || (_data==RR_GHOST_BUFFER && !data)) && _type==type && _width==width && _height==height && _depth==depth && _format==format)
	{
		// optimalization: quit if buffer already has requested properties
		scaled = _scaled;
		return true;
	}
	if ((_format==BF_RGB || _format==BF_BGR || _format==BF_RGBA) && !_scaled)
	{
//		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"If it's not for bent normals, integer buffer won't be precise enough for physical (linear) scale data. Switch to floats or custom scale.\n"));
	}
	if ((_format==BF_RGBF || _format==BF_RGBAF) && _scaled && _width>2) // ignore it for tiny buffers like 2*2*6 cube from createSky()
	{
//		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Float buffer used for data in custom scale (screen colors). Maybe you can save space by using integer buffer.\n"));
	}

	unsigned bytesTotal = getBufferSize(_format,_width,_height,_depth);

	// alloc/free data
	if (!data || !_data || _data==RR_GHOST_BUFFER || width!=_width || height!=_height || depth!=_depth || format!=_format)
	{
		RR_SAFE_DELETE_ARRAY(data);
		// pointer value 1 = don't allocate buffer, caller promises he will never use it
		if ((_data || _format!=BF_DEPTH) && _data!=RR_GHOST_BUFFER)
		{
			data = new (std::nothrow) unsigned char[bytesTotal];
			if (!data)
			{
				RRReporter::report(ERRO,"Not enough memory, %s buffer not created.\n",RRReporter::bytesToString(bytesTotal));
				return false;
			}
		}
	}
	// copy data
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
		case BF_BGR:
			data[3*index+0] = RR_FLOAT2BYTE(element[2]);
			data[3*index+1] = RR_FLOAT2BYTE(element[1]);
			data[3*index+2] = RR_FLOAT2BYTE(element[0]);
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
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"setElement() not supported for compressed formats.\n"));
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
		RR_LIMITED_TIMES(1,RRReporter::report(WARN,"getElement() called on an uninitialized buffer.\n"));
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
		case BF_BGR:
			result[0] = RR_BYTE2FLOAT(data[ofs+2]);
			result[1] = RR_BYTE2FLOAT(data[ofs+1]);
			result[2] = RR_BYTE2FLOAT(data[ofs+0]);
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
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"getElement() not supported for compressed formats.\n"));
			break;
		default:
			RRReporter::report(WARN,"Unexpected buffer format.\n");
			break;
	}
	//RR_ASSERT(result[0]>=0); // bent normals may be negative
	return result;
}

RRVec4 RRBufferInMemory::getElementAtPosition(const RRVec3& position) const
{
	unsigned coord[3];
	coord[0] = (unsigned)(position[0] * width) % width;
	coord[1] = (unsigned)(position[1] * height) % height;
	coord[2] = (unsigned)(position[2] * depth) % depth;
	return getElement(coord[0]+coord[1]*width+coord[2]*width*height);
}

RRVec4 RRBufferInMemory::getElementAtDirection(const RRVec3& direction) const
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
				// 360*180 degree panorama
				float d = direction.x*direction.x+direction.z*direction.z;
				if (d)
				{
					float sin_angle = direction.x/sqrt(d);
					float angle = asin(sin_angle);
					if (direction.z<0) angle = (RRReal)(RR_PI-angle);
					coord[0] = (unsigned)( (angle*(-0.5f/RR_PI)+0.75f) * width) % width;
				}
				else
					coord[0] = 0;
				coord[1] = (unsigned)( (asin(direction.y/direction.length())*(1.0f/RR_PI)+0.5f) * height) % height;
				coord[2] = 0;
				break;
			}
	}
	return getElement(coord[0]+coord[1]*width+coord[2]*width*height);
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer* RRBuffer::create(RRBufferType _type, unsigned _width, unsigned _height, unsigned _depth, RRBufferFormat _format, bool _scaled, const unsigned char* _data)
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
		const RRVec4 data[6] = {upper,upper,upper,upper,upper,upper};
		return create(BT_CUBE_TEXTURE,1,1,6,BF_RGBAF,scaled,(const unsigned char*)data);
	}
	else
	{
		const RRVec4 data[24] = {
			upper,upper,lower,lower,
			upper,upper,lower,lower,
			upper,upper,upper,upper,
			lower,lower,lower,lower,//bottom
			upper,upper,lower,lower,
			upper,upper,lower,lower
		};
		return create(BT_CUBE_TEXTURE,2,2,6,BF_RGBAF,scaled,(const unsigned char*)data);
	}
}

}; // namespace
