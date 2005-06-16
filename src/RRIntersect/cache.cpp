#include "cache.h"
#include "sha1.h"
#include <stdio.h>
#include <stdlib.h>

namespace rrIntersect
{

#define min(a,b) (((a)<(b))?(a):(b))

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

const char* getFileName(unsigned char* data, unsigned bits)
{
	static char name[100];
	static char letter[33]="0123456789abcdefghijklmnopqrstuv";
	for(unsigned i=0;i<min((bits+4)/5,100);i++)
		name[i]=letter[getBits(data, i*5, min(5,bits-i*5))];
	name[99]=0;
	return name;
}

const char* getFileName(RRObjectImporter* importer)
{
	sha1_context ctx;
	sha1_starts(&ctx);
	unsigned i = importer->getNumVertices();
	while(i--)
	{
		sha1_update(&ctx, (unsigned char*)importer->getVertex(i), sizeof(RRReal)*3);
	}
	i = importer->getNumTriangles();
	while(i--)
	{
		struct S {unsigned a,b,c;} s;
		importer->getTriangle(i,s.a,s.b,s.c);
		sha1_update(&ctx, (unsigned char*)&s, sizeof(s));
	}
	unsigned char digest[20];
	sha1_finish(&ctx, digest);
	return getFileName(digest,8*sizeof(digest));
}

#ifdef _MSC_VER
#include <windows.h>
#endif

const char* getFileName(RRObjectImporter* importer, char* extension)
{
	static char buf[300];
#ifdef _MSC_VER
	// microsofts way to support variables changed by the same program
	GetEnvironmentVariable("RRCACHE",buf,299);
	buf[299]=0;
	int len = strlen(buf);
	_snprintf(buf+len,299-len,"s%s",getFileName(importer),extension);
#else
	char* dir=getenv("RRCACHE");
	_snprintf(buf,299,"%s%s%s",dir?dir:"",getFileName(importer),extension);
#endif
	buf[299]=0;
	return buf;
}

}
