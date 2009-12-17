// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
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
	// setting
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data);
	virtual void setElement(unsigned index, const RRVec4& element);

	// reading
	virtual RRBufferType getType() const {return type;}
	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual unsigned getDepth() const {return depth;}
	virtual RRBufferFormat getFormat() const {return format;}
	virtual bool getScaled() const {return scaled;}
	virtual unsigned getBufferBytes() const;
	virtual RRVec4 getElement(unsigned index) const;
	virtual RRVec4 getElement(const RRVec3& coord) const;
	virtual unsigned char* lock(RRBufferLock lock) {if (lock!=BL_READ)version++;return data;}
	virtual void unlock() {}

	// reference counting
	void* operator new(std::size_t n);
	void operator delete(void* p, std::size_t n);
	RRBufferInMemory();
	virtual RRBuffer* createReference();
	virtual ~RRBufferInMemory();
protected:
	unsigned refCount;

	RRBufferType type;
	unsigned width;
	unsigned height;
	unsigned depth;
	RRBufferFormat format;
	bool scaled;
	unsigned char* data;
};

}; // namespace

#endif
