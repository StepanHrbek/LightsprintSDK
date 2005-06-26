#include "cache.h"
#include "sha1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace rrIntersect
{

#define TREE_VERSION 0

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

void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits)
{
	char* letter="0123456789abcdefghijklmnopqrstuv";
	for(unsigned i=0;i<min((bits+4)/5,bufsize-1);i++)
		buf[i]=letter[getBits(hash, i*5, min(5,bits-i*5))];
	buf[min((bits+4)/5,bufsize-1)]=0;
}

void getFileName(char* buf, unsigned bufsize, RRObjectImporter* importer)
{
	sha1_context ctx;
	sha1_starts(&ctx);
	if(TREE_VERSION)
	{
		uint8 ver = TREE_VERSION;
		sha1_update(&ctx, &ver, 1);
	}
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
	return getFileName(buf,bufsize,digest,8*sizeof(digest));
}

void getFileName(char* buf, unsigned bufsize, RRObjectImporter* importer, char* extension)
{
	if(!bufsize) return;
	buf[0]=0;
	// rrcache
	char* dir=getenv("RRCACHE");
	if(dir) 
	{
		strncpy(buf,dir,bufsize-1);
		buf[bufsize-1]=0;
		unsigned len = (unsigned)strlen(buf); 
		buf += len; 
		bufsize -= len;
	}
	// hash
	if(importer)
	{
		getFileName(buf,bufsize-1,importer);
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
