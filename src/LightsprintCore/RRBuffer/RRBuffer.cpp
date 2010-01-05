// --------------------------------------------------------------------------
// Generic buffer code: load & save using FreeImage.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "../squish/squish.h"

// image cache
#include <boost/unordered_map.hpp>
#include <string>

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer::RRBuffer()
{
	customData = NULL;
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
	if (getFormat()==BF_DXT1 || getFormat()==BF_DXT3 || getFormat()==BF_DXT5)
	{
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),BF_RGBA,getScaled(),NULL);
		int flags = 0;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
		};
		// compressed copy -> decompressed this
		squish::DecompressImage(lock(BL_DISCARD_AND_WRITE),getWidth(),getHeight(),copy->lock(BL_READ),flags);
		unlock();
		copy->unlock();
		delete copy;
		setFormat(newFormat);
	}
	else
	if (newFormat==BF_DXT1 || newFormat==BF_DXT3 || newFormat==BF_DXT5)
	{
		setFormat(BF_RGBA);
		RRBuffer* copy = createCopy();
		reset(getType(),getWidth(),getHeight(),getDepth(),newFormat,getScaled(),NULL);
		int flags = 0;
		switch (getFormat())
		{
			case BF_DXT1: flags = squish::kDxt1; break;
			case BF_DXT3: flags = squish::kDxt3; break;
			case BF_DXT5: flags = squish::kDxt5; break;
		};
		// uncompressed copy -> compressed this
		squish::CompressImage(copy->lock(BL_READ),getWidth(),getHeight(),lock(BL_DISCARD_AND_WRITE),flags);
		unlock();
		copy->unlock();
		delete copy;
	}
	else
	{
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
}

void RRBuffer::setFormatFloats()
{
	switch (getFormat())
	{
		case BF_RGB:
			setFormat(BF_RGBF);
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
		case BF_RGBA:
			setFormat(BF_RGBAF);
			break;
	}
}

void RRBuffer::invert()
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
			{
				unsigned numElements = getWidth()*getHeight()*getDepth();
				for (unsigned i=0;i<numElements;i++)
				{
					RRVec4 color = getElement(i);
					color = RRVec4(1)-color;
					setElement(i,color);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"invert() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::multiplyAdd(RRVec4 multiplier, RRVec4 addend)
{
	if (multiplier==RRVec4(1) && addend==RRVec4(0))
	{
		return;
	}
	switch (getFormat())
	{
		case BF_RGB:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
			{
				unsigned numElements = getWidth()*getHeight()*getDepth();
				for (unsigned i=0;i<numElements;i++)
				{
					RRVec4 color = getElement(i);
					color = color*multiplier+addend;
					setElement(i,color);
				}
			}
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"multiplyAdd() not supported for compressed formats.\n"));
			break;
	}
}

void RRBuffer::flip(bool flipX, bool flipY, bool flipZ)
{
	switch (getFormat())
	{
		case BF_RGB:
		case BF_RGBA:
		case BF_RGBF:
		case BF_RGBAF:
		case BF_DEPTH:
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
			break;
		case BF_DXT1:
		case BF_DXT3:
		case BF_DXT5:
			RR_LIMITED_TIMES(1,RRReporter::report(WARN,"flip() not supported for compressed formats.\n"));
			break;
	}
}


//////////////////////////////////////////////////////////////////////////////
//
// ImageCache


class ImageCache
{
public:
	RRBuffer* load_cached(const char* filename, const char* cubeSideName[6])
	{
		Cache::iterator i = cache.find(filename);
		if (i!=cache.end())
		{
			// image was found in cache
			if (i->second.buffer // we try again if previous load failed, perhaps file was created on background
				&& i->second.buffer->version==i->second.bufferVersionWhenLoaded
				// && i->second.fileTimeWhenLoaded==boost::filesystem::last_write_time(filename)
				)
			{
				// image is already in memory and it was not modified since load, use it
				return i->second.buffer->createReference(); // add one ref for user
			}
			// modified after load, delete it from cache, we can't use it anymore
			delete i->second.buffer;
			cache.erase(i);
		}
		// load new file into cache
		Value& value = cache[filename];
		value.buffer = load_noncached(filename,cubeSideName);
		if (value.buffer)
		{
			value.buffer->createReference(); // keep initial ref for us, add one ref for user
			value.bufferVersionWhenLoaded = value.buffer->version;
			//value.fileTimeWhenLoaded = boost::filesystem::last_write_time(filename);
		}
		return value.buffer;
	}
	size_t getMemoryOccupied()
	{
		size_t memoryOccupied = 0;
		for (Cache::iterator i=cache.begin();i!=cache.end();i++)
		{
			if (i->second.buffer)
				memoryOccupied += i->second.buffer->getBufferBytes();
		}
		return memoryOccupied;
	}
	~ImageCache()
	{
		for (Cache::iterator i=cache.begin();i!=cache.end();i++)
		{
			// if users deleted their buffers, refcount should be down at 1 and this delete is final
			if (i->second.buffer && i->second.buffer->getReferenceCount()!=1)
				RRReporter::report(WARN,"Memory leak, image %s not deleted (%dx).\n",i->second.buffer->filename.c_str(),i->second.buffer->getReferenceCount()-1);

			delete i->second.buffer;
		}
	}
protected:
	RRBuffer* load_noncached(const char *filename, const char* cubeSideName[6])
	{
		RRBuffer* texture = RRBuffer::create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
		if (!texture->reload(filename,cubeSideName))
		{
			RR_SAFE_DELETE(texture);
		}
		return texture;
	}
	struct Value
	{
		RRBuffer* buffer;
		unsigned bufferVersionWhenLoaded;
		//std::time_t fileTimeWhenLoaded;
	};
	typedef boost::unordered_map<std::string,Value> Cache;
	Cache cache;
};

// Single cache works better than individual cache instances in scenes,
// especially if user loads many models that share textures.
ImageCache s_imageCache;


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

bool RRBuffer::save(const char *_filename, const char* _cubeSideName[6], const SaveParameters* _parameters)
{
	if (s_save && this && _filename && s_save(this,_filename,_cubeSideName,_parameters))
	{
		filename = _filename;
		return true;
	}
	return false;
}

bool RRBuffer::reload(const char *_filename, const char* _cubeSideName[6])
{
	if (s_reload && this && _filename && s_reload(this,_filename,_cubeSideName))
	{
		filename = _filename;
		return true;
	}
	return false;
}

RRBuffer* RRBuffer::load(const char *filename, const char* cubeSideName[6])
{
	// cached version
	return s_imageCache.load_cached(filename,cubeSideName);

	// non-cached version
	//RRBuffer* texture = create(BT_VERTEX_BUFFER,1,1,1,BF_RGBA,true,NULL);
	//if (!texture->reload(filename,cubeSideName))
	//{
	//	RR_SAFE_DELETE(texture);
	//}
	//return texture;
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
