// --------------------------------------------------------------------------
// Blend of two buffers.
// Copyright (c) 2006-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef BUFFERBLEND_H
#define BUFFERBLEND_H

#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRLight.h" // scaler

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRBufferBlend

class RR_API RRBufferBlend : public RRBuffer
{
public:
	RRBufferBlend(const RRBuffer* environment0, const RRBuffer* environment1, RRReal angleRad0, RRReal angleRad1, RRReal blendFactor);

	void report(const char* name) const
	{
		RRReporter::report(ERRO,"Buffers created with createEnvironmentBlend() have only getElementAtDirection() implemented, %s() was called.\n",name);
	}

	// setting
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) {report("reset");return false;}
	virtual void setElement(unsigned index, const RRVec4& element, const RRScaler* scaler) {report("setElement");}

	// reading
	virtual RRBufferType getType()                                   const {report("getType");return BT_2D_TEXTURE;}
	virtual unsigned getWidth()                                      const {report("getWidth");return 1;}
	virtual unsigned getHeight()                                     const {report("getHeight");return 1;}
	virtual unsigned getDepth()                                      const {report("getDepth");return 1;}
	virtual RRBufferFormat getFormat()                               const {report("getFormat");return BF_RGBAF;}
	virtual bool getScaled()                                         const {return false;}
	virtual unsigned getBufferBytes()                                const {return sizeof(*this);}
	virtual RRVec4 getElement(unsigned index, const RRScaler* scaler)const {report("getElement");return RRVec4(0);}
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRScaler* scaler, bool interpolated) const {report("getElementAtPosition");return RRVec4(0);}
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRScaler* scaler) const;
	virtual unsigned char* lock(RRBufferLock lock)                         {report("lock");return NULL;}
	virtual void unlock()                                                  {report("unlock");}
	virtual bool isStub()                                                  {return false;}

	// reference counting
	virtual RRBuffer* createReference()                                    {report("createReference");return NULL;}
	virtual unsigned getReferenceCount()                                   {report("getReferenceCount");return 1;}

protected:
	const RRBuffer* environment0;
	const RRBuffer* environment1;
	RRMatrix3x4 rotation0;
	RRMatrix3x4 rotation1;
	RRReal blendFactor;
};

}; // namespace

#endif
