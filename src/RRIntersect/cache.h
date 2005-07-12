#ifndef RRINTERSECT_CACHE_H
#define RRINTERSECT_CACHE_H

#include "RRIntersect.h"

namespace rrIntersect
{                                                                            
	void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	void getFileName(char* buf, unsigned bufsize, RRObjectImporter* importer);
	void getFileName(char* buf, unsigned bufsize, RRObjectImporter* importer, const char* extension);
}

#endif
