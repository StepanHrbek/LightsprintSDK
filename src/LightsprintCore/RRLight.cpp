// --------------------------------------------------------------------------
// Light source (point, spot, dir, but not emissive material, not skybox).
// Copyright (c) 2005-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"
#include "RRMathPrivate.h"

namespace rr
{

#define PREVENT_INF 0.000000000137289f // used to avoid INFs and NaNs or make them extremely rare

const RRVec4& warnIfNegative(const RRVec4& a, const char* name)
{
	if (a[0]<0 || a[1]<0 || a[2]<0 || a[3]<0 || !IS_VEC4(a))
		RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f %f %f %f).\n",name,a[0],a[1],a[2],a[3]));
	return a;
}

const RRVec3& warnIfNegative(const RRVec3& a, const char* name)
{
	if (a[0]<0 || a[1]<0 || a[2]<0 || !IS_VEC3(a))
		RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f %f %f).\n",name,a[0],a[1],a[2]));
	return a;
}

RRReal warnIfNegative(RRReal a, const char* name)
{
	if (a<0 || !_finite(a))
		RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with negative %s (%f).\n",name,a));
	return a;
}

const RRVec3 warnIfZero(const RRVec3& a, const char* name)
{
	if (a==RRVec3(0) || !IS_VEC3(a))
	{
		RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with invalid %s (%f %f %f).\n",name,a[0],a[1],a[2]));
		return RRVec3(1,0,0);
	}
	return a;
}

const RRVec3 warnIfNaN(const RRVec3& a, const char* name)
{
	if (!IS_VEC3(a))
	{
		RR_LIMITED_TIMES(5,RRReporter::report(WARN,"Light initialized with invalid %s (%f %f %f).\n",name,a[0],a[1],a[2]));
		return RRVec3(0,0,0);
	}
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
		direction = warnIfZero(_direction,"direction").normalized();
		color = warnIfNegative(_color,"color");
		if (!_physicalScale)
		{
			color[0] = pow(color[0],2.2222f);
			color[1] = pow(color[1],2.2222f);
			color[2] = pow(color[2],2.2222f);
		}
		rtNumShadowmaps = 3;
		rtShadowmapSize = 2048;
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		radius = warnIfNegative(_radius,"radius");
		fallOffExponent = warnIfNegative(_fallOffExponent,"fallOffExponent");
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		polynom = warnIfNegative(_polynom,"polynom");
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		direction = warnIfZero(_direction,"direction").normalized();
		#define DELTA 0.0001f
		outerAngleRad = RR_CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		rtNumShadowmaps = 1;
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		direction = warnIfZero(_direction,"direction").normalized();
		outerAngleRad = RR_CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		rtNumShadowmaps = 1;
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		radius = warnIfNegative(_radius,"radius");
		fallOffExponent = warnIfNegative(_fallOffExponent,"fallOffExponent");
		direction = warnIfZero(_direction,"direction").normalized();
		outerAngleRad = RR_CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		rtNumShadowmaps = 1;
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
		position = warnIfNaN(_position,"position");
		color = warnIfNegative(_color,"color");
		polynom = warnIfNegative(_polynom,"polynom");
		direction = warnIfZero(_direction,"direction").normalized();
		outerAngleRad = RR_CLAMPED(_outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,DELTA,outerAngleRad);
		spotExponent = RR_CLAMPED(_spotExponent,0,1e10f);
		rtNumShadowmaps = 1;
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
	enabled	= true;
	castShadows = true;
	directLambertScaled = false;
	rtProjectedTexture = NULL;
	rtNumShadowmaps = 6;
	rtShadowmapSize = 1024;
	customData = NULL;
}

