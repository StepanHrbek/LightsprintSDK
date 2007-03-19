#include "Lightsprint/RRRealtimeRadiosity.h"

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// DirectionalLight

class DirectionalLight : public RRLight
{
public:
	DirectionalLight(const RRVec3& adirection, const RRVec3& airradiance)
	{
		type = DIRECTIONAL;
		direction = adirection;
		irradiance = airradiance;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return irradiance;
	}
	RRColorRGBF irradiance;
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLight

class PointLight : public RRLight
{
public:
	PointLight(const RRVec3& aposition, const RRVec3& airradianceAtDistance1)
	{
		type = POINT;
		position = aposition;
		irradianceAtDistance1 = airradianceAtDistance1;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return irradianceAtDistance1 / (receiverPosition-position).length2();
	}
	RRColorRGBF irradianceAtDistance1;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLight

RRLight* RRLight::createDirectionalLight(const RRVec3& direction, const RRVec3& irradiance)
{
	return new DirectionalLight(direction,irradiance);
}

RRLight* RRLight::createPointLight(const RRVec3& position, const RRVec3& irradianceAtDistance1)
{
	return new PointLight(position,irradianceAtDistance1);
}

} // namespace
