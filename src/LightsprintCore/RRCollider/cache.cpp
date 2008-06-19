// --------------------------------------------------------------------------
// Cache of acceleration structures for ray-mesh intersections.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "cache.h"
#include "sha1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
	#include <windows.h> // GetTempPath
#else
	#include <sys/stat.h> // mkdir
#endif

namespace rr
{

#define MIN(a,b) (((a)<(b))?(a):(b))

static unsigned getBit(unsigned char* data, unsigned bit)
{
	return (data[bit/8]>>(bit%8))&1;
}

static unsigned getBits(unsigned char* data, unsigned bit, unsigned bits)
{
	unsigned tmp = 0;
	while(bits--) tmp = tmp*2 + getBit(data, bit+bits);
	return tmp;
}

PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits)
{
	const char* letter="0123456789abcdefghijklmnopqrstuv";
	for(unsigned i=0;i<MIN((bits+4)/5,bufsize-1);i++)
		buf[i]=letter[getBits(hash, i*5, MIN(5,bits-i*5))];
	buf[MIN((bits+4)/5,bufsize-1)]=0;
}

PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, const RRMesh* importer)
{
	sha1::sha1_context ctx;
	sha1::sha1_starts(&ctx);
	if(version)
	{
		unsigned char ver = version;
		sha1::sha1_update(&ctx, &ver, 1);
	}
	unsigned i = importer->getNumVertices();
	while(i--)
	{
		RRMesh::Vertex v;
		importer->getVertex(i,v);
#if defined(XBOX) || defined(__PPC__)
		for(unsigned j=0;j<3;j++)
			((unsigned long*)&v)[j] = _byteswap_ulong(((unsigned long*)&v)[j]);
#endif
		sha1::sha1_update(&ctx, (unsigned char*)&v, sizeof(v));
	}
	i = importer->getNumTriangles();
	while(i--)
	{
		RRMesh::Triangle t;
		importer->getTriangle(i,t);
#if defined(XBOX) || defined(__PPC__)
		for(unsigned j=0;j<3;j++)
			((unsigned long*)&t)[j] = _byteswap_ulong(((unsigned long*)&t)[j]);
#endif
		sha1::sha1_update(&ctx, (unsigned char*)&t, sizeof(t));
	}
	unsigned char digest[20];
	sha1::sha1_finish(&ctx, digest);
	return getFileName(buf,bufsize,digest,8*sizeof(digest));
}

PRIVATE void getFileName(char* buf, unsigned bufsize, unsigned version, const RRMesh* importer, const char* cacheLocation, const char* extension)
{
	if(!bufsize) return;
	buf[0]=0;
	// rrcache
#ifdef _WIN32
	char tmpPath[_MAX_PATH+1];
	if(!cacheLocation)
	{
		GetTempPath(_MAX_PATH, tmpPath);
		#define IS_PATHSEP(x) (((x) == '\\') || ((x) == '/'))
		if(!IS_PATHSEP(tmpPath[strlen(tmpPath)-1])) strcat(tmpPath, "\\");
		cacheLocation = tmpPath;
	}
#else
	char tmpDir[] = "/tmp/lightsprint/";
        mkdir(tmpDir, 0744);
	cacheLocation = tmpDir;
#endif
#ifdef XBOX
	if(!cacheLocation) cacheLocation = "game:\\"; // xbox 360
#endif
	if(cacheLocation) 
	{
		strncpy(buf,cacheLocation,bufsize-1);
		buf[bufsize-1]=0;
		unsigned len = (unsigned)strlen(buf); 
		buf += len; 
		bufsize -= len;
	}
	// hash
	if(importer)
	{
		getFileName(buf,bufsize-1,version,importer);
		unsigned len = (unsigned)strlen(buf); 
		buf += len; 
		bufsize -= len;
	}
	// extension
	if(extension)
	{
		strncpy(buf,extension,bufsize-1);
		buf[bufsize-1]=0;
	}
}

}
