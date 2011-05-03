// --------------------------------------------------------------------------
// Base class for safer object allocation/freeing.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib> // malloc, free
#include <cstring> // _strdup
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
	str = NULL;
	wstr = NULL;
}

RRString::RRString(const RRString& a)
{
	if (a.str)
	{
		size_t bytes1 = (char*)a.wstr-a.str;
		size_t bytes2 = (wcslen(a.wstr)+1)*sizeof(wchar_t);
		str = (char*)malloc(bytes1+bytes2);
		memcpy(str,a.str,bytes1+bytes2);
		wstr = (wchar_t*)(str+bytes1);
	}
	else
	{
		str = NULL;
		wstr = NULL;
	}
}

RRString::RRString(const char* a)
{
	if (a&&a[0])
	{
		size_t bytes1 = strlen(a)+1;
		size_t wchars = mbstowcs(NULL,a,0)+1; // +1 for null terminator
		size_t bytes2 = (wchars+1)*sizeof(wchar_t); // +1 for null terminator even in case of invalid string (mbstowcs=-1)
		RR_ASSERT(bytes1>0);
		RR_ASSERT(bytes2>0);
		str = (char*)malloc(bytes1+bytes2);
		wstr = (wchar_t*)(str+bytes1);
		memcpy(str,a,bytes1);
		size_t result = mbstowcs(wstr,a,wchars); // VS2008 runtime sometimes fails if third parameter is INT_MAX
		if (result==(size_t)-1)
		{
			wstr[0] = 0; // cleanup in case of invalid a / mbstowcs failure
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"RRString(char*) failed, errno=%d, string=%s\n",(int)errno,a));
		}
	}
	else
	{
		str = NULL;
		wstr = NULL;
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
		wstr = (wchar_t*)(str+bytes1);
		memcpy(wstr,a,bytes2);
		mbstate_t mbstate;
		memset(&mbstate,0,sizeof(mbstate));
		char* stri = str;
		const wchar_t* wstri = wstr;
		while (*wstri)
		{
			int bytesWritten = wcrtomb(stri,*wstri,&mbstate);
			if (bytesWritten>=0)
				stri += bytesWritten;
			else
				*(stri++) = '?';
			wstri++;
		}
		*stri = 0;
	}
	else
	{
		str = NULL;
		wstr = NULL;
	}
}

void RRString::clear()
{
	free(str);
	str = NULL;
	wstr = NULL;
}

void RRString::format(const wchar_t* fmt, ...)
{
	va_list argptr;
	size_t bufSize = 1000;
	wchar_t buf[1000];
	va_start(argptr,fmt);
	int characters = _vsnwprintf(buf,bufSize-1,fmt,argptr);
	if (characters>=0)
	{
		buf[bufSize-1] = 0;
		*this = buf;
	}
	else
	while (1)
	{
		bufSize *= 10;
		wchar_t* buf = new (std::nothrow) wchar_t[bufSize];
		if (!buf)
		{
			RR_ASSERT(0);
			*this = "format() error";
			return;
		}
		characters = _vsnwprintf(buf,bufSize-1,fmt,argptr);
		if (characters>=0)
		{
			buf[bufSize-1] = 0;
			*this = buf;
			delete[] buf;
			return;
		}
		delete[] buf;
	}
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
		str--;
	}
	else
	{
		// destruct as usual
		free(str);
	}
}

} //namespace
