// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/GL/Timer.h"
#include <cstdio>

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

static int indentation = 0;
static RRReporter* reporter = NULL;
bool typeEnabled[TIMI+1] = {1,1,1,1,1,0,0,1};

void RRReporter::setFilter(bool warnings, unsigned infLevel, bool timing)
{
	typeEnabled[WARN] = warnings;
	typeEnabled[INF1] = infLevel>=1;
	typeEnabled[INF2] = infLevel>=2;
	typeEnabled[INF3] = infLevel>=3;
	typeEnabled[TIMI] = timing;
}

void RRReporter::indent(int delta)
{
	indentation += delta;
}

void RRReporter::reportV(RRReportType type, const char* format, va_list& vars)
{
	if (reporter && type>=ERRO && type<=TIMI && typeEnabled[type])
	{
		char msg[1000];
		_vsnprintf(msg,999,format,vars);
		msg[999] = 0;
		reporter->customReport(type,indentation,msg);
	}
}

void RRReporter::report(RRReportType type, const char* format, ...)
{
	va_list argptr;
	va_start (argptr,format);
	reportV(type,format,argptr);
	va_end (argptr);
}

void RRReporter::assertionFailed(const char* expression, const char* func, const char* file, unsigned line)
{
	if (reporter)
	{
		report(ASSE,"%s in %s, file %s, line %d.\n",expression,func,file,line);
#if defined(_DEBUG) && defined(RR_STATIC) && defined(_MSC_VER)
		__debugbreak();
#endif
	}
}

void RRReporter::setReporter(RRReporter* _reporter)
{
	reporter = _reporter;
}

RRReporter* RRReporter::getReporter()
{
	return reporter;
}

RRReporter::~RRReporter()
{
	// This reporter no longer exists, stop sending messages to it.
	if (RRReporter::getReporter()==this)
	{
		RRReporter::setReporter(NULL);
	}
}

} //namespace
