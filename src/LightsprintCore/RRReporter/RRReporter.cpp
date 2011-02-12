// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include "Lightsprint/GL/Timer.h"
#include <cstdio>

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

static int         g_indentation = 0;
static RRReporter* g_reporter = NULL;
bool               g_typeEnabled[TIMI+1] = {1,1,1,1,1,0,0,1};

void RRReporter::setFilter(bool warnings, unsigned infLevel, bool timing)
{
	g_typeEnabled[WARN] = warnings;
	g_typeEnabled[INF1] = infLevel>=1;
	g_typeEnabled[INF2] = infLevel>=2;
	g_typeEnabled[INF3] = infLevel>=3;
	g_typeEnabled[TIMI] = timing;
}

void RRReporter::indent(int delta)
{
	g_indentation += delta;
}

void RRReporter::reportV(RRReportType type, const char* format, va_list& vars)
{
	if (g_reporter && type>=ERRO && type<=TIMI && g_typeEnabled[type])
	{
		enum {MAX_REPORT_SIZE=1000};
		char msg[MAX_REPORT_SIZE+1];
		_vsnprintf(msg,MAX_REPORT_SIZE,format,vars);
		msg[MAX_REPORT_SIZE-1] = '\n';
		msg[MAX_REPORT_SIZE] = 0;
		g_reporter->customReport(type,g_indentation,msg);
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
	if (g_reporter)
	{
		report(ASSE,"%s in %s, file %s, line %d.\n",expression,func,file,line);
#if defined(_DEBUG) && defined(RR_STATIC) && defined(_MSC_VER)
		__debugbreak();
#endif
	}
}

void RRReporter::setReporter(RRReporter* _reporter)
{
	g_reporter = _reporter;
}

RRReporter* RRReporter::getReporter()
{
	return g_reporter;
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
