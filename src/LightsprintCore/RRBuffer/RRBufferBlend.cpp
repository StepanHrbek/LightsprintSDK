//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
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

RRBufferBlend::RRBufferBlend(const RRBuffer* _environment0, const RRBuffer* _environment1, RRReal _angleRad0, RRReal _angleRad1, RRReal _blendFactor)
{
	environment0 = _environment0;
	environment1 = _environment1;
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

}; // namespace
