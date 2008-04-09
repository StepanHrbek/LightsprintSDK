// --------------------------------------------------------------------------
// Light source (point, spot, dir, but not emissive material, not skybox).
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"
#include "RRMathPrivate.h"

namespace rr
{

#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

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
		color = rr::RRVec3(0);
		polynom = rr::RRVec3(0);
		fallOffExponent = 0;
		fallOffAngleRad = 0;
		spotExponent = 1;
		castShadows = true;
		customData = NULL;
	}
};

const RRVec3& check(const RRVec3& a)
{
	if(a[0]<0 && a[1]<0 && a[2]<0)
		LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative value (%f %f %f).\n",a[0],a[1],a[2]));
	return a;
}

RRReal check(RRReal a)
{
	if(a<0)
		LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative scalar (%f).\n",a));
	return a;
}

//////////////////////////////////////////////////////////////////////////////
//
// DirectionalLight
// color is in physical scale

class DirectionalLight : public CleanLight
{
public:
	DirectionalLight(const RRVec3& _direction, const RRVec3& _color, bool _physicalScale)
	{
		type = DIRECTIONAL;
		distanceAttenuationType = NONE;
		direction = _direction.normalized();
		color = check(_color);
		if(!_physicalScale)
		{
			color[0] = pow(color[0],2.2222f);
			color[1] = pow(color[1],2.2222f);
			color[2] = pow(color[2],2.2222f);
		}
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return color;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPhys
// color is in physical scale

class PointLightPhys : public CleanLight
{
public:
	PointLightPhys(const RRVec3& _position, const RRVec3& _color)
	{
		type = POINT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		color = check(_color);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return color / (receiverPosition-position).length2();
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightNoAtt
// color is in physical scale

class PointLightNoAtt : public CleanLight
{
public:
	PointLightNoAtt(const RRVec3& _position, const RRVec3& _color)
	{
		type = POINT;
		distanceAttenuationType = NONE;
		position = _position;
		color = check(_color);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		return color;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightRadiusExp
// color is in custom scale

class PointLightRadiusExp : public CleanLight
{
public:
	PointLightRadiusExp(const RRVec3& _position, const RRVec3& _color, RRReal _radius, RRReal _fallOffExponent)
	{
		type = POINT;
		distanceAttenuationType = EXPONENTIAL;
		position = _position;
		color = check(_color);
		radius = check(_radius);
		fallOffExponent = _fallOffExponent;
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = pow(MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
		return color * distanceAttenuation;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPoly
// color is in custom scale

class PointLightPoly : public CleanLight
{
public:
	PointLightPoly(const RRVec3& _position, const RRVec3& _color, RRVec3 _polynom)
	{
		type = POINT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		color = check(_color);
		polynom = check(_polynom);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2());
		RRVec3 irradiance = color * distanceAttenuation;
		if(scaler) scaler->getPhysicalScale(irradiance);
		return irradiance;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPhys
// color is in physical scale

class SpotLightPhys : public CleanLight
{
public:
	SpotLightPhys(const RRVec3& _position, const RRVec3& _color, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		color = check(_color);
		direction = _direction.normalized();
		#define DELTA 0.0001f
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,M_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(receiverPosition-position).length2();
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		return color * attenuation;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightNoAtt
// color is in physical scale

class SpotLightNoAtt : public CleanLight
{
public:
	SpotLightNoAtt(const RRVec3& _position, const RRVec3& _color, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = NONE;
		position = _position;
		color = check(_color);
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,M_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = CLAMPED(angleAttenuation,0,1);
		return color * attenuation;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightRadiusExp
// color is in custom scale

class SpotLightRadiusExp : public CleanLight
{
public:
	SpotLightRadiusExp(const RRVec3& _position, const RRVec3& _color, RRReal _radius, RRReal _fallOffExponent, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = EXPONENTIAL;
		position = _position;
		color = check(_color);
		radius = check(_radius);
		fallOffExponent = _fallOffExponent;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,M_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = pow(MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		return color * attenuation;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPoly
// color is in custom scale

class SpotLightPoly : public CleanLight
{
public:
	SpotLightPoly(const RRVec3& _position, const RRVec3& _color, RRVec3 _polynom, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad, RRReal _spotExponent)
	{
		type = SPOT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		color = check(_color);
		polynom = _polynom;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,M_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		spotExponent = CLAMPED(_spotExponent,0,1e10f);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2());
		float angleRad = acos(dot(direction,(receiverPosition-position).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		angleAttenuation = pow(CLAMPED(angleAttenuation,0.00001f,1),spotExponent);
		RRVec3 irradiance = color * distanceAttenuation * angleAttenuation;
		if(scaler) scaler->getPhysicalScale(irradiance);
		return irradiance;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLight

RRLight* RRLight::createDirectionalLight(const RRVec3& direction, const RRVec3& color, bool physicalScale)
{
	return new DirectionalLight(direction,color,physicalScale);
}

RRLight* RRLight::createPointLight(const RRVec3& position, const RRVec3& color)
{
	return new PointLightPhys(position,color);
}

RRLight* RRLight::createPointLightNoAtt(const RRVec3& position, const RRVec3& color)
{
	return new PointLightNoAtt(position,color);
}

RRLight* RRLight::createPointLightRadiusExp(const RRVec3& position, const RRVec3& color, RRReal radius, RRReal fallOffExponent)
{
	return new PointLightRadiusExp(position,color,radius,fallOffExponent);
}

RRLight* RRLight::createPointLightPoly(const RRVec3& position, const RRVec3& color, RRVec3 polynom)
{
	return new PointLightPoly(position,color,polynom);
}

RRLight* RRLight::createSpotLight(const RRVec3& position, const RRVec3& color, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightPhys(position,color,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightNoAtt(const RRVec3& position, const RRVec3& color, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightNoAtt(position,color,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightRadiusExp(const RRVec3& position, const RRVec3& color, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad)
{
	return new SpotLightRadiusExp(position,color,radius,fallOffExponent,majorDirection,outerAngleRad,fallOffAngleRad);
}

RRLight* RRLight::createSpotLightPoly(const RRVec3& position, const RRVec3& color, RRVec3 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad, RRReal spotExponent)
{
	return new SpotLightPoly(position,color,polynom,majorDirection,outerAngleRad,fallOffAngleRad,spotExponent);
}

} // namespace
