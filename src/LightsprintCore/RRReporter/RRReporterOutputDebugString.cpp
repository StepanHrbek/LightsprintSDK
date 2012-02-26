// --------------------------------------------------------------------------
// Reporting messages.
// Copyright (c) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"

#ifdef _WIN32

#include <windows.h>

namespace rr
{

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

RRReporter* RRReporter::createOutputDebugStringReporter()
{
	return new RRReporterOutputDebugString;
}

} //namespace

#else // _WIN32

rr::RRReporter* rr::RRReporter::createOutputDebugStringReporter()
{
	return NULL;
}

#endif // !_WIN32
