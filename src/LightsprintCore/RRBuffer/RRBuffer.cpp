// --------------------------------------------------------------------------
// Generic buffer code: load & save using FreeImage.
// Copyright 2006-2008 Lightsprint, Stepan Hrbek. All rights reserved.
// --------------------------------------------------------------------------

#include <cassert>
#include <cstdio>
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
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElementBits() called.\n"));
	return 0;
}

void RRBuffer::setElement(unsigned index, const RRVec4& element)
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::setElement() called.\n"));
}

RRVec4 RRBuffer::getElement(unsigned index) const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

RRVec4 RRBuffer::getElement(const RRVec3& coord) const
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::getElement() called.\n"));
	return RRVec4(0);
}

unsigned char* RRBuffer::lock(RRBufferLock lock)
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::lock() called.\n"));
	return NULL;
}

void RRBuffer::unlock()
{
	LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Default empty RRBuffer::unlock() called.\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// save & load

static bool (*s_reload)(RRBuffer* buffer, const char *filename, const char* cubeSideName[6], bool flipV, bool flipH) = NULL;
static bool (*s_save)(RRBuffer* buffer, const char* filenameMask, const char* cubeSideName[6]) = NULL;

void RRBuffer::setLoader(bool (*_reload)(RRBuffer* buffer, const char *filename, const char* cubeSideName[6], bool flipV, bool flipH), bool (*_save)(RRBuffer* buffer, const char* filenameMask, const char* cubeSideName[6]))
{
	s_reload = _reload;
	s_save = _save;
}

bool RRBuffer::save(const char *filename, const char* cubeSideName[6])
{
	return (s_save && this && filename) ? s_save(this,filename,cubeSideName) : false;
}

bool RRBuffer::reload(const char *filename, const char* cubeSideName[6], bool flipV, bool flipH)
{
	return (s_reload && this && filename) ? s_reload(this,filename,cubeSideName,flipV,flipH) : false;
}

}; // namespace
