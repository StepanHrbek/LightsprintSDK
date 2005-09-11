#ifndef COLLIDER_CACHE_H
#define COLLIDER_CACHE_H

#include "RRCollider.h"

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace rrCollider
{                                                                            
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMeshImporter* importer);
	PRIVATE void getFileName(char* buf, unsigned bufsize, RRMeshImporter* importer, const char* extension);
}

#endif
