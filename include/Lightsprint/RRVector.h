#ifndef RRVECTOR_H
#define RRVECTOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVector.h
//! \brief LightsprintCore | portable but limited std::vector replacement
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2008
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstdlib>

namespace rr
{

//! Portable but limited, minimalistic std::vector replacement.
//
//! Purpose of RRVector is to replace STL in public Lightsprint headers,
//! which makes Lightsprint work with any STL implementation.
//! It works like std::vector in simple cases demonstrated in Lightsprint SDK
//! where C is pointer, RRObject* or RRLight*.
//! It is not suitable for more complex operations.
template<class C>
class RRVector
{
public:
	//! Creates empty vector (like std::vector).
	RRVector()
	{
		numUsed = 0;
		numAllocated = 16;
		c = (C*)malloc(sizeof(C)*numAllocated);
	}
	//! Creates copy of vector.
	//! Does shallow copy of elements (unlike std::vector).
	RRVector(const RRVector& a)
	{
		numUsed = a.numUsed;
		numAllocated = a.numAllocated;
		c = (C*)malloc(sizeof(C)*numAllocated);
		memcpy(c,a.c,sizeof(C)*numUsed);
	}
	//! Assigns vector.
	//! Does shallow copy of elements (unlike std::vector).
	RRVector& operator=(const RRVector& a)
	{
		free(c);
		numUsed = a.numUsed;
		numAllocated = a.numAllocated;
		c = (C*)malloc(sizeof(C)*numAllocated);
		memcpy(c,a.c,sizeof(C)*numUsed);
		return *this;
	}
	//! Appends element at the end of vector.
	//! Elements may be relocated to different address in memory.
	//! Does shallow copy at relocation (unlike std::vector).
	void push_back(C a)
	{
		if(numUsed==numAllocated)
		{
			numAllocated *= 2;
			c = (C*)std::realloc(c,sizeof(C)*numAllocated);
		}
		c[numUsed++] = a;
	}
	//! Returns number of elements in vector (like std::vector).
	unsigned size() const
	{
		return numUsed;
	}
	//! Returns reference to i-th element (like std::vector).
	C& operator[](unsigned i)
	{
		assert(i<numUsed);
		return c[i];
	}
	//! Returns const reference to i-th element (like std::vector).
	const C& operator[](unsigned i) const
	{
		assert(i<numUsed);
		return c[i];
	}
	//! Clear all elements from vector, setting size to 0.
	//! Doesn't call element destructors (unlike std::vector).
	void clear()
	{
		numUsed = 0;
	}
	//! Frees elements.
	//! Doesn't call element destructors (unlike std::vector).
	~RRVector()
	{
		free(c);
	}
protected:
	C* c;
	unsigned numAllocated;
	unsigned numUsed;
};

} // namespace

#endif

