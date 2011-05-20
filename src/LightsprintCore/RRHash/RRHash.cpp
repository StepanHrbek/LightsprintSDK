// --------------------------------------------------------------------------
// Cache of acceleration structures for ray-mesh intersections.
// Copyright (c) 2000-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRObject.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
	#include <windows.h> // GetTempPath
#else
	#include <sys/stat.h> // mkdir
#endif

#define SWAP_32(x) \
	((x) << 24) | \
	(((x) & 0x0000FF00) << 8) | \
	(((x) & 0x00FF0000) >> 8) | \
	(((x) >> 24) & 0xFF)

namespace rr
{

static unsigned getBit(unsigned char* data, unsigned bit)
{
	return (data[bit/8]>>(bit%8))&1;
}

static unsigned getBits(unsigned char* data, unsigned bit, unsigned bits)
{
	unsigned tmp = 0;
	while (bits--) tmp = tmp*2 + getBit(data, bit+bits);
	return tmp;
}

static void getFileName(char* buf, unsigned bufsize, unsigned char* hash, unsigned bits)
{
	const char* letter="0123456789abcdefghijklmnopqrstuv";
	for (unsigned i=0;i<RR_MIN((bits+4)/5,bufsize-1);i++)
		buf[i]=letter[getBits(hash, i*5, RR_MIN(5,bits-i*5))];
	buf[RR_MIN((bits+4)/5,bufsize-1)]=0;
}

RRHash RRMesh::getHash() const
{
	CSHA1 sha1;
	unsigned numVertices = getNumVertices();
	for (unsigned i=0;i<numVertices;i++)
	{
		RRMesh::Vertex v;
		getVertex(i,v);
#ifdef RR_BIG_ENDIAN
		for (unsigned j=0;j<3;j++)
			((unsigned long*)&v)[j] = SWAP_32(((unsigned long*)&v)[j]);
#endif
		sha1.Update((const unsigned char*)&v, sizeof(v));
	}
	unsigned numTriangles = getNumTriangles();
	for (unsigned i=0;i<numTriangles;i++)
	{
		RRMesh::Triangle t;
		getTriangle(i,t);
#ifdef RR_BIG_ENDIAN
		for (unsigned j=0;j<3;j++)
			((unsigned long*)&t)[j] = SWAP_32(((unsigned long*)&t)[j]);
#endif
		sha1.Update((const unsigned char*)&t, sizeof(t));
	}
	RRHash hash;
	sha1.Final();
	sha1.GetHash(hash.value);
	return hash;
}

RRHash RRObject::getHash() const
{
	CSHA1 sha1;
	const RRMesh* mesh = getCollider()->getMesh();
	unsigned numVertices = mesh->getNumVertices();
	for (unsigned i=0;i<numVertices;i++)
	{
		RRMesh::Vertex v;
		mesh->getVertex(i,v);
#ifdef RR_BIG_ENDIAN
		for (unsigned j=0;j<3;j++)
			((unsigned long*)&v)[j] = SWAP_32(((unsigned long*)&v)[j]);
#endif
		sha1.Update((const unsigned char*)&v, sizeof(v));
	}
	unsigned numTriangles = mesh->getNumTriangles();
	for (unsigned i=0;i<numTriangles;i++)
	{
		struct TriangleData
		{
			RRMesh::Triangle triangle;
			RRMesh::TriangleNormals triangleNormals;
			RRMesh::TriangleMapping triangleMapping[4];
			RRSideBits sideBits[2];
			RRVec3 materialData[4];
			RRReal refractionIndex;

			TriangleData(const RRObject* object, const RRMesh* mesh, unsigned t)
			{
				// without this, triangle mappings that don't exist would be uninitialized
				memset(this,0,sizeof(*this));

				mesh->getTriangle(t,triangle);
#ifdef RR_BIG_ENDIAN
				for (unsigned j=0;j<3;j++)
					((unsigned long*)&triangle)[j] = SWAP_32(((unsigned long*)&triangle)[j]);
#endif

				const RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
				if (material)
				{
					// sideBits is bitfield, it contains at least one unused uninitialized bit
					// we don't want any random data here, so we memset target and copy only used bits
					sideBits[0].renderFrom   = material->sideBits[0].renderFrom;
					sideBits[0].emitTo       = material->sideBits[0].emitTo;
					sideBits[0].catchFrom    = material->sideBits[0].catchFrom;
					sideBits[0].legal        = material->sideBits[0].legal;
					sideBits[0].receiveFrom  = material->sideBits[0].receiveFrom;
					sideBits[0].reflect      = material->sideBits[0].reflect;
					sideBits[0].transmitFrom = material->sideBits[0].transmitFrom;
					sideBits[1].renderFrom   = material->sideBits[1].renderFrom;
					sideBits[1].emitTo       = material->sideBits[1].emitTo;
					sideBits[1].catchFrom    = material->sideBits[1].catchFrom;
					sideBits[1].legal        = material->sideBits[1].legal;
					sideBits[1].receiveFrom  = material->sideBits[1].receiveFrom;
					sideBits[1].reflect      = material->sideBits[1].reflect;
					sideBits[1].transmitFrom = material->sideBits[1].transmitFrom;
					materialData[0] = material->diffuseReflectance.color;
					materialData[1] = material->diffuseEmittance.color;
					materialData[2] = material->specularReflectance.color;
					materialData[3] = material->specularTransmittance.color;
					refractionIndex = material->refractionIndex;
				}

				mesh->getTriangleNormals(t,triangleNormals);
				mesh->getTriangleMapping(t,triangleMapping[0],material->diffuseReflectance.texcoord);
				mesh->getTriangleMapping(t,triangleMapping[1],material->specularReflectance.texcoord);
				mesh->getTriangleMapping(t,triangleMapping[2],material->diffuseEmittance.texcoord);
				mesh->getTriangleMapping(t,triangleMapping[3],material->specularTransmittance.texcoord);
			}
		};
		TriangleData td(this,mesh,i);
		sha1.Update((const unsigned char*)&td, sizeof(td));
	}
	RRHash hash;
	sha1.Final();
	sha1.GetHash(hash.value);
	return hash;
}

//////////////////////////////////////////////////////////////////////////////
//
//  RRHash

RRHash::RRHash(const unsigned char* data, unsigned size)
{
	CSHA1 sha1;
	if (data)
		sha1.Update(data, size);
	sha1.Final();
	sha1.GetHash(value);
}

void RRHash::getFileName(char* buf, unsigned bufsize, unsigned version, const char* cacheLocation, const char* extension) const
{
	if (!bufsize) return;
	buf[0]=0;
	// rrcache
#ifdef _WIN32
	char tmpPath[_MAX_PATH+1];
	if (!cacheLocation)
	{
		GetTempPath(_MAX_PATH, tmpPath);
		#define IS_PATHSEP(x) (((x) == '\\') || ((x) == '/'))
		if (!IS_PATHSEP(tmpPath[strlen(tmpPath)-1])) strcat(tmpPath, "\\");
		cacheLocation = tmpPath;
	}
#else
	char tmpDir[] = "/tmp/lightsprint/";
        mkdir(tmpDir, 0744);
	cacheLocation = tmpDir;
#endif
#ifdef XBOX
	if (!cacheLocation) cacheLocation = "game:\\"; // xbox 360
#endif
	if (cacheLocation) 
	{
		strncpy(buf,cacheLocation,bufsize-1);
		buf[bufsize-1]=0;
		unsigned len = (unsigned)strlen(buf); 
		buf += len; 
		bufsize -= len;
	}
	// hash
	{
		RRHash hash2 = *this;
		*(unsigned*)hash2.value += version;
		rr::getFileName(buf,bufsize-1,hash2.value,8*sizeof(hash2.value));
		unsigned len = (unsigned)strlen(buf); 
		buf += len; 
		bufsize -= len;
	}
	// extension
	if (extension)
	{
		strncpy(buf,extension,bufsize-1);
		buf[bufsize-1]=0;
	}
}

bool RRHash::operator !=(const RRHash& a) const
{
	return memcmp(value,a.value,sizeof(value))!=0;
}

}
