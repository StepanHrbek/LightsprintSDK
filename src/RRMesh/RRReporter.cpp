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
	virtual void customReport(Type type, const char* message)
	{
		printf("%s",message);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// RRReporterOutputDebugString

class RRReporterOutputDebugString : public RRReporter
{
public:
	virtual void customReport(Type type, const char* message)
	{
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
		strcpy(msg,(type==ERRO)?"ERROR: ":((type==ASSE)?"Assert ":((type==WARN)?" Warn: ":((type==INFO)?" info: ":""))));
		_vsnprintf (msg+7,999-7,format,argptr);
		msg[999] = 0;
		va_end (argptr);
		reporter->customReport(type,(type==CONT)?msg+7:msg);
	}
}

void RRReporter::assertionFailed(const char* expression, const char* file, unsigned line)
{
	if(reporter)
	{
		report(ASSE,"FAILED: %s, file %s, line %d.\n",expression,file,line);
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
