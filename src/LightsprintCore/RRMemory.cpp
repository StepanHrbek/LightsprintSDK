// --------------------------------------------------------------------------
// Base class for safer object allocation/freeing.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#include <stdlib.h> // malloc, free


namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRUniformlyAllocated

void* RRUniformlyAllocated::operator new(std::size_t n)
{
	return malloc(n);
};

void* RRUniformlyAllocated::operator new[](std::size_t n)
{
	return malloc(n);
};

void RRUniformlyAllocated::operator delete(void* p, std::size_t n)
{
	if (p) free(p);
};

void RRUniformlyAllocated::operator delete[](void* p, std::size_t n)
{
	if (p) free(p);
};


/////////////////////////////////////////////////////////////////////////////
//
// RRAligned

void* AlignedMalloc(size_t size,int byteAlign)
{
	void *mallocPtr = malloc(size + byteAlign + sizeof(void*));
	RR_ASSERT(mallocPtr);
	size_t ptrInt = (size_t)mallocPtr;

	ptrInt = (ptrInt + byteAlign + sizeof(void*)) / byteAlign * byteAlign;
	*(((void**)ptrInt) - 1) = mallocPtr;

	return (void*)ptrInt;
}

void AlignedFree(void *ptr)
{
	free(*(((void**)ptr) - 1));
}

void* RRAligned::operator new(std::size_t n)
{
	return AlignedMalloc(n,16);
};

void* RRAligned::operator new[](std::size_t n)
{
	return AlignedMalloc(n,16);
};

void RRAligned::operator delete(void* p, std::size_t n)
{
	if (p) AlignedFree(p);
};

void RRAligned::operator delete[](void* p, std::size_t n)
{
	if (p) AlignedFree(p);
};

} //namespace
