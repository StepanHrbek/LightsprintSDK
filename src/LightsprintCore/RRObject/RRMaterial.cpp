// --------------------------------------------------------------------------
// Material.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <math.h>

#include "Lightsprint/RRObject.h"
#include "memory.h"

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
	sideBits[0]           = sideBitsTmp[twoSided?1:0][0];
	sideBits[1]           = sideBitsTmp[twoSided?1:0][1];
	diffuseReflectance    = RRVec3(0.5f);
	diffuseEmittance      = RRVec3(0);
	specularReflectance   = 0;
	specularTransmittance = RRVec3(0);
	refractionIndex       = 1;
	name                  = NULL;
}

bool clamp(RRReal& vec, RRReal min, RRReal max)
{
	if(vec<min) {vec=min; return true;}
	if(vec>max) {vec=max; return true;}
	return false;
}

bool clamp(RRVec3& vec, RRReal min, RRReal max)
{
	unsigned clamped = 3;
	for(unsigned i=0;i<3;i++)
	{
		if(vec[i]<min) vec[i]=min; else
			if(vec[i]>max) vec[i]=max; else
				clamped--;
	}
	return clamped>0;
}

bool RRMaterial::validate()
{
	bool changed = false;

	const float MAX_LEFT = 0.98f; // photons that left surface, sum of dif/spec reflected, transmitted
	const float MAX_REFL = MAX_LEFT;
	const float MAX_TRANS = MAX_LEFT;
	const float MAX_EMIT = 1e6;//1000;

	if(clamp(diffuseEmittance,0,MAX_EMIT)) changed = true;
	if(clamp(specularTransmittance,0,MAX_TRANS)) changed = true;
	if(clamp(diffuseReflectance,0,MAX_REFL)) changed = true;
	if(clamp(specularReflectance,0,MAX_REFL)) changed = true;
	// clamp so that no new photons appear
	for(unsigned i=0;i<3;i++)
	{
		RRReal all = diffuseReflectance[i]+specularReflectance+specularTransmittance[i];
		if(clamp(all,0,MAX_LEFT)) changed = true;
		if(clamp(diffuseReflectance[i],0,all-specularTransmittance[i])) changed = true;
		if(clamp(specularReflectance,0,all-specularTransmittance[i]-diffuseReflectance[i])) changed = true;
	}

	// FCollada returns 0 when information is not available
	if(refractionIndex==0) {refractionIndex = 1; changed = true;}

	return changed;
}

void RRMaterial::convertToCustomScale(const RRScaler* scaler)
{
	if(scaler)
	{
		scaler->getCustomFactor(diffuseReflectance);
		scaler->getCustomScale(diffuseEmittance);
		scaler->getCustomFactor(specularReflectance);
		scaler->getCustomFactor(specularTransmittance);
		validate();
	}
}

void RRMaterial::convertToPhysicalScale(const RRScaler* scaler)
{
	if(scaler)
	{
		scaler->getPhysicalFactor(diffuseReflectance);
		scaler->getPhysicalScale(diffuseEmittance);
		scaler->getPhysicalFactor(specularReflectance);
		scaler->getPhysicalFactor(specularTransmittance);
		validate();
	}
}

} // namespace
