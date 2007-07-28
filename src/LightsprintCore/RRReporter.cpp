#include "Lightsprint/RRDebug.h"

#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Lightsprint/GL/Timer.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterPrintf

class RRReporterPrintf : public RRReporter
{
public:
	RRReporterPrintf()
	{
		hconsole = GetStdHandle (STD_OUTPUT_HANDLE);
	}
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		indentation *= 2;
		if(indentation>0 && indentation<999)
		{
			char space[1000];
			memset(space,' ',indentation);
			space[indentation] = 0;
			printf(space);
		}
		// type
		if(type!=CONT)
		{
			char typeColor[CONT] = {15+16*4,14,15,7,7,7,6};
			SetConsoleTextAttribute(hconsole, typeColor[type]);
		}
		// message
		printf("%s%s",(type==ASSE)?"Assert failed: ":"",message);
	}
	HANDLE hconsole;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRReporterOutputDebugString

class RRReporterOutputDebugString : public RRReporter
{
public:
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		indentation *= 2;
		if(indentation>0 && indentation<999)
		{
			char space[1000];
			memset(space,' ',indentation);
			space[indentation] = 0;
			OutputDebugString(space);
		}
		// type
		if(type<ERRO || type>CONT) type = CONT;
		static const char* typePrefix[] = {"ERROR: ","Assert failed: "," Warn: "," inf1: "," inf2: "," inf3: ","",""};
		OutputDebugString(typePrefix[type]);
		// message
		OutputDebugString(message);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

static int indentation = 0;
static RRReporter* reporter = NULL;
static bool typeEnabled[CONT+1] = {1,1,1,1,1,0,1,1};

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
	if(reporter && type>=ERROR && type<=CONT && typeEnabled[type])
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
	if(reporter)
	{
		report(ASSE,"%s in %s, file %s, line %d.\n",expression,func,file,line);
#ifdef RR_STATIC
		DebugBreak();
#endif
	}
}

void RRReporter::setReporter(RRReporter* areporter)
{
	reporter = areporter;
}

RRReporter* RRReporter::getReporter()
{
	return reporter;
}

RRReporter* RRReporter::createPrintfReporter()
{
	return new RRReporterPrintf;
}

RRReporter* RRReporter::createOutputDebugStringReporter()
{
	return new RRReporterOutputDebugString;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRReportInterval

RRReportInterval::RRReportInterval(RRReportType type, const char* format, ...)
{
	enabled = type>=ERROR && type<=CONT && typeEnabled[type];
	if(enabled)
	{
		creationTime = GETTIME;
		va_list argptr;
		va_start (argptr,format);
		RRReporter::reportV(type,format,argptr);
		va_end (argptr);
		RRReporter::indent(+1);
	}
}

RRReportInterval::~RRReportInterval()
{
	if(enabled)
	{
		RRReporter::report(TIMI,"done in %.1fs.\n",(GETTIME-creationTime)/(float)PER_SEC);
		RRReporter::indent(-1);
	}
}

} //namespace
