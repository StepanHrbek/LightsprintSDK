#include "cache.h"
#include "sha1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace rr
{

#define TREE_VERSION 0

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
	char* letter="0123456789abcdefghijklmnopqrstuv";
	for(unsigned i=0;i<MIN((bits+4)/5,bufsize-1);i++)
		buf[i]=letter[getBits(hash, i*5, MIN(5,bits-i*5))];
	buf[MIN((bits+4)/5,bufsize-1)]=0;
}

PRIVATE void getFileName(char* buf, unsigned bufsize, RRMesh* importer)
{
	sha1::sha1_context ctx;
	sha1::sha1_starts(&ctx);
	if(TREE_VERSION)
	{
		unsigned char ver = TREE_VERSION;
		sha1::sha1_update(&ctx, &ver, 1);
	}
	unsigned i = importer->getNumVertices();
	while(i--)
	{
		RRMesh::Vertex v;
		importer->getVertex(i,v);
		sha1::sha1_update(&ctx, (unsigned char*)&v, sizeof(v));
	}
	i = importer->getNumTriangles();
	while(i--)
	{
		RRMesh::Triangle t;
		importer->getTriangle(i,t);
		sha1::sha1_update(&ctx, (unsigned char*)&t, sizeof(t));
	}
	unsigned char digest[20];
	sha1::sha1_finish(&ctx, digest);
	return getFileName(buf,bufsize,digest,8*sizeof(digest));
}

PRIVATE void getFileName(char* buf, unsigned bufsize, RRMesh* importer, const char* cacheLocation, const char* extension)
{
	if(!bufsize) return;
	buf[0]=0;
	// rrcache
	const char* dir = cacheLocation?cacheLocation:getenv("RRCACHE");
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
