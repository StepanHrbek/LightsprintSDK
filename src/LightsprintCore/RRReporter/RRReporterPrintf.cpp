// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include <cstdio>
#include <cstring> // memset

#ifdef _WIN32
	#include <windows.h>
#endif

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterPrintf

#ifdef _WIN32
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
		if (indentation>0 && indentation<999)
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

#else

class RRReporterPrintf : public RRReporter
{
public:
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		char space[1000];
		space[0] = 0;
		indentation *= 2;
		if (indentation>0 && indentation<999)
		{
			memset(space,' ',indentation);
			space[indentation] = 0;
		}
		// print
		char typeAttr[TIMI+1] = {1,1,1,0,0,0,0,0};
		char typeFore[TIMI+1] = {37,33,37,37,37,37,37,36};
		char typeBack[TIMI+1] = {41,40,40,40,40,40,40,40};
		printf("%c[%d;%d;%dm%s%s%s",27,typeAttr[type],typeFore[type],typeBack[type],space,(type==ASSE)?"Assert failed: ":"",message);
	}
};
#endif

RRReporter* RRReporter::createPrintfReporter()
{
	return new RRReporterPrintf;
}

} //namespace
