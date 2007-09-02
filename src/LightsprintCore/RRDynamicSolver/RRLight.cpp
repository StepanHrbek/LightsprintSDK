#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"

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
// PointLightPhys

class PointLightPhys : public RRLight
{
public:
	PointLightPhys(const RRVec3& _position, const RRVec3& _irradianceAtDistance1)
	{
		type = POINT;
		position = _position;
		irradianceAtDistance1 = _irradianceAtDistance1;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return irradianceAtDistance1 / (receiverPosition-position).length2();
	}
	RRColorRGBF irradianceAtDistance1;
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightRadiusExp

class PointLightRadiusExp : public RRLight
{
public:
	PointLightRadiusExp(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRReal _radius, RRReal _fallOffExponent)
	{
		type = POINT;
		position = _position;
		colorAtDistance0 = _colorAtDistance0;
		radius = _radius;
		fallOffExponent = _fallOffExponent;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = pow(MAX(0,1-(receiverPosition-position).length()/radius),fallOffExponent);
		RRColor color = colorAtDistance0 * distanceAttenuation;
		if(scaler) scaler->getPhysicalScale(color);
		return color;
	}
	RRColorRGBF colorAtDistance0;
	RRReal radius;
	RRReal fallOffExponent;
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPhys

class SpotLightPhys : public RRLight
{
public:
	SpotLightPhys(const RRVec3& _position, const RRVec3& _irradianceAtDistance1, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = POINT;
		position = _position;
		irradianceAtDistance1 = _irradianceAtDistance1;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,0.001f,1);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,0.001f,outerAngleRad);
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(receiverPosition-position).length2();
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		return irradianceAtDistance1 * attenuation;
	}
	RRColorRGBF irradianceAtDistance1;
	RRReal outerAngleRad;
	RRReal fallOffAngleRad;
};



//////////////////////////////////////////////////////////////////////////////
//
// SpotLightRadiusExp

class SpotLightRadiusExp : public RRLight
{
public:
	SpotLightRadiusExp(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRReal _radius, RRReal _fallOffExponent, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = POINT;
		position = _position;
		colorAtDistance0 = _colorAtDistance0;
		radius = _radius;
		fallOffExponent = _fallOffExponent;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,0.001f,1);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,0.001f,outerAngleRad);
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = pow(MAX(0,1-(receiverPosition-position).length()/radius),fallOffExponent);
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		RRColor color = colorAtDistance0 * attenuation;
		if(scaler) scaler->getPhysicalScale(color);
		return color;
	}
	RRColorRGBF colorAtDistance0;
	RRReal radius;
	RRReal fallOffExponent;
	RRReal outerAngleRad;
	RRReal fallOffAngleRad;
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
	return new PointLightPhys(position,irradianceAtDistance1);
}

RRLight* RRLight::createPointLightRadiusExp(const RRVec3& position, const RRVec3& colorAtDistance0, RRReal radius, RRReal fallOffExponent)
{
	return new PointLightRadiusExp(position,colorAtDistance0,radius,fallOffExponent);
}

RRLight* RRLight::createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightPhys(position,irradianceAtDistance1,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightRadiusExp(const RRVec3& position, const RRVec3& colorAtDistance0, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightRadiusExp(position,colorAtDistance0,radius,fallOffExponent,majorDirection,outerAngleRad,fallOffAngleRad);
}

} // namespace
