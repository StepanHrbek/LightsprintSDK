#include "Lightsprint/RRDynamicSolver.h"
#include "../../RRMath/RRMathPrivate.h"

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
// SpotLight

class SpotLight : public RRLight
{
public:
	SpotLight(const RRVec3& aposition, const RRVec3& airradianceAtDistance1, const RRVec3& adirection, RRReal aouterAngle, RRReal afallOffAngle)
	{
		type = POINT;
		position = aposition;
		irradianceAtDistance1 = airradianceAtDistance1;
		direction = adirection.normalized();
		outerAngle = CLAMPED(aouterAngle,0.001f,1);
		fallOffAngle = CLAMPED(afallOffAngle,0.001f,outerAngle);
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(receiverPosition-position).length2();
		float angle = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngle-angle)/fallOffAngle;
		return irradianceAtDistance1 * distanceAttenuation * CLAMPED(angleAttenuation,0,1);
	}
	RRColorRGBF irradianceAtDistance1;
	RRReal outerAngle;
	RRReal fallOffAngle;
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

RRLight* RRLight::createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& direction, RRReal outerAngle, RRReal fallOffAngle)
{
	return new SpotLight(position,irradianceAtDistance1,direction,outerAngle,fallOffAngle);
}

} // namespace
