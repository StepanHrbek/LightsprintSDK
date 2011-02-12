// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/GL/Timer.h"

namespace rr
{

extern bool g_typeEnabled[TIMI+1];

/////////////////////////////////////////////////////////////////////////////
//
// RRReportInterval

RRReportInterval::RRReportInterval(RRReportType type, const char* format, ...)
{
	enabled = type>=ERRO && type<=TIMI && g_typeEnabled[type];
	if (enabled)
	{
		creationTime = GETTIME;
	}
	va_list argptr;
	va_start (argptr,format);
	RRReporter::reportV(type,format,argptr);
	va_end (argptr);
	if (enabled)
	{
		RRReporter::indent(+1);
	}
}

RRReportInterval::~RRReportInterval()
{
	if (enabled)
	{
		float doneIn = (float)((GETTIME-creationTime)/(float)PER_SEC);
		RRReporter::report(TIMI,(doneIn>1)?"done in %.1fs.\n":((doneIn>0.1f)?"done in %.2fs.\n":((doneIn>0.01f)?"done in %.0fms.\n":"done in %.1fms.\n")),(doneIn>0.1f)?doneIn:doneIn*1000);
		RRReporter::indent(-1);
	}
}

} //namespace
