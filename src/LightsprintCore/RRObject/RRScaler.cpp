#include <cassert>
#include <cfloat>
#include <cmath>

#include "Lightsprint/RRObject.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRGammaScaler
//
// With GammaScaler, you may directly work for example with 
// approximate screen colors.
// gamma=0.45 approximates screen colors
// gamma=1 makes no difference

class RRGammaScaler : public RRScaler
{
public:
	RRGammaScaler(RRReal agamma)
	{
		gamma = agamma;
		invGamma = 1/gamma;
	}
	virtual void getCustomScale(RRColor& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
#ifdef _DEBUG
		RRColor tmp = color;
#endif
		color = RRColor(
			(color[0]>=0)?pow(color[0],gamma):-pow(-color[0],gamma),
			(color[1]>=0)?pow(color[1],gamma):-pow(-color[1],gamma),
			(color[2]>=0)?pow(color[2],gamma):-pow(-color[2],gamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual void getPhysicalScale(RRColor& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRColor(
			// supports negative colors
			(color[0]>=0)?pow(color[0],invGamma):-pow(-color[0],invGamma),
			(color[1]>=0)?pow(color[1],invGamma):-pow(-color[1],invGamma),
			(color[2]>=0)?pow(color[2],invGamma):-pow(-color[2],invGamma)
			// faster, but returns NaN for negative colors
			//pow(color[0],invGamma),
			//pow(color[1],invGamma),
			//pow(color[2],invGamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
protected:
	RRReal gamma;
	RRReal invGamma;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRFastGammaScaler
//

class RRFastGammaScaler : public RRScaler
{
public:
	RRFastGammaScaler(RRReal agamma)
	{
		gamma = agamma;
		invGamma = 1/gamma;
	}
	virtual void getCustomScale(RRColor& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRColor(
			powf(color[0],gamma),
			powf(color[1],gamma),
			powf(color[2],gamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
	virtual void getPhysicalScale(RRColor& color) const
	{
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
		color = RRColor(
			powf(color[0],invGamma),
			powf(color[1],invGamma),
			powf(color[2],invGamma)
			);
		RR_ASSERT(_finite(color[0]));
		RR_ASSERT(_finite(color[1]));
		RR_ASSERT(_finite(color[2]));
	}
protected:
	RRReal gamma;
	RRReal invGamma;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRScaler

RRScaler* RRScaler::createRgbScaler(RRReal power)
{
	return new RRGammaScaler(power);
}

RRScaler* RRScaler::createFastRgbScaler(RRReal power)
{
	return new RRFastGammaScaler(power);
}


} // namespace
