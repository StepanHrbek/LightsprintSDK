// --------------------------------------------------------------------------
// Material.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <math.h>

#include "Lightsprint/RRObject.h"
#include "memory.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRMaterial


void RRMaterial::reset(bool twoSided)
{
	RRSideBits sideBitsTmp[2][2]={
		{{1,1,1,1,1,1,1,0},{0,0,1,0,0,0,0,0}}, // definition of default 1-sided (front side, back side)
		{{1,1,1,1,1,1,1,0},{1,0,1,1,1,1,1,0}}, // definition of default 2-sided (front side, back side)
	};
	sideBits[0]                  = sideBitsTmp[twoSided?1:0][0];
	sideBits[1]                  = sideBitsTmp[twoSided?1:0][1];
	diffuseReflectance.color     = RRVec3(0.5f);
	specularTransmittanceInAlpha = false;
	specularTransmittanceKeyed   = false;
	refractionIndex              = 1;
	lightmapTexcoord             = 0;
	name                         = NULL;
}

bool clamp1(RRReal& a, RRReal min, RRReal max)
{
	if (a<min) a=min; else
		if (a>max) a=max; else
			return false;
	return true;
}

bool clamp3(RRVec3& vec, RRReal min, RRReal max)
{
	unsigned clamped = 3;
	for (unsigned i=0;i<3;i++)
	{
		if (vec[i]<min) vec[i]=min; else
			if (vec[i]>max) vec[i]=max; else
				clamped--;
	}
	return clamped>0;
}

bool RRMaterial::validate(RRReal redistributedPhotonsLimit)
{
	bool changed = false;

	if (clamp3(diffuseReflectance.color,0,10)) changed = true;
	if (clamp3(specularReflectance.color,0,10)) changed = true;
	if (clamp3(specularTransmittance.color,0,10)) changed = true;
	if (clamp3(diffuseEmittance.color,0,1e6f)) changed = true;

	RRVec3 sum = diffuseReflectance.color+specularTransmittance.color+specularReflectance.color;
	RRReal max = MAX(sum[0],MAX(sum[1],sum[2]));
	if (max>redistributedPhotonsLimit)
	{
		diffuseReflectance.color *= redistributedPhotonsLimit/max;
		specularReflectance.color *= redistributedPhotonsLimit/max;
		specularTransmittance.color *= redistributedPhotonsLimit/max;
		changed = true;
	}

	// it is common error to default refraction to 0, we must anticipate and handle it
	if (refractionIndex==0) {refractionIndex = 1; changed = true;}

	return changed;
}

void RRMaterial::convertToCustomScale(const RRScaler* scaler)
{
	if (scaler)
	{
		scaler->getCustomFactor(diffuseReflectance.color);
		scaler->getCustomScale(diffuseEmittance.color);
		scaler->getCustomFactor(specularReflectance.color);
		scaler->getCustomFactor(specularTransmittance.color);
		validate();
	}
}

void RRMaterial::convertToPhysicalScale(const RRScaler* scaler)
{
	if (scaler)
	{
		scaler->getPhysicalFactor(diffuseReflectance.color);
		scaler->getPhysicalScale(diffuseEmittance.color);
		scaler->getPhysicalFactor(specularReflectance.color);
		scaler->getPhysicalFactor(specularTransmittance.color);
		validate();
	}
}

} // namespace
