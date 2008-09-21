// --------------------------------------------------------------------------
// Reporting messages.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRDebug.h"
#include <cstring>
#include <cstdio>

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
		if (type<ERRO || type>TIMI) type = INF9;
		static const char* typePrefix[] = {"ERROR: ","Assertion failed: ","Warning: ","","","","","",""};
		// message
		// print
		if (flush)
			file = fopen(filename,"at");
		if (file)
			fprintf(file,"%s%s%s",space,typePrefix[type],message);
		if (file && flush)
		{
			fclose(file);
			file = NULL;
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
