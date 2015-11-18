// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Generic code for reporting messages.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include <cstdio>
#include <set>

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

static int                   g_indentation = 0;
static std::set<RRReporter*> g_reporters;
bool                         g_typeEnabled[TIMI+1] = {1,1,1,1,1,0,0,1};

RRReporter::RRReporter()
{
#pragma omp critical(reporter)
	g_reporters.insert(this);
}

RRReporter::~RRReporter()
{
#pragma omp critical(reporter)
	g_reporters.erase(this);
}

void RRReporter::setFilter(bool warnings, unsigned infLevel, bool timing)
{
	g_typeEnabled[WARN] = warnings;
	g_typeEnabled[INF1] = infLevel>=1;
	g_typeEnabled[INF2] = infLevel>=2;
	g_typeEnabled[INF3] = infLevel>=3;
	g_typeEnabled[TIMI] = timing;
}

void RRReporter::getFilter(bool& warnings, unsigned& infLevel, bool& timing)
{
	warnings = g_typeEnabled[WARN];
	infLevel = g_typeEnabled[INF3]?3:(g_typeEnabled[INF2]?2:1);
	timing = g_typeEnabled[TIMI];
}

void RRReporter::indent(int delta)
{
	g_indentation += delta;
}

void RRReporter::reportV(RRReportType type, const char* format, va_list& vars)
{
	if (g_reporters.size() && type>=ERRO && type<=TIMI && g_typeEnabled[type])
	{
		enum {MAX_REPORT_SIZE=1000};
		char msg[MAX_REPORT_SIZE+1];
		_vsnprintf(msg,MAX_REPORT_SIZE,format,vars);
		msg[MAX_REPORT_SIZE-1] = '\n';
		msg[MAX_REPORT_SIZE] = 0;
		for (std::set<RRReporter*>::iterator i=g_reporters.begin();i!=g_reporters.end();++i)
			(*i)->customReport(type,g_indentation,msg);
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
	if (g_reporters.size())
	{
		report(ASSE,"%s in %s, file %s, line %d.\n",expression,func,file,line);
#if defined(_DEBUG) && defined(RR_STATIC) && defined(_MSC_VER)
		__debugbreak();
#endif
	}
}

const char* RRReporter::bytesToString(size_t bytes)
{
	static char buf[20];
	sprintf(buf,(bytes<1024)?"%d B":((bytes<1024*1024)?"%d KB":"%d MB"),(unsigned)((bytes<1024)?bytes:((bytes<1024*1024)?bytes/1024:bytes/1024/1024)));
	return buf;
}

} //namespace
