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
// gamma=0.45 approximates screen colors
// gamma=1 makes no difference

class RRGammaScaler : public RRScaler
{
public:
	RRGammaScaler(RRReal agamma)
	{
		gamma = agamma;
	}
	virtual void getCustomScale(RRColor& color) const
	{
		color = RRColor(
			pow(color[0],gamma),
			pow(color[1],gamma),
			pow(color[2],gamma)
			);
	}
	virtual void getPhysicalScale(RRColor& color) const
	{
		color = RRColor(
			pow(color[0],1/gamma),
			pow(color[1],1/gamma),
			pow(color[2],1/gamma)
			);
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
