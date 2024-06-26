// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Base class for safer object allocation/freeing.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib> // malloc, free
#include <cstring> // memcpy, memset
#include <errno.h> // errno
#include <wchar.h>


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
	str = nullptr;
	wstr = nullptr;
}

RRString::RRString(const RRString& a)
{
	if (a.str)
	{
		size_t bytes1 = (char*)a.wstr-a.str;
		size_t bytes2 = (wcslen(a.wstr)+1)*sizeof(wchar_t);
		str = (char*)malloc(bytes1+bytes2);
		if (!str)
			goto fail;
		memcpy(str,a.str,bytes1+bytes2);
		wstr = (wchar_t*)(str+bytes1);
	}
	else
	{
		fail:
		str = nullptr;
		wstr = nullptr;
	}
}

RRString::RRString(const char* a)
{
	if (a&&a[0])
	{
		size_t bytes1 = strlen(a)+1;
		size_t bytesp = (bytes1+sizeof(wchar_t)-1)/sizeof(wchar_t)*sizeof(wchar_t)-bytes1; // padding, linux/gcc needs wchars aligned
		size_t wchars = mbstowcs(nullptr,a,0)+1; // +1 for null terminator
		size_t bytes2 = (wchars+1)*sizeof(wchar_t); // +1 for null terminator even in case of invalid string (mbstowcs=-1)
		RR_ASSERT(bytes1>0);
		RR_ASSERT(bytes2>0);
		str = (char*)malloc(bytes1+bytesp+bytes2);
		if (!str)
			goto fail;
		wstr = (wchar_t*)(str+bytes1+bytesp);
		memcpy(str,a,bytes1);
		memset(str+bytes1,0,bytesp);
		size_t result = mbstowcs(wstr,a,wchars); // VS2008 runtime sometimes fails if third parameter is INT_MAX
		if (result==(size_t)-1)
		{
			wstr[0] = 0; // cleanup in case of invalid a / mbstowcs failure
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"RRString(char*) failed, errno=%d, string=%s\n",(int)errno,a));
		}
	}
	else
	{
		fail:
		str = nullptr;
		wstr = nullptr;
	}
}

RRString::RRString(const wchar_t* a)
{
	if (a&&a[0])
	{
		size_t bytes2 = (wcslen(a)+1)*sizeof(wchar_t);
		size_t bytes1 = bytes2*2; // estimate worst case
		RR_ASSERT(bytes1>0);
		RR_ASSERT(bytes2>0);
		str = (char*)malloc(bytes1+bytes2);
		if (!str)
			goto fail;
		wstr = (wchar_t*)(str+bytes1);
		memcpy(wstr,a,bytes2);
		mbstate_t mbstate;
		memset(&mbstate,0,sizeof(mbstate));
		char* stri = str;
		const wchar_t* wstri = wstr;
		while (*wstri)
		{
			size_t bytesWritten = wcrtomb(stri,*wstri,&mbstate);
			if (bytesWritten!=(size_t)-1)
				stri += bytesWritten;
			else
				*(stri++) = '?';
			wstri++;
		}
		*stri = 0;
	}
	else
	{
		fail:
		str = nullptr;
		wstr = nullptr;
	}
}

void RRString::clear()
{
	free(str);
	str = nullptr;
	wstr = nullptr;
}

static void form(RRString* thiz, const wchar_t* fmt, va_list& argptr)
{
	size_t bufSize = 1000;
	wchar_t buf[1000];
	int characters = vswprintf(buf,bufSize,fmt,argptr);
	if (characters>=0)
	{
		*thiz = buf;
	}
	else
	while (1)
	{
		bufSize *= 10;
		wchar_t* buf = new (std::nothrow) wchar_t[bufSize];
		if (!buf)
		{
			RR_ASSERT(0);
			*thiz = "format() error";
			return;
		}
		characters = vswprintf(buf,bufSize,fmt,argptr);
		if (characters>=0)
		{
			*thiz = buf;
			delete[] buf;
			return;
		}
		delete[] buf;
	}
}

RRString::RRString(unsigned zero, const wchar_t* fmt, ...)
{
	str = nullptr;
	wstr = nullptr;
	va_list argptr;
	va_start(argptr,fmt);
	form(this,fmt,argptr);
}

void RRString::format(const wchar_t* fmt, ...)
{
	va_list argptr;
	va_start(argptr,fmt);
	form(this,fmt,argptr);
}

RRString& RRString::operator =(const RRString& a)
{
	free(str);
	::new(this) RRString(a);
	return *this;
}

RRString& RRString::operator =(const char* a)
{
	free(str);
	::new(this) RRString(a);
	return *this;
}

RRString& RRString::operator =(const wchar_t* a)
{
	free(str);
	::new(this) RRString(a);
	return *this;
}

bool RRString::operator ==(const RRString& a) const
{
	return (!wstr && !a.wstr) || (wstr && a.wstr && !wcscmp(wstr,a.wstr));
}

bool RRString::operator ==(const char* a) const
{
	return (!str && (!a || !a[0])) || (str && a && a[0] && !strcmp(str,a));
}

bool RRString::operator ==(const wchar_t* a) const
{
	return (!wstr && (!a || !a[0])) || (wstr && a && a[0] && !wcscmp(wstr,a));
}

bool RRString::operator !=(const RRString& a) const
{
	return !(*this==a);
}

bool RRString::operator !=(const char* a) const
{
	return !(*this==a);
}

bool RRString::operator !=(const wchar_t* a) const
{
	return !(*this==a);
}

void RRString::_skipDestructor()
{
	// Refcounting in ~RRBuffer may decide it's not yet time to destruct.
	// It can't stop ~RRString from beeing called, so it at least instructs us in advance
	// to ignore following ~RRString call. We set this flag for ~RRString.
	str++;
}

RRString::~RRString()
{
	if (((size_t)str)&1)
	{
		// don't destruct this time, just clear flag from _skipDestructor()
		// volatile prevents gcc from optimizing parts of destructor away
		((char *volatile*)(&str))[0] --;
	}
	else
	{
		// destruct as usual
		free(str);
	}
}

} //namespace
