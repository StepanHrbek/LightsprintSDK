// --------------------------------------------------------------------------
// Generic buffer code: load & save using FreeImage.
// Copyright (c) 2006-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer::RRBuffer()
{
	customData = NULL;
}

unsigned RRBuffer::getElementBits() const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElementBits() called.\n"));
	return 0;
}

void RRBuffer::setElement(unsigned index, const RRVec4& element)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::setElement() called.\n"));
}

RRVec4 RRBuffer::getElement(unsigned index) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElement(const RRVec3& coord) const
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

unsigned char* RRBuffer::lock(RRBufferLock lock)
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::lock() called.\n"));
	return NULL;
}

void RRBuffer::unlock()
{
	RR_LIMITED_TIMES(1,RRReporter::report(WARN,"Default empty RRBuffer::unlock() called.\n"));
}

unsigned RRBuffer::getMemoryOccupied() const
{
	return getWidth()*getHeight()*getDepth()*((getElementBits()+7)/8);
}

RRBuffer* RRBuffer::createCopy()
{
	unsigned char* data = lock(BL_READ);
	RRBuffer* copy = RRBuffer::create(getType(),getWidth(),getHeight(),getDepth(),getFormat(),getScaled(),data);
	unlock();
	return copy;
}

void RRBuffer::setFormat(RRBufferFormat newFormat)
{
	if (newFormat==getFormat())
	{
		return;
	}
	RRBuffer* copy = createCopy();
	reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),NULL);
	unsigned numElements = getWidth()*getHeight()*getDepth();
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = copy->getElement(i);
		setElement(i,color);
	}
	delete copy;
}

void RRBuffer::setFormatFloats()
{
	switch (getFormat())
	{
		case BF_RGB:
			setFormat(BF_RGBF);
			break;
		case BF_RGBA:
			setFormat(BF_RGBAF);
			break;
	}
}

void RRBuffer::invert()
{
	unsigned numElements = getWidth()*getHeight()*getDepth();
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i);
		color = RRVec4(1)-color;
		setElement(i,color);
	}
}

void RRBuffer::multiplyAdd(RRVec4 multiplier, RRVec4 addend)
{
	if (multiplier==RRVec4(1) && addend==RRVec4(0))
	{
		return;
	}
	unsigned numElements = getWidth()*getHeight()*getDepth();
	for (unsigned i=0;i<numElements;i++)
	{
		RRVec4 color = getElement(i);
		color = color*multiplier+addend;
		setElement(i,color);
	}
}

void RRBuffer::flip(bool flipX, bool flipY, bool flipZ)
{
	// slow getElement path, faster path can be written using lock and direct access
	unsigned xmax = getWidth();
	unsigned ymax = getHeight();
	unsigned zmax = getDepth();
	for (unsigned x=0;x<xmax;x++)
	for (unsigned y=0;y<ymax;y++)
	for (unsigned z=0;z<zmax;z++)
	{
		unsigned e1 = x+xmax*(y+ymax*z);
		unsigned e2 = (flipX?xmax-1-x:x)+xmax*((flipY?ymax-1-y:y)+ymax*(flipZ?zmax-1-z:z));
		if (e1<e2)
		{
			RRVec4 color1 = getElement(e1);
			RRVec4 color2 = getElement(e2);
			setElement(e1,color2);
			setElement(e2,color1);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// save & load

static RRBuffer::Loader* s_reload = NULL;
static RRBuffer::Saver* s_save = NULL;

RRBuffer::Loader* RRBuffer::setLoader(Loader* loader)
{
	Loader* result = s_reload;
	s_reload = loader;
	return result;
}

RRBuffer::Saver* RRBuffer::setSaver(Saver* saver)
{
	Saver* result = s_save;
	s_save = saver;
	return result;
}

bool RRBuffer::save(const char *filename, const char* cubeSideName[6], const SaveParameters* parameters)
{
	return (s_save && this && filename) ? s_save(this,filename,cubeSideName,parameters) : false;
}

bool RRBuffer::reload(const char *filename, const char* cubeSideName[6])
{
	return (s_reload && this && filename) ? s_reload(this,filename,cubeSideName) : false;
}

RRBuffer* RRBuffer::load(const char *filename, const char* cubeSideName[6])
{
	RRBuffer* texture = create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
	if (!texture->reload(filename,cubeSideName))
	{
		RR_SAFE_DELETE(texture);
	}
	return texture;
}

RRBuffer* RRBuffer::loadCube(const char *filename)
{
	RRBuffer* texture = create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
	if (!texture->reloadCube(filename))
	{
		RR_SAFE_DELETE(texture);
	}
	return texture;
}

bool RRBuffer::reloadCube(const char *_filename)
{
	if (!_filename)
		return false;
	const unsigned numConventions = 3;
	const char* cubeSideNames[numConventions][6] =
	{
		{"bk","ft","up","dn","rt","lf"}, // Quake
		{"BK","FT","UP","DN","RT","LF"}, // Quake
		{"negative_x","positive_x","positive_y","negative_y","positive_z","negative_z"} // codemonsters.de
	};
	const char** selectedConvention = cubeSideNames[0];
	// inserts %s if it is one of 6 images
	char* filename = _strdup(_filename);
	size_t filenameLen = strlen(filename);
	for (unsigned c=0;c<numConventions;c++)
	{
		for (unsigned s=0;s<6;s++)
		{
			const char* suffix = cubeSideNames[c][s];
			size_t suffixLen = strlen(suffix);
			if (filenameLen>=suffixLen+4 && strncmp(filename+filenameLen-suffixLen-4,suffix,suffixLen)==0 && filename[filenameLen-4]=='.')
			{
				filename[filenameLen-suffixLen-4] = '%';
				filename[filenameLen-suffixLen-3] = 's';
				filename[filenameLen-suffixLen-2] = '.';
				filename[filenameLen-suffixLen-1] = filename[filenameLen-3];
				filename[filenameLen-suffixLen  ] = filename[filenameLen-2];
				filename[filenameLen-suffixLen+1] = filename[filenameLen-1];
				filename[filenameLen-suffixLen+2] = 0;
				selectedConvention = cubeSideNames[c];
				goto done;
			}
		}
	}
done:
	bool result = reload(filename,selectedConvention);
	free(filename);
	return result;
}

}; // namespace
