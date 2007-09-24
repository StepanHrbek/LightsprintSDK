#ifndef COLLIDER_CACHE_H
#define COLLIDER_CACHE_H

#include "Lightsprint/RRCollider.h"

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace rr
{                                                                            
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, RRMesh* mesh);
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, RRMesh* mesh, const char* cacheLocation, const char* extension);
}

#endif
