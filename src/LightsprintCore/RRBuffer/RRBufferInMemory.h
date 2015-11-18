// --------------------------------------------------------------------------
// Buffer stored in system memory.
// Copyright (c) 2006-2014 Stepan Hrbek, Lightsprint. All rights reserved.
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
	virtual void setElement(unsigned index, const RRVec4& element, const RRScaler* scaler);

	// reading
	virtual RRBufferType getType() const {return type;}
	virtual unsigned getWidth() const {return width;}
	virtual unsigned getHeight() const {return height;}
	virtual unsigned getDepth() const {return depth;}
	virtual RRBufferFormat getFormat() const {return format;}
	virtual bool getScaled() const {return scaled;}
	virtual unsigned getBufferBytes() const;
	virtual RRVec4 getElement(unsigned index, const RRScaler* scaler) const;
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRScaler* scaler) const;
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRScaler* scaler) const;
	virtual unsigned char* lock(RRBufferLock lock) {if (lock!=BL_READ)version++;return data;}
	virtual void unlock() {}
	virtual bool isStub() {return stub;}

	// reference counting
	void* operator new(std::size_t n);
	void operator delete(void* p, std::size_t n);
	RRBufferInMemory();
	virtual RRBuffer* createReference();
	virtual unsigned getReferenceCount() {return refCount;}
	virtual ~RRBufferInMemory();
protected:
	// refCount is aligned and modified only by ++ and -- in createReference() and delete.
	// this and volatile makes it mostly thread safe, at least on x86
	// (still we clearly say it's not thread safe)
	volatile unsigned refCount;

	RRBufferType type;
	unsigned width;
	unsigned height;
	unsigned depth;
	RRBufferFormat format;
	bool scaled;
public: // for RRBuffer::load()
	bool stub;
protected:
	unsigned char* data;
};

}; // namespace

#endif
