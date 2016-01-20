// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Reporting messages with OutputDebugString().
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
			OutputDebugStringA(space);
		}
		// type
		if (type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","",""};
		OutputDebugStringA(typePrefix[type]);
		// message
		OutputDebugStringA(message);
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
	return nullptr;
}

#endif // !_WIN32
