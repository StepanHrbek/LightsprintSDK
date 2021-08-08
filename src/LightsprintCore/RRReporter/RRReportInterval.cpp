// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Reporting intervals.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

namespace rr
{

extern bool g_typeEnabled[TIMI+1];

/////////////////////////////////////////////////////////////////////////////
//
// RRReportInterval

RRReportInterval::RRReportInterval(RRReportType type, const char* format, ...)
	: time(false)
{
	enabled = type>=ERRO && type<=TIMI && g_typeEnabled[type];
	va_list argptr;
	va_start (argptr,format);
	RRReporter::reportV(type,format,argptr);
	va_end (argptr);
	if (enabled)
	{
		time.setNow();
		RRReporter::indent(+1);
	}
}

RRReportInterval::~RRReportInterval()
{
	if (enabled)
	{
		float doneIn = time.secondsPassed();
		RRReporter::report(TIMI,(doneIn>1)?"done in %.1fs.\n":((doneIn>0.1f)?"done in %.2fs.\n":((doneIn>0.01f)?"done in %.0fms.\n":"done in %.1fms.\n")),(doneIn>0.1f)?doneIn:doneIn*1000);
		RRReporter::indent(-1);
	}
}

} //namespace
