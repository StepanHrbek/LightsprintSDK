#include "cache.h"
#include "sha1.h"
#include <stdio.h>

namespace rrIntersect
{

#define min(a,b) (((a)<(b))?(a):(b))

unsigned getBit(unsigned char* data, unsigned bit)
{
	return (data[bit/8]>>(bit%8))&1;
}

unsigned getBits(unsigned char* data, unsigned bit, unsigned bits)
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
		sha1_update(&ctx, (unsigned char*)importer->getVertex(i), sizeof(RRreal)*3);
	}
	i = importer->getNumTriangles();
	while(i--)
	{
		struct S {unsigned a,b,c,m;} s;
		importer->getTriangle(i,s.a,s.b,s.c,s.m);
		sha1_update(&ctx, (unsigned char*)&s, sizeof(s));
	}
	unsigned char digest[20];
	sha1_finish(&ctx, digest);
	return getFileName(digest,8*sizeof(digest));
}

const char* getFileName(RRObjectImporter* importer, char* extension)
{
	static char buf[100];
	_snprintf(buf,99,"%s%s",getFileName(importer),extension);
	buf[99]=0;
	return buf;
}

}
