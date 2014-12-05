// --------------------------------------------------------------------------
// Blend of two buffers.
// Copyright (c) 2006-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRBufferBlend.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{


/////////////////////////////////////////////////////////////////////////////
//
// RRBuffer

RRBuffer* RRBuffer::createEnvironmentBlend(RRBuffer* environment0, RRBuffer* environment1, RRReal angleRad0, RRReal angleRad1, RRReal blendFactor, const RRScaler* scaler)
{
	// optimization, don't blend when not needed
	if (!environment1 && !angleRad0)
	{
		return environment0 ? environment0->createReference() : NULL;
	}

	return new RRBufferBlend(environment0, environment1, angleRad0, angleRad1, blendFactor, scaler);
}


/////////////////////////////////////////////////////////////////////////////
//
// RRBufferBlend

RRBufferBlend::RRBufferBlend(const RRBuffer* _environment0, const RRBuffer* _environment1, RRReal _angleRad0, RRReal _angleRad1, RRReal _blendFactor, const RRScaler* _scaler)
{
	environment0 = _environment0;
	environment1 = _environment1;
	rotation0 = RRMatrix3x4::rotationByYawPitchRoll(RRVec3(_angleRad0,0,0));
	rotation1 = RRMatrix3x4::rotationByYawPitchRoll(RRVec3(_angleRad1,0,0));
	blendFactor = _blendFactor;
	scaler0 = (_environment0 && _environment0->getScaled()) ? _scaler : NULL;
	scaler1 = (_environment1 && _environment1->getScaled()) ? _scaler : NULL;
}

RRVec4 RRBufferBlend::getElementAtDirection(const RRVec3& direction) const
{
	RRVec3 direction0 = direction;
	RRVec3 direction1 = direction;
	RRVec4 color0(0);
	RRVec4 color1(0);
	if (environment0)
	{
		rotation0.transformDirection(direction0);
		color0 = environment0->getElementAtDirection(direction0);
		if (scaler0)
			scaler0->getPhysicalScale(color0);
	}
	if (environment1)
	{
		rotation1.transformDirection(direction1);
		color1 = environment1->getElementAtDirection(direction1);
		if (scaler1)
			scaler1->getPhysicalScale(color1);
	}
	return color0*(1-blendFactor) + color1*blendFactor;
}

}; // namespace
