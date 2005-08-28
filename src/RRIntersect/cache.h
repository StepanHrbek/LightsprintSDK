#ifndef RRINTERSECT_CACHE_H
#define RRINTERSECT_CACHE_H

#include "RRIntersect.h"

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace rrIntersect
{                                                                            
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMeshImporter* importer);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMeshImporter* importer, const char* extension);
}

#endif
