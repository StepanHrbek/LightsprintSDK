#ifndef RRINTERSECT_CACHE_H
#define RRINTERSECT_CACHE_H

#include "RRIntersect.h"

namespace rrIntersect
{

	const char* getFileName(unsigned char* data, unsigned bits);
	const char* getFileName(RRObjectImporter* importer);
	const char* getFileName(RRObjectImporter* importer, char* extension);

}

#endif
