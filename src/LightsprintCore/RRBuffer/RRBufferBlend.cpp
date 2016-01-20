//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Blend of two buffers.
// --------------------------------------------------------------------------

#include "RRBufferBlend.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer* RRBuffer::createEnvironmentBlend(RRBuffer* environment0, RRBuffer* environment1, RRReal angleRad0, RRReal angleRad1, RRReal blendFactor)
{
	// optimization, don't blend when not needed
	if (!environment1 && !angleRad0)
	{
		return environment0 ? environment0->createReference() : nullptr;
	}

	return new RRBufferBlend(environment0, environment1, angleRad0, angleRad1, blendFactor);
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBufferBlend

RRBufferBlend::RRBufferBlend(RRBuffer* _environment0, RRBuffer* _environment1, RRReal _angleRad0, RRReal _angleRad1, RRReal _blendFactor)
{
	// creating references adds small overhead, but it protects us against users who delete original buffers before they delete us
	// (on the other hand, createReference is not 100% thread safe, so now we are not 100% thread safe too.
	//  but hopefully no one creates us in worker threads, it's more efficient to create us once in job setup)
	environment0 = _environment0 ? _environment0->createReference() : nullptr;
	environment1 = _environment1 ? _environment1->createReference() : nullptr;
	rotation0 = RRMatrix3x4::rotationByYawPitchRoll(RRVec3(_angleRad0,0,0));
	rotation1 = RRMatrix3x4::rotationByYawPitchRoll(RRVec3(_angleRad1,0,0));
	blendFactor = _blendFactor;
}

RRVec4 RRBufferBlend::getElementAtDirection(const RRVec3& direction, const RRColorSpace* colorSpace) const
{
	RRVec3 direction0 = direction;
	RRVec3 direction1 = direction;
	RRVec4 color0(0);
	RRVec4 color1(0);
	if (environment0)
	{
		rotation0.transformDirection(direction0);
		color0 = environment0->getElementAtDirection(direction0,colorSpace);
	}
	if (environment1)
	{
		rotation1.transformDirection(direction1);
		color1 = environment1->getElementAtDirection(direction1,colorSpace);
	}
	return color0*(1-blendFactor) + color1*blendFactor;
}

RRBufferBlend::~RRBufferBlend()
{
	delete environment0;
	delete environment1;
}

}; // namespace
