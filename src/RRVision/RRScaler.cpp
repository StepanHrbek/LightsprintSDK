#include <math.h>

#include "RRVision.h"

namespace rrVision
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
	virtual RRReal getScaled(RRReal original) const
	{
		return pow(original,gamma);
	}
	virtual RRReal getOriginal(RRReal scaled) const
	{
		return pow(scaled,1/gamma);
	}
private:
	RRReal gamma;
};

//////////////////////////////////////////////////////////////////////////////
//
// RRScaler

RRScaler* RRScaler::createGammaScaler(RRReal gamma)
{
	return new RRGammaScaler(gamma);
}


} // namespace
