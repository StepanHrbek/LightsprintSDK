//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Blend of two buffers.
// --------------------------------------------------------------------------

#ifndef BUFFERBLEND_H
#define BUFFERBLEND_H

#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRLight.h" // colorSpace

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRBufferBlend

class RR_API RRBufferBlend : public RRBuffer
{
public:
	RRBufferBlend(RRBuffer* environment0, RRBuffer* environment1, RRReal angleRad0, RRReal angleRad1, RRReal blendFactor);
	virtual ~RRBufferBlend();

	void report(const char* name) const
	{
		RRReporter::report(ERRO,"Buffers created with createEnvironmentBlend() have only getElementAtDirection() implemented, %s() was called.\n",name);
	}

	// setting
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) {report("reset");return false;}
	virtual void setElement(unsigned index, const RRVec4& element, const RRColorSpace* colorSpace) {report("setElement");}

	// reading
	virtual RRBufferType getType()                                   const {return BT_CUBE_TEXTURE;} // necessary for createEquirectangular() to work
	virtual unsigned getWidth()                                      const {return environment0?environment0->getWidth():1;} // necessary for createEquirectangular() to work
	virtual unsigned getHeight()                                     const {return environment0?environment0->getHeight():1;} // necessary for createEquirectangular() to work
	virtual unsigned getDepth()                                      const {return 6;}
	virtual RRBufferFormat getFormat()                               const {return environment0?environment0->getFormat():BF_RGBA;}
	virtual bool getScaled()                                         const {return environment0?environment0->getScaled():false;}
	virtual unsigned getBufferBytes()                                const {return sizeof(*this);}
	virtual RRVec4 getElement(unsigned index, const RRColorSpace* colorSpace)const {report("getElement");return RRVec4(0);}
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRColorSpace* colorSpace, bool interpolated) const {report("getElementAtPosition");return RRVec4(0);}
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRColorSpace* colorSpace) const;
	virtual unsigned char* lock(RRBufferLock lock)                         {report("lock");return nullptr;}
	virtual void unlock()                                                  {report("unlock");}
	virtual bool isStub()                                                  {return false;}

	// reference counting
	virtual RRBuffer* createReference()                                    {report("createReference");return nullptr;}
	virtual unsigned getReferenceCount()                                   {report("getReferenceCount");return 1;}

protected:
	RRBuffer* environment0;
	RRBuffer* environment1;
	RRMatrix3x4 rotation0;
	RRMatrix3x4 rotation1;
	RRReal blendFactor;
};

}; // namespace

#endif
