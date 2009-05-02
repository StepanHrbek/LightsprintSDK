// --------------------------------------------------------------------------
// Light source (point, spot, dir, but not emissive material, not skybox).
// Copyright (c) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"
#include "RRMathPrivate.h"

namespace rr
{

#define PREVENT_INF 0.000000000137289f // used to avoid INFs and NaNs or make them extremely rare
#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

const RRVec4& warnIfNegative(const RRVec4& a, const char* name)
{
	if (a[0]<0 || a[1]<0 || a[2]<0 || a[3]<0)
		LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f %f %f %f).\n",name,a[0],a[1],a[2],a[3]));
	return a;
}

const RRVec3& warnIfNegative(const RRVec3& a, const char* name)
{
	if (a[0]<0 || a[1]<0 || a[2]<0)
		LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f %f %f).\n",name,a[0],a[1],a[2]));
	return a;
}

RRReal warnIfNegative(RRReal a, const char* name)
{
	if (a<0)
		LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f).\n",name,a));
	return a;
}


//////////////////////////////////////////////////////////////////////////////
//
// DirectionalLight
// color is in physical scale

class DirectionalLight : public RRLight
{
public:
	DirectionalLight(const RRVec3& _direction, const RRVec3& _color, bool _physicalScale)
	{
		type = DIRECTIONAL;
		distanceAttenuationType = NONE;
		direction = _direction.normalized();
		color = warnIfNegative(_color,"color");
		if (!_physicalScale)
		{
			color[0] = pow(color[0],2.2222f);
			color[1] = pow(color[1],2.2222f);
			color[2] = pow(color[2],2.2222f);
		}
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_VEC3(color));
		return color;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPhys
// color is in physical scale

class PointLightPhys : public RRLight
{
public:
	PointLightPhys(const RRVec3& _position, const RRVec3& _color)
	{
		type = POINT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		color = warnIfNegative(_color,"color");
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RRVec3 result = color / (PREVENT_INF+(receiverPosition-position).length2());
		RR_ASSERT(IS_VEC3(result));
		return result;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightNoAtt
// color is in physical scale

class PointLightNoAtt : public RRLight
{
public:
	PointLightNoAtt(const RRVec3& _position, const RRVec3& _color)
	{
		type = POINT;
		distanceAttenuationType = NONE;
		position = _position;
		color = warnIfNegative(_color,"color");
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_VEC3(color));
		return color;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightRadiusExp
// color is in custom scale

class PointLightRadiusExp : public RRLight
{
public:
	PointLightRadiusExp(const RRVec3& _position, const RRVec3& _color, RRReal _radius, RRReal _fallOffExponent)
	{
		type = POINT;
		distanceAttenuationType = EXPONENTIAL;
		position = _position;
		color = warnIfNegative(_color,"color");
		radius = warnIfNegative(_radius,"radius");
		fallOffExponent = _fallOffExponent;
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(radius>0 && _finite(radius));
		RR_ASSERT(fallOffExponent>=0 && _finite(fallOffExponent));
		float distanceAttenuation = pow(RR_MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
		RR_ASSERT(_finite(distanceAttenuation));
		RRVec3 result = color * distanceAttenuation;
		RR_ASSERT(IS_VEC3(result));
		return result;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// PointLightPoly
// color is in custom scale

class PointLightPoly : public RRLight
{
public:
	PointLightPoly(const RRVec3& _position, const RRVec3& _color, RRVec4 _polynom)
	{
		type = POINT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		color = warnIfNegative(_color,"color");
		polynom = warnIfNegative(_polynom,"polynom");
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		float distanceAttenuation = 1/RR_MAX(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2(),polynom[3]);
		RRVec3 irradiance = color * distanceAttenuation;
		if (scaler) scaler->getPhysicalScale(irradiance);
		RR_ASSERT(IS_VEC3(irradiance));
		return irradiance;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPhys
// color is in physical scale

class SpotLightPhys : public RRLight
{
public:
	SpotLightPhys(const RRVec3& _position, const RRVec3& _color, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = PHYSICAL;
		position = _position;
		color = warnIfNegative(_color,"color");
		direction = _direction.normalized();
		#define DELTA 0.0001f
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
		float distanceAttenuation = 1/(PREVENT_INF+(receiverPosition-position).length2());
		float angleRad = acos(dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		RRVec3 result = color * attenuation;
		RR_ASSERT(IS_VEC3(result));
		return result;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightNoAtt
// color is in physical scale

class SpotLightNoAtt : public RRLight
{
public:
	SpotLightNoAtt(const RRVec3& _position, const RRVec3& _color, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = NONE;
		position = _position;
		color = warnIfNegative(_color,"color");
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
		float angleRad = acos(dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = CLAMPED(angleAttenuation,0,1);
		RRVec3 result = color * attenuation;
		RR_ASSERT(IS_VEC3(result));
		return result;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightRadiusExp
// color is in custom scale

class SpotLightRadiusExp : public RRLight
{
public:
	SpotLightRadiusExp(const RRVec3& _position, const RRVec3& _color, RRReal _radius, RRReal _fallOffExponent, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad)
	{
		type = SPOT;
		distanceAttenuationType = EXPONENTIAL;
		position = _position;
		color = warnIfNegative(_color,"color");
		radius = warnIfNegative(_radius,"radius");
		fallOffExponent = _fallOffExponent;
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
		RR_ASSERT(radius>0 && _finite(radius));
		RR_ASSERT(fallOffExponent>=0 && _finite(fallOffExponent));
		float distanceAttenuation = pow(RR_MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
		float angleRad = acos(dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		float attenuation = distanceAttenuation * CLAMPED(angleAttenuation,0,1);
		RRVec3 result = color * attenuation;
		RR_ASSERT(IS_VEC3(result));
		return result;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// SpotLightPoly
// color is in custom scale

class SpotLightPoly : public RRLight
{
public:
	SpotLightPoly(const RRVec3& _position, const RRVec3& _color, RRVec4 _polynom, const RRVec3& _direction, RRReal _outerAngleRad, RRReal _fallOffAngleRad, RRReal _spotExponent)
	{
		type = SPOT;
		distanceAttenuationType = POLYNOMIAL;
		position = _position;
		color = warnIfNegative(_color,"color");
		polynom = warnIfNegative(_polynom,"polynom");
		direction = _direction.normalized();
		outerAngleRad = CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		spotExponent = CLAMPED(_spotExponent,0,1e10f);
	}
	virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
	{
		RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
		float distanceAttenuation = 1/RR_MAX(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2(),polynom[3]);
		float angleRad = acos(dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized()));
		float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		angleAttenuation = pow(CLAMPED(angleAttenuation,0.00001f,1),spotExponent);
		RRVec3 irradiance = color * distanceAttenuation * angleAttenuation;
		if (scaler) scaler->getPhysicalScale(irradiance);
		RR_ASSERT(IS_VEC3(irradiance));
		return irradiance;
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// RRLight

RRLight::RRLight()
{
	type = POINT;
	position = RRVec3(0);
	direction = RRVec3(0);
	outerAngleRad = 1;
	radius = 1;
	color = RRVec3(1);
	distanceAttenuationType = NONE;
	polynom = RRVec4(0,0,0,1);
	fallOffExponent = 1;
	fallOffAngleRad = 0;
	spotExponent = 1;
	castShadows = true;
	customData = NULL;
	rtProjectedTextureFilename = NULL;
	rtMaxShadowSize = 1000;
}

RRLight::~RRLight()
{
	free(rtProjectedTextureFilename);
}

RRVec3 RRLight::getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
{
	// calls implementation from subclass
	// we can do this because subclasses differ only in code, not in data
	#define CALL(SUBCLASS) return ((SUBCLASS*)this)->SUBCLASS::getIrradiance(receiverPosition, scaler);

	switch(type)
	{
		case DIRECTIONAL: CALL(DirectionalLight);
		case POINT:
			switch(distanceAttenuationType)
			{
				case NONE: CALL(PointLightNoAtt);
				case PHYSICAL: CALL(PointLightPhys);
				case POLYNOMIAL: CALL(PointLightPoly);
				case EXPONENTIAL: CALL(PointLightRadiusExp);
			}
		case SPOT:
			switch(distanceAttenuationType)
			{
				case NONE: CALL(SpotLightNoAtt);
				case PHYSICAL: CALL(SpotLightPhys);
				case POLYNOMIAL: CALL(SpotLightPoly);
				case EXPONENTIAL: CALL(SpotLightRadiusExp);
			}
	}
	// invalid light, it is in an unsupported internal state
	RR_ASSERT(0);
	return RRVec3(0);
}

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

RRLight* RRLight::createPointLightPoly(const RRVec3& position, const RRVec3& color, RRVec4 polynom)
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

RRLight* RRLight::createSpotLightPoly(const RRVec3& position, const RRVec3& color, RRVec4 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad, RRReal spotExponent)
{
	return new SpotLightPoly(position,color,polynom,majorDirection,outerAngleRad,fallOffAngleRad,spotExponent);
}

} // namespace
