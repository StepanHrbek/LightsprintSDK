#include "Lightsprint/RRMesh.h"

#include <cassert>
#include <cstdio>
#include <windows.h>

namespace rr
{

static RRReporter* reporter = NULL;


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
	virtual void customReport(Type type, const char* message)
	{
		if(type!=CONT)
		{
			char color[4] = {15+3,14+3,15,7};
			SetConsoleTextAttribute(hconsole, color[type]);
		}
		printf("%s",message);
	}
	HANDLE hconsole;
};


/////////////////////////////////////////////////////////////////////////////
//
// RRReporterOutputDebugString

class RRReporterOutputDebugString : public RRReporter
{
public:
	virtual void customReport(Type type, const char* message)
	{
		OutputDebugString((type==ERRO)?"ERROR: ":((type==ASSE)?"Assert failed: ":((type==WARN)?" Warn: ":((type==INFO)?" info: ":""))));
		OutputDebugString(message);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

void RRReporter::report(Type type, const char* format, ...)
{
	if(reporter)
	{
		char msg[1000];
		va_list argptr;
		va_start (argptr,format);
		_vsnprintf (msg,999,format,argptr);
		msg[999] = 0;
		va_end (argptr);
		reporter->customReport(type,msg);
	}
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

} //namespace
