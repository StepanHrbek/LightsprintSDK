// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (C) Lightsprint, Stepan Hrbek, 2006-2007
// --------------------------------------------------------------------------

#ifndef BUFFERINMEMORY_H
#define BUFFERINMEMORY_H

#include "Lightsprint/RRBuffer.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRBufferInMemory

class RR_API RRBufferInMemory : public RRBuffer
{
public:
	RRBufferInMemory(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, const unsigned char *data);

	// setting
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, const unsigned char* data);
	virtual void setElement(unsigned index, const RRVec3& element);

	// reading
	virtual RRBufferType getType() const {return type;}
	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual unsigned getDepth() const {return depth;}
	virtual RRBufferFormat getFormat() const {return format;}
	virtual unsigned getElementBits() const;
	virtual RRVec4 getElement(unsigned index) const;
	virtual RRVec4 getElement(const RRVec3& coord) const;
	virtual unsigned char* lock(RRBufferLock lock) {return data;}
	virtual void unlock() {}

	virtual ~RRBufferInMemory();

protected:
	RRBufferType type;
	unsigned width;
	unsigned height;
	unsigned depth;
	RRBufferFormat format;
	unsigned char* data;
};

}; // namespace

#endif RRBUFFERINMEMORY_H
