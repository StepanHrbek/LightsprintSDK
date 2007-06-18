#ifndef RRVECTOR_H
#define RRVECTOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVector.h
//! \brief RRVector - portable std::vector replacement
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>

namespace rr
{

//! Portable and limited std::vector replacement.
//
//! Purpose of RRVector is to replace STL in public Lightsprint headers,
//! which makes Lightsprint work with any STL implementation.
//! Warning: It is only similar to std::vector.
template<class C>
class RRVector
{
public:
	RRVector()
	{
		numUsed = 0;
		numAllocated = 16;
		c = (C*)malloc(sizeof(C)*numAllocated);
	}
	void push_back(C a)
	{
		if(numUsed==numAllocated)
		{
			numAllocated *= 2;
			c = (C*)realloc(c,sizeof(C)*numAllocated);
		}
		c[numUsed++] = a;
	}
	unsigned size() const
	{
		return numUsed;
	}
	C& operator[](unsigned i)
	{
		assert(i<numUsed);
		return c[i];
	}
	const C& operator[](unsigned i) const
	{
		assert(i<numUsed);
		return c[i];
	}
	~RRVector()
	{
		free(c);
	}
private:
	C* c;
	unsigned numAllocated;
	unsigned numUsed;
};

} // namespace

#endif

