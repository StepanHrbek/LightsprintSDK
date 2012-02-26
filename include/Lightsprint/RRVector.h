#ifndef RRVECTOR_H
#define RRVECTOR_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVector.h
//! \brief LightsprintCore | portable but limited std::vector replacement
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2012
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include "RRDebug.h"

#if _MSC_VER>=1600 || __GNUC__>4 || (__GNUC__==4 && __GNUC_MINOR__>=3)
	#define RR_SUPPORTS_RVALUE_REFERENCES
#endif

namespace rr
{

//! Portable but limited, minimalistic std::vector replacement.
//
//! Purpose of RRVector is to replace STL in public Lightsprint headers,
//! to make Lightsprint work with any STL implementation.
//! It works like std::vector in simple cases in Lightsprint SDK interface
//! where C is pointer, RRObject* or RRLight*, and vector size is small
//! (no risk of allocation failure).
//! It is not suitable for anything more complex.
template<class C>
class RRVector
{
public:
	//! Creates empty vector (like std::vector).
	RRVector()
	{
		numUsed = 0;
		numAllocated = 0;
		c = NULL;
	}
	//! Creates vector by copying.
	//! Uses memcpy to copy elements (unlike std::vector).
	RRVector(const RRVector& a)
	{
		numUsed = a.numUsed;
		numAllocated = a.numAllocated;
		if (numAllocated)
		{
			c = (C*)malloc(sizeof(C)*numAllocated);
			memcpy(c,a.c,sizeof(C)*numUsed);
		}
		else
			c = NULL;
	}
	//! Assigns vector.
	//! Uses memcpy to copy elements (unlike std::vector).
	RRVector& operator=(const RRVector& a)
	{
		if (&a!=this)
		{
			free(c);
			numUsed = a.numUsed;
			numAllocated = a.numAllocated;
			if (numAllocated)
			{
				c = (C*)malloc(sizeof(C)*numAllocated);
				memcpy(c,a.c,sizeof(C)*numUsed);
			}
			else
				c = NULL;
		}
		return *this;
	}
#ifdef RR_SUPPORTS_RVALUE_REFERENCES
	//! Local replacement for std::swap
	template <class T> void swap(T& a, T& b)
	{
		T c(a); a=b; b=c;
	}
	//! Creates vector by moving.
	RRVector(RRVector&& a)
	{
		c = a.c;
		numAllocated = a.numAllocated;
		numUsed = a.numUsed;
		a.c = NULL;
		a.numAllocated = 0;
		a.numUsed = 0;
	}
	//! Moves vector.
	RRVector& operator=(RRVector&& a)
	{
		swap(c,a.c);
		swap(numAllocated,a.numAllocated);
		swap(numUsed,a.numUsed);
		return *this;
	}
#endif
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
			if (!numAllocated)
			{
				numAllocated = 16;
			}
			else
			{
				numAllocated *= 2;
			}
			c = (C*)std::realloc(c,sizeof(C)*numAllocated);
		}
		c[numUsed++] = a;
	}
	//! Removes last element from vector.
	//! Destructor is not called (unlike std::vector).
	void pop_back()
	{
		if (numUsed)
		{
			numUsed--;
		}
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
	const C* begin() const
	{
		return c;
	}
	//! Returns iterator pointing beyond last element (like std::vector).
	C* end()
	{
		return c+numUsed;
	}
	const C* end() const
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
	bool operator ==(const RRVector<C>& a) const
	{
		if (size()!=a.size())
			return false;
		for (unsigned i=0;i<size();i++)
		{
			if ((*this)[i]!=a[i])
				return false;
		}
		return true;
	}
	bool operator !=(const RRVector<C>& a) const
	{
		return !(a==*this);
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

