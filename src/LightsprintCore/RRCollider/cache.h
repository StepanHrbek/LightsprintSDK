// --------------------------------------------------------------------------
// Cache of acceleration structures for ray-mesh intersections.
// Copyright (c) 2000-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_CACHE_H
#define COLLIDER_CACHE_H

#include "Lightsprint/RRCollider.h"

#ifndef PRIVATE
	#define PRIVATE
#endif

namespace rr
{                                                                            
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits);
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, const RRMesh* mesh);
	PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, const RRMesh* mesh, const char* cacheLocation, const char* extension);
}

#endif
