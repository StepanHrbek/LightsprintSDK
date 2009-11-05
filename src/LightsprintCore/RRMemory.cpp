// --------------------------------------------------------------------------
// Base class for safer object allocation/freeing.
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#include <cstdlib> // malloc, free
#include <cstring> // _strdup


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


/////////////////////////////////////////////////////////////////////////////
//
// RRString

RRString::RRString()
{
	str = NULL;
}

RRString::RRString(const RRString& a)
{
	str = a.str?_strdup(a.str):NULL;
}

RRString::RRString(const char* a)
{
	str = (a&&a[0])?_strdup(a):NULL;
}

RRString& RRString::operator =(const RRString& a)
{
	free(str);
	str = a.str?_strdup(a.str):NULL;
	return *this;
}

RRString& RRString::operator =(const char* a)
{
	free(str);
	str = (a&&a[0])?_strdup(a):NULL;
	return *this;
}

bool RRString::operator ==(const RRString& a) const
{
	return (!str && !a.str) || (str && a.str && !strcmp(str,a.str));
}

bool RRString::operator ==(const char* a) const
{
	return (!str && (!a || !a[0])) || (str && a && a[0] && !strcmp(str,a));
}

bool RRString::operator !=(const RRString& a) const
{
	return !(*this==a);
}

bool RRString::operator !=(const char* a) const
{
	return !(*this==a);
}

RRString::~RRString()
{
	free(str);
}

} //namespace