RRVec3 RRLight::getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
{
	switch(type)
	{
		case DIRECTIONAL:
		{
			RR_ASSERT(IS_VEC3(color));
			return color;
		}
		case POINT:
		{
			switch(distanceAttenuationType)
			{
				case NONE:
				{
					RR_ASSERT(IS_VEC3(color));
					return color;
				}
				case PHYSICAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RRVec3 result = color / (PREVENT_INF+(receiverPosition-position).length2());
					RR_ASSERT(IS_VEC3(result));
					return result;
				}
				case POLYNOMIAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					float distanceAttenuation = 1/RR_MAX(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2(),polynom[3]);
					RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
					RRVec3 irradiance = color * distanceAttenuation;
					if (scaler) scaler->getPhysicalScale(irradiance);
					RR_ASSERT(IS_VEC3(irradiance));
					return irradiance;
				}
				case EXPONENTIAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RR_ASSERT(radius>0 && _finite(radius));
					RR_ASSERT(fallOffExponent>=0 && _finite(fallOffExponent));
					float distanceAttenuation = pow(RR_MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
					RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
					RRVec3 result = color * distanceAttenuation;
					RR_ASSERT(IS_VEC3(result));
					return result;
				}
			}
		}
		case SPOT:
		{
			switch(distanceAttenuationType)
			{
				case NONE:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
					float angleCos = dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized());
					float angleRad = acos(RR_CLAMPED(angleCos,-1,1)); // clamp prevents NaN from values like -1.0001 or +1.0001
					RR_ASSERT(_finite(angleRad));
					float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
					RR_ASSERT(_finite(angleAttenuation));
					float attenuation = RR_CLAMPED(angleAttenuation,0,1);
					RRVec3 result = color * attenuation;
					RR_ASSERT(IS_VEC3(result));
					return result;
				}
				case PHYSICAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
					float distanceAttenuation = 1/(PREVENT_INF+(receiverPosition-position).length2());
					RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
					float angleCos = dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized());
					float angleRad = acos(RR_CLAMPED(angleCos,-1,1)); // clamp prevents NaN from values like -1.0001 or +1.0001
					RR_ASSERT(_finite(angleRad));
					float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
					RR_ASSERT(_finite(angleAttenuation));
					float attenuation = distanceAttenuation * RR_CLAMPED(angleAttenuation,0,1);
					RRVec3 result = color * attenuation;
					RR_ASSERT(IS_VEC3(result));
					return result;
				}
				case POLYNOMIAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
					float distanceAttenuation = 1/RR_MAX(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2(),polynom[3]);
					RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
					float angleCos = dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized());
					float angleRad = acos(RR_CLAMPED(angleCos,-1,1)); // clamp prevents NaN from values like -1.0001 or +1.0001
					RR_ASSERT(_finite(angleRad));
					float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
					RR_ASSERT(_finite(angleAttenuation));
					angleAttenuation = pow(RR_CLAMPED(angleAttenuation,0.00001f,1),spotExponent);
					RRVec3 irradiance = color * distanceAttenuation * angleAttenuation;
					if (scaler) scaler->getPhysicalScale(irradiance);
					RR_ASSERT(IS_VEC3(irradiance));
					return irradiance;
				}
				case EXPONENTIAL:
				{
					RR_ASSERT(IS_VEC3(receiverPosition));
					RR_ASSERT(IS_VEC3(position));
					RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
					RR_ASSERT(radius>0 && _finite(radius));
					RR_ASSERT(fallOffExponent>=0 && _finite(fallOffExponent));
					float distanceAttenuation = pow(RR_MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
					RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
					float angleCos = dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized());
					float angleRad = acos(RR_CLAMPED(angleCos,-1,1)); // clamp prevents NaN from values like -1.0001 or +1.0001
					RR_ASSERT(_finite(angleRad));
					float angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
					RR_ASSERT(_finite(angleAttenuation));
					float attenuation = distanceAttenuation * RR_CLAMPED(angleAttenuation,0,1);
					RRVec3 result = color * attenuation;
					RR_ASSERT(IS_VEC3(result));
					return result;
				}
			}
		}
	}
	// invalid light, it is in an unsupported internal state
	RR_ASSERT(0);
	return RRVec3(0);
}

bool RRLight::operator ==(const RRLight& a) const
{
	// Q: should we ignore unused variables (like position in directional light)?
	// A: yes at least for position in dirlight, it freely changes
	//    irrelevant for other unused variables, they don't change
	return 1
		&& a.type==type
		&& (type==DIRECTIONAL || a.position==position) // position may differ in directional light
		&& a.direction==direction
		&& a.outerAngleRad==outerAngleRad
		&& a.radius==radius
		&& a.color==color
		&& a.distanceAttenuationType==distanceAttenuationType
		&& a.polynom==polynom
		&& a.fallOffExponent==fallOffExponent
		&& a.spotExponent==spotExponent
		&& a.fallOffAngleRad==fallOffAngleRad
		&& a.enabled==enabled
		&& a.castShadows==castShadows
		&& a.directLambertScaled==directLambertScaled
		&& a.name==name
		&& a.rtProjectedTexture==rtProjectedTexture
		&& a.rtNumShadowmaps==rtNumShadowmaps
		&& a.customData==customData
		;
}

bool RRLight::operator !=(const RRLight& a) const
{
	return !(a==*this);
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
