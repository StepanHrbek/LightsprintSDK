#ifndef RRVECTOR_H
#define RRVECTOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVector.h
//! \brief LightsprintCore | portable but limited std::vector replacement
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include "RRDebug.h"

namespace rr
{

//! Portable but limited, minimalistic std::vector replacement.
//
//! Purpose of RRVector is to replace STL in public Lightsprint headers,
//! to make Lightsprint work with any STL implementation.
//! It works like std::vector in simple cases in Lightsprint SDK interface
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
	//! Uses memcpy to copy elements (unlike std::vector).
	RRVector(const RRVector& a)
	{
		numUsed = a.numUsed;
		numAllocated = a.numAllocated;
		c = (C*)malloc(sizeof(C)*numAllocated);
		memcpy(c,a.c,sizeof(C)*numUsed);
	}
	//! Assigns vector.
	//! Uses memcpy to copy elements (unlike std::vector).
	RRVector& operator=(const RRVector& a)
	{
		free(c);
		numUsed = a.numUsed;
		numAllocated = a.numAllocated;
		c = (C*)malloc(sizeof(C)*numAllocated);
		memcpy(c,a.c,sizeof(C)*numUsed);
		return *this;
	}
	//! Resizes vector, adding or removing elements at the end.
	//! Does not destruct removed elements (unlike std::vector).
	void resize(unsigned newSize, C initial=C())
	{
		while (size()<newSize)
			push_back(initial);
		numUsed = newSize;
	}
	//! Appends element at the end of vector.
	//! Elements may be relocated to different address in memory.
	//! Does shallow copy at relocation (unlike std::vector).
	void push_back(C a)
	{
		if (numUsed==numAllocated)
		{
			numAllocated *= 2;
			c = (C*)std::realloc(c,sizeof(C)*numAllocated);
		}
		c[numUsed++] = a;
	}
	//! Removes i-th element from vector (i is not iterator unlike std::vector).
	//! Destructor is not called (unlike std::vector).
	void erase(unsigned i)
	{
		if (numUsed)
		{
			numUsed--;
			for (;i<numUsed;i++) c[i] = c[i+1];
		}
	}
	//! Returns number of elements in vector (like std::vector).
	unsigned size() const
	{
		return numUsed;
	}
	//! Returns reference to i-th element (like std::vector).
	C& operator[](unsigned i)
	{
		RR_ASSERT(i<numUsed);
		return c[i];
	}
	//! Returns const reference to i-th element (like std::vector).
	const C& operator[](unsigned i) const
	{
		RR_ASSERT(i<numUsed);
		return c[i];
	}
	//! Clear all elements from vector, setting size to 0.
	//! Doesn't call element destructors (unlike std::vector).
	void clear()
	{
		numUsed = 0;
	}
	//! Returns iterator pointing to first element (like std::vector).
	C* begin()
	{
		return c;
	}
	//! Returns iterator pointing beyond last element (like std::vector).
	C* end()
	{
		return c+numUsed;
	}
	//! Inserts range of elements.
	//! Destination must be end of current vector (unlike std::vector).
	void insert(C* _where, const C* _first, const C* _last)
	{
		if (_where==end())
		{
			// append
			for (;_first<_last;_first++)
			{
				push_back(*_first);
			}
		}
		else
		{
			// insert, not supported yet
			RR_ASSERT(0);
		}
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

