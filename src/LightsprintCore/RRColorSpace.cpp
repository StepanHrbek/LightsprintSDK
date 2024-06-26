// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// sRGB color space.
// --------------------------------------------------------------------------

#include <cfloat>
#include <cmath>
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRGammaScaler
//
// Transforms colors with pow() function.

class RRGammaScaler : public RRColorSpace
{
public:
	RRGammaScaler(RRReal agamma)
	{
		gamma = agamma;
		invGamma = 1/gamma;
		for (unsigned i=0;i<256;i++)
			byteToLinear[i] = pow(float(i)/255,invGamma);
	}
	virtual void fromLinear(RRReal& a) const
	{
		RR_ASSERT(std::isfinite(a));
		a = (a>=0)?pow(a,gamma):-pow(-a,gamma);
		RR_ASSERT(std::isfinite(a));
	}
	virtual void toLinear(RRReal& a) const
	{
		RR_ASSERT(std::isfinite(a));
		a = (a>=0)?pow(a,invGamma):-pow(-a,invGamma);
		RR_ASSERT(std::isfinite(a));
	}
	virtual void fromLinear(RRVec3& color) const
	{
		RR_ASSERT(std::isfinite(color[0]));
		RR_ASSERT(std::isfinite(color[1]));
		RR_ASSERT(std::isfinite(color[2]));
		color = RRVec3(
			(color[0]>=0)?pow(color[0],gamma):-pow(-color[0],gamma),
			(color[1]>=0)?pow(color[1],gamma):-pow(-color[1],gamma),
			(color[2]>=0)?pow(color[2],gamma):-pow(-color[2],gamma)
			);
		RR_ASSERT(std::isfinite(color[0]));
		RR_ASSERT(std::isfinite(color[1]));
		RR_ASSERT(std::isfinite(color[2]));
	}
	virtual void toLinear(RRVec3& color) const
	{
		RR_ASSERT(std::isfinite(color[0]));
		RR_ASSERT(std::isfinite(color[1]));
		RR_ASSERT(std::isfinite(color[2]));
		color = RRVec3(
			(color[0]>=0)?pow(color[0],invGamma):-pow(-color[0],invGamma),
			(color[1]>=0)?pow(color[1],invGamma):-pow(-color[1],invGamma),
			(color[2]>=0)?pow(color[2],invGamma):-pow(-color[2],invGamma)
			);
		RR_ASSERT(std::isfinite(color[0]));
		RR_ASSERT(std::isfinite(color[1]));
		RR_ASSERT(std::isfinite(color[2]));
	}
	virtual RRVec3 getLinear(const unsigned char rgb[3]) const
	{
		return RRVec3(byteToLinear[rgb[0]],byteToLinear[rgb[1]],byteToLinear[rgb[2]]);
	}

protected:
	RRReal gamma;
	RRReal invGamma;
	RRReal byteToLinear[256];
};

//////////////////////////////////////////////////////////////////////////////
//
// RRColorSpace

RRColorSpace* RRColorSpace::create_sRGB(RRReal power)
{
	return new RRGammaScaler(power);
}


} // namespace
