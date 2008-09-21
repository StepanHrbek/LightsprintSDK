// --------------------------------------------------------------------------
// Reporting messages.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#ifdef _WIN32
	#include <windows.h>
#endif

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterOutputDebugString

#ifdef _WIN32
class RRReporterOutputDebugString : public RRReporter
{
public:
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// indentation
		indentation *= 2;
		if (indentation>0 && indentation<999)
		{
			char space[1000];
			memset(space,' ',indentation);
			space[indentation] = 0;
			OutputDebugString(space);
		}
		// type
		if (type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","",""};
		OutputDebugString(typePrefix[type]);
		// message
		OutputDebugString(message);
	}
};
#endif

RRReporter* RRReporter::createOutputDebugStringReporter()
{
#ifdef _WIN32
	return new RRReporterOutputDebugString;
#else
	return NULL;
#endif
}

} //namespace
