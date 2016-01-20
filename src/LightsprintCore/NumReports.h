// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Reporting helper used by checkConsistency().
// --------------------------------------------------------------------------

#ifndef NUMREPORTS_H
#define NUMREPORTS_H

#include "Lightsprint/RRDebug.h"

namespace rr
{

// helper for checkConsistency(), similar to unsigned, but reports object number
class NumReports
{
public:
	NumReports(const char* _meshName)
	{
		numReports = 0;
		meshName = _meshName;
		indented = false;
	}
	~NumReports()
	{
		if (indented)
			RRReporter::indent(-2);
	}
	void operator ++(int i)
	{
		if (!numReports && meshName)
		{
			RRReporter::report(WARN,"Problem found in %s:\n",meshName);
			RRReporter::indent(2);
			indented = true;
		}
		numReports++;
	}
	operator unsigned() const
	{
		return numReports;
	}
private:
	unsigned numReports;
	const char* meshName;
	bool indented;
};

} // namespace

#endif
