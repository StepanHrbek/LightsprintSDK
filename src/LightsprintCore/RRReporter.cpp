#include "Lightsprint/RRDebug.h"

#include <cassert>
#include <cstring>
#include <cstdio>
#ifdef WIN32
	#include <windows.h>
#endif
#include "Lightsprint/GL/Timer.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterFile

class RRReporterFile : public RRReporter
{
public:
	RRReporterFile(const char* filename)
	{
		file = fopen(filename,"wt");
	}
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		if(!file) return;
		// indentation
		char space[1000];
		space[0] = 0;
		indentation *= 2;
		if(indentation>0 && indentation<999)
		{
			memset(space,' ',indentation);
			space[indentation] = 0;
		}
		// type
		if(type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assert failed: "," Warn: "," inf1: "," inf2: "," inf3: ","","",""};
		// message
		// print
		fprintf(file,"%s%s%s",space,typePrefix[type],message);
	}
	~RRReporterFile()
	{
		if(file) fclose(file);
	}
	FILE* file;
};


#ifdef WIN32
/////////////////////////////////////////////////////////////////////////////
//
// RRReporterPrintf

class RRReporterPrintf : public RRReporter
{
public:
	RRReporterPrintf()
	{
		hconsole = GetStdHandle (STD_OUTPUT_HANDLE);
		currentColor = 7;
	}
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		char space[1000];
		space[0] = 0;
		indentation *= 2;
		if(indentation>0 && indentation<999)
		{
			memset(space,' ',indentation);
			space[indentation] = 0;
		}
		// type
		char typeColor[TIMI+1] = {15+16*4,14,15,7,7,7,7,6};
		SetConsoleTextAttribute(hconsole, currentColor = typeColor[type]);
		// message
		// print
		printf("%s%s%s",space,(type==ASSE)?"Assert failed: ":"",message);
	}
	HANDLE hconsole;
	char currentColor;
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
		if(type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assert failed: "," Warn: "," inf1: "," inf2: "," inf3: ","",""};
		OutputDebugString(typePrefix[type]);
		// message
		OutputDebugString(message);
	}
};
#endif

/////////////////////////////////////////////////////////////////////////////
//
// RRReporter

static int indentation = 0;
static RRReporter* reporter = NULL;
static bool typeEnabled[TIMI+1] = {1,1,1,1,1,0,0,1};

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
	if(reporter && type>=ERRO && type<=TIMI && typeEnabled[type])
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
#if defined(RR_STATIC) && defined(WIN32)
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

RRReporter* RRReporter::createFileReporter(const char* filename)
{
	return new RRReporterFile(filename);
}

RRReporter* RRReporter::createPrintfReporter()
{
#ifdef WIN32
	return new RRReporterPrintf;
#else
	return NULL;
#endif
}

RRReporter* RRReporter::createOutputDebugStringReporter()
{
#ifdef WIN32
	return new RRReporterOutputDebugString;
#else
	return NULL;
#endif
}


/////////////////////////////////////////////////////////////////////////////
//
// RRReportInterval

RRReportInterval::RRReportInterval(RRReportType type, const char* format, ...)
{
	enabled = type>=ERRO && type<=TIMI && typeEnabled[type];
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
		float doneIn = (float)((GETTIME-creationTime)/(float)PER_SEC);
		RRReporter::report(TIMI,(doneIn>1)?"done in %.1fs.\n":((doneIn>0.1f)?"done in %.2fs.\n":((doneIn>0.01f)?"done in %.0fms.\n":"done in %.1fms.\n")),(doneIn>0.1f)?doneIn:doneIn*1000);
		RRReporter::indent(-1);
	}
}

} //namespace
