// --------------------------------------------------------------------------
// Temporary formatted string, for internal use.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef TMPSTR_H
#define TMPSTR_H

#include <cstdio>

namespace rr_gl
{

//! Returns formatted string (printf-like) for immediate use.
//
//! Has slots for 4 string, fifth tmpstr() overwrites first one.
//! Fully static, no allocation.
inline const char* tmpstr(const char* fmt, ...)
{
	#define STRINGS 4
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
