// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Temporary formatted string, for internal use.
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
		MAX_STRINGS=2,
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
