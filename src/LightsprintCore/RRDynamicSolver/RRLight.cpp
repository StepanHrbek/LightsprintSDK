#include "Lightsprint/RRDynamicSolver.h"
#include "../RRMathPrivate.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// helps clean unused attributes

class CleanLight : public RRLight
{
public:
	CleanLight()
	{
		position = rr::RRVec3(0);
		direction = rr::RRVec3(0);
		outerAngleRad = 0;
		radius = 0;
		irradianceAtDistance1 = rr::RRVec3(0);
		colorAtDistance0 = rr::RRVec3(0);
		polynom = rr::RRVec3(0);
		fallOffExponent = 0;
		fallOffAngleRad = 0;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// DirectionalLight

class DirectionalLight : public CleanLight
{
public:
	DirectionalLight(const RRVec3& adirection, const RRVec3& airradiance)
	{
		type = DIRECTIONAL;
		distanceAttenuationType = NONE;
		direction = adirection;
		irradianceAtDistance1 = airradiance;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return irradianceAtDistance1;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPhys

class PointLightPhys : public CleanLight
{
public:
	PointLightPhys(const RRVec3& _position, const RRVec3& _irradianceAtDistance1)
	{
		type = POINT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		irradianceAtDistance1 = _irradianceAtDistance1;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return irradianceAtDistance1 / (receiverPosition-position).length2();
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightRadiusExp

class PointLightRadiusExp : public CleanLight
{
public:
	PointLightRadiusExp(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRReal _radius, RRReal _fallOffExponent)
	{
		type = POINT;
		distanceAttenuationType = EXPONENTIAL;
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
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPoly

class PointLightPoly : public CleanLight
{
public:
	PointLightPoly(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRVec3 _polynom)
	{
		type = POINT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		colorAtDistance0 = _colorAtDistance0;
		polynom = _polynom;
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2());
		RRColor color = colorAtDistance0 * distanceAttenuation;
		if(scaler) scaler->getPhysicalScale(color);
		return color;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPhys

class SpotLightPhys : public CleanLight
{
public:
	SpotLightPhys(const RRVec3& _position, const RRVec3& _irradianceAtDistance1, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		irradianceAtDistance1 = _irradianceAtDistance1;
		direction = _direction.normalized();
		#define DELTA 0.0001f
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,1-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(receiverPosition-position).length2();
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		return irradianceAtDistance1 * attenuation;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightRadiusExp

class SpotLightRadiusExp : public CleanLight
{
public:
	SpotLightRadiusExp(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRReal _radius, RRReal _fallOffExponent, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = EXPONENTIAL;
		position = _position;
		colorAtDistance0 = _colorAtDistance0;
		radius = _radius;
		fallOffExponent = _fallOffExponent;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,1-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
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
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPoly

class SpotLightPoly : public CleanLight
{
public:
	SpotLightPoly(const RRVec3& _position, const RRVec3& _colorAtDistance0, RRVec3 _polynom, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		colorAtDistance0 = _colorAtDistance0;
		polynom = _polynom;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,1-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRColorRGBF getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2());
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		RRColor color = colorAtDistance0 * attenuation;
		if(scaler) scaler->getPhysicalScale(color);
		return color;
	}
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

RRLight* RRLight::createPointLightPoly(const RRVec3& position, const RRVec3& colorAtDistance0, RRVec3 polynom)
{
	return new PointLightPoly(position,colorAtDistance0,polynom);
}

RRLight* RRLight::createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightPhys(position,irradianceAtDistance1,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightRadiusExp(const RRVec3& position, const RRVec3& colorAtDistance0, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightRadiusExp(position,colorAtDistance0,radius,fallOffExponent,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightPoly(const RRVec3& position, const RRVec3& colorAtDistance0, RRVec3 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightPoly(position,colorAtDistance0,polynom,majorDirection,outerAngleRad,fallOffAngleRad);
}

} // namespace
