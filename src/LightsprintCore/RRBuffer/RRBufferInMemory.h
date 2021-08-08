//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Buffer allocated in system memory.
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
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) override;
	virtual void setElement(unsigned index, const RRVec4& element, const RRColorSpace* colorSpace) override;

	// reading
	virtual RRBufferType getType() const override {return type;}
	virtual unsigned getWidth() const override {return width;}
	virtual unsigned getHeight() const override {return height;}
	virtual unsigned getDepth() const override {return depth;}
	virtual RRBufferFormat getFormat() const override {return format;}
	virtual bool getScaled() const override {return scaled;}
	virtual size_t getBufferBytes() const override;
	virtual RRVec4 getElement(unsigned index, const RRColorSpace* colorSpace) const override;
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRColorSpace* colorSpace, bool interpolated) const override;
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRColorSpace* colorSpace) const override;
	virtual unsigned char* lock(RRBufferLock lock) override {if (lock!=BL_READ)version++;return data;}
	virtual void unlock() override {}
	virtual bool isStub() override {return stub;}

	// reference counting
	void* operator new(std::size_t n);
	void operator delete(void* p, std::size_t n);
	RRBufferInMemory();
	virtual RRBuffer* createReference() override;
	virtual unsigned getReferenceCount() override {return refCount;}
	virtual ~RRBufferInMemory();
protected:
	// refCount is aligned and modified only by ++ and -- in createReference() and delete.
	// this and volatile makes it mostly thread safe, at least on x86
	// (still we clearly say it's not thread safe)
	// proper std::atomic<unsigned> or boost::detail::atomic_count would be probably more expensive
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
