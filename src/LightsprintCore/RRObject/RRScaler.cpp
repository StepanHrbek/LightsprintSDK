// --------------------------------------------------------------------------
// Conversions between physical and custom (usually sRGB) space.
// Copyright (c) 2005-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cfloat>
#include <cmath>
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRGammaScaler - supports negative values
//
// With GammaScaler, you may directly work for example with 
// approximate screen colors.
// gamma=0.45 approximates screen colors
// gamma=1 makes no difference

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
	virtual void toCustomSpace(RRReal& a) const
	{
		RR_ASSERT(_finite(a));
		a = (a>=0)?pow(a,gamma):-pow(-a,gamma);
		RR_ASSERT(_finite(a));
	}
	virtual void toLinearSpace(RRReal& a) const
	{
		RR_ASSERT(_finite(a));
		a = (a>=0)?pow(a,invGamma):-pow(-a,invGamma);
		RR_ASSERT(_finite(a));
	}
	virtual void toCustomSpace(RRVec3& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRVec3(
			(color[0]>=0)?pow(color[0],gamma):-pow(-color[0],gamma),
			(color[1]>=0)?pow(color[1],gamma):-pow(-color[1],gamma),
			(color[2]>=0)?pow(color[2],gamma):-pow(-color[2],gamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual void toLinearSpace(RRVec3& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRVec3(
			(color[0]>=0)?pow(color[0],invGamma):-pow(-color[0],invGamma),
			(color[1]>=0)?pow(color[1],invGamma):-pow(-color[1],invGamma),
			(color[2]>=0)?pow(color[2],invGamma):-pow(-color[2],invGamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual RRVec3 getLinearSpace(const unsigned char rgb[3]) const
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
// RRFastGammaScaler - returns NaN for negative values
//

class RRFastGammaScaler : public RRColorSpace
{
public:
	RRFastGammaScaler(RRReal agamma)
	{
		gamma = agamma;
		invGamma = 1/gamma;
		for (unsigned i=0;i<256;i++)
			byteToLinear[i] = pow(float(i)/255,invGamma);
	}
	virtual void toCustomSpace(RRReal& a) const
	{
		RR_ASSERT(_finite(a));
		a = pow(a,gamma);
		RR_ASSERT(_finite(a));
	}
	virtual void toLinearSpace(RRReal& a) const
	{
		RR_ASSERT(_finite(a));
		a = pow(a,invGamma);
		RR_ASSERT(_finite(a));
	}
	virtual void toCustomSpace(RRVec3& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRVec3(
			pow(color[0],gamma),
			pow(color[1],gamma),
			pow(color[2],gamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual void toLinearSpace(RRVec3& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRVec3(
			pow(color[0],invGamma),
			pow(color[1],invGamma),
			pow(color[2],invGamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual RRVec3 getLinearSpace(const unsigned char rgb[3]) const
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

RRColorSpace* RRColorSpace::createRgbScaler(RRReal power)
{
	return new RRGammaScaler(power);
}

RRColorSpace* RRColorSpace::createFastRgbScaler(RRReal power)
{
	return new RRFastGammaScaler(power);
}


} // namespace
