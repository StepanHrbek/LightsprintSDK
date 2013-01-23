// --------------------------------------------------------------------------
// Temporary formatted string, for internal use.
// Copyright (C) 2008-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef TMPSTR_H
#define TMPSTR_H

#include <cstdio>
#include <cstdarg>

namespace rr_gl
{

//! Returns formatted string (printf-like) for immediate use.
//
//! Fully static, no allocations.
//! Has slots for several strings, call to tmpstr() overwrites one of previously returned strings.
inline const char* tmpstr(const char* fmt, ...)
{
	enum
	{
		MAX_STRINGS=12, // FpsDisplay::FpsDisplay requires at least 12
		MAX_STRING_SIZE=1000
	};
	static unsigned i = 0;
	static char bufs[MAX_STRINGS][MAX_STRING_SIZE+1];
	char* buf = bufs[++i%MAX_STRINGS];
	va_list argptr;
	va_start (argptr,fmt);
	_vsnprintf (buf,MAX_STRING_SIZE,fmt,argptr);
	buf[MAX_STRING_SIZE] = 0;
	va_end (argptr);
	return buf;
}

}; // namespace

#endif
