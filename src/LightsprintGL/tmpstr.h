// --------------------------------------------------------------------------
// Temporary formatted string, for internal use.
// Copyright (C) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
	#define STRINGS 12 // FpsDisplay::FpsDisplay requires at least 12
	static unsigned i = 0;
	i++;
	static char msg[STRINGS][1000];
	va_list argptr;
	va_start (argptr,fmt);
	_vsnprintf (msg[i%STRINGS],999,fmt,argptr);
	msg[i%STRINGS][999] = 0;
	va_end (argptr);
	return msg[i%STRINGS];
}

}; // namespace

#endif
