#ifndef _CACHE_H
#define _CACHE_H

#include "RRIntersect.h"

const char* getFileName(unsigned char* data, unsigned bits);
const char* getFileName(RRObjectImporter* importer);
const char* getFileName(RRObjectImporter* importer, char* extension);

#endif
