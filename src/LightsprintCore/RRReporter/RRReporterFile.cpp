// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Reporting messages to file.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include <cstring>
#include <cstdio>
#include <cstdlib> // free

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRReporterFile

class RRReporterFile : public RRReporter
{
public:
	static RRReporter* create(const char* filename, bool flush)
	{
		RRReporterFile* r = new RRReporterFile(filename,flush);
		if (!r->file) RR_SAFE_DELETE(r);
		return r;
	}
	virtual void customReport(RRReportType type, int indentation, const char* message)
	{
		// type
		if (type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","","",""};
		// message
		// print
		if (flush)
			file = fopen(filename,"at");
		if (file)
			fprintf(file,"%*s%s%s",2*indentation,"",typePrefix[type],message);
		if (file && flush)
		{
			fclose(file);
			file = nullptr;
		}
	}
	~RRReporterFile()
	{
		if (file) fclose(file);
		free(filename);
	}
private:
	RRReporterFile(const char* _filename, bool _flush)
	{
		filename = _strdup(_filename);
		file = fopen(filename,"wt");
		flush = _flush;
		if (file && flush)
			fclose(file);
	}
	char* filename;
	FILE* file;
	bool flush;
};

RRReporter* RRReporter::createFileReporter(const char* filename, bool caching)
{
	return RRReporterFile::create(filename,!caching);
}

} //namespace
