#include <math.h>

#include "RRVision.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRGammaScaler
//
// With GammaScaler, you may directly work for example with 
// approximate screen colors.
// gamma=0.4 approximates screen colors
// gamma=1 makes no difference

class RRGammaScaler : public RRScaler
{
public:
	RRGammaScaler(RRReal agamma)
	{
		gamma = agamma;
	}
	virtual RRReal getScaled(RRReal standard) const
	{
		return pow(standard,gamma);
	}
	virtual RRReal getStandard(RRReal scaled) const
	{
		return pow(scaled,1/gamma);
	}
private:
	RRReal gamma;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRScaler

RRScaler* RRScaler::createRgbScaler(RRReal power)
{
	return new RRGammaScaler(power);
}


} // namespace
