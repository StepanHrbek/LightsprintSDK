#ifndef COLLIDER_CACHE_H
#define COLLIDER_CACHE_H

#include "RRCollider.h"

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace rr
{                                                                            
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMesh* importer);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMesh* importer, const char* cacheLocation, const char* extension);
}

#endif
