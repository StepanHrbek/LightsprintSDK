// --------------------------------------------------------------------------
// Light source (point, spot, dir, but not emissive material, not skybox).
// Copyright (c) 2005-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRDebug.h"
#include "RRMathPrivate.h"

#define PHYS2SRGB 0.45f
#define SRGB2PHYS 2.22222222f

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
			color[0] = pow(color[0],SRGB2PHYS);
			color[1] = pow(color[1],SRGB2PHYS);
			color[2] = pow(color[2],SRGB2PHYS);
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
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,0,outerAngleRad);
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
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,0,outerAngleRad);
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
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,0,outerAngleRad);
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
		fallOffAngleRad = RR_CLAMPED(_fallOffAngleRad,0,outerAngleRad);
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
	rtShadowmapBias = RRVec2(1);
	//name = "";
	customData = NULL;
}

RRLight::RRLight(const RRLight& a)
{
	type = a.type;
	position = a.position;
	direction = a.direction;
	outerAngleRad = a.outerAngleRad;
	radius = a.radius;
	color = a.color;
	distanceAttenuationType = a.distanceAttenuationType;
	polynom = a.polynom;
	fallOffExponent = a.fallOffExponent;
	fallOffAngleRad = a.fallOffAngleRad;
	spotExponent = a.spotExponent;
	enabled	= a.enabled;
	castShadows = a.castShadows;
	directLambertScaled = a.directLambertScaled;
	rtProjectedTexture = a.rtProjectedTexture ? a.rtProjectedTexture->createReference() : NULL;
	rtNumShadowmaps = a.rtNumShadowmaps;
	rtShadowmapSize = a.rtShadowmapSize;
	rtShadowmapBias = a.rtShadowmapBias;
	name = a.name;
	customData = a.customData;
}

const RRLight& RRLight::operator=(const RRLight& a)
{
	type = a.type;
	position = a.position;
	direction = a.direction;
	outerAngleRad = a.outerAngleRad;
	radius = a.radius;
	color = a.color;
	distanceAttenuationType = a.distanceAttenuationType;
	polynom = a.polynom;
	fallOffExponent = a.fallOffExponent;
	fallOffAngleRad = a.fallOffAngleRad;
	spotExponent = a.spotExponent;
	enabled	= a.enabled;
	castShadows = a.castShadows;
	directLambertScaled = a.directLambertScaled;
	if (rtProjectedTexture!=a.rtProjectedTexture)
	{
		delete rtProjectedTexture;
		rtProjectedTexture = a.rtProjectedTexture ? a.rtProjectedTexture->createReference() : NULL;
	}
	rtNumShadowmaps = a.rtNumShadowmaps;
	rtShadowmapSize = a.rtShadowmapSize;
	rtShadowmapBias = a.rtShadowmapBias;
	name = a.name;
	customData = a.customData;
	return *this;
}

RRVec3 RRLight::getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const
{
	RR_ASSERT(IS_VEC3(color));

	if (type==DIRECTIONAL)
	{
		return color;
	}

	RR_ASSERT(IS_VEC3(receiverPosition));
	RR_ASSERT(IS_VEC3(position));

	float distanceAttenuation = 1;
	switch (distanceAttenuationType)
	{
		case PHYSICAL:
		{
			distanceAttenuation = 1/(PREVENT_INF+(receiverPosition-position).length2());
			RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
			break;
		}
		case POLYNOMIAL:
		{
			distanceAttenuation = 1/RR_MAX(polynom[0]+polynom[1]*(receiverPosition-position).length()+polynom[2]*(receiverPosition-position).length2(),polynom[3]);
			RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
			break;
		}
		case EXPONENTIAL:
		{
			RR_ASSERT(radius>0 && _finite(radius));
			RR_ASSERT(fallOffExponent>=0 && _finite(fallOffExponent));
			distanceAttenuation = pow(RR_MAX(0,1-(receiverPosition-position).length2()/(radius*radius)),fallOffExponent);
			RR_ASSERT(distanceAttenuation>=0 && _finite(distanceAttenuation));
			break;
		}
	}

	float angleAttenuation = 1;
	if (type==SPOT)
	{
		RR_ASSERT(IS_NORMALIZED(direction)); // must be normalized, otherwise acos might return NaN
		float angleCos = dot(direction,(receiverPosition-position+RRVec3(PREVENT_INF)).normalized());
		float angleRad = acos(RR_CLAMPED(angleCos,-1,1)); // clamp prevents NaN from values like -1.0001 or +1.0001
		RR_ASSERT(_finite(angleRad));
		angleAttenuation = (outerAngleRad-angleRad)/fallOffAngleRad;
		//RR_ASSERT(_finite(angleAttenuation)); // may be +/-1.#INF after division by zero, but next line clamps it back to 0,1 range
		// c++ defines pow(0,0)=1. we want 0, therefore we have to clamp exponent to small positive value
		angleAttenuation = pow(RR_CLAMPED(angleAttenuation,0,1),((distanceAttenuationType!=POLYNOMIAL)?SRGB2PHYS:1)*RR_MAX(spotExponent,1e-10f));
	}

	RRVec3 result = color * (distanceAttenuation * angleAttenuation);
	if (scaler && distanceAttenuationType==POLYNOMIAL)
		scaler->getPhysicalScale(result);
	RR_ASSERT(IS_VEC3(result));
	return result;
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

static void makeFinite(float& f, float def)
{
	if (!_finite(f)) f = def;
}

static void makeFinite(RRVec3& v, const RRVec3& def)
{
	makeFinite(v[0],def[0]);
	makeFinite(v[1],def[1]);
	makeFinite(v[2],def[2]);
}

static void makeFinite(RRVec4& v, const RRVec4& def)
{
	makeFinite(v[0],def[0]);
	makeFinite(v[1],def[1]);
	makeFinite(v[2],def[2]);
	makeFinite(v[3],def[3]);
}

void RRLight::fixInvalidValues()
{
	// light enevelope
	RR_CLAMP(type,DIRECTIONAL,SPOT);
	makeFinite(position,RRVec3(0));
	//makeFinite(direction,RRVec3(1,0,0));
	direction.normalizeSafe();
	#define DELTA 0.0001f
	RR_CLAMP(outerAngleRad,DELTA,RR_PI*0.5f-DELTA);
	RR_CLAMP(radius,0,1e20f);

	// color
	makeFinite(color,RRVec3(0));
	RR_CLAMP(distanceAttenuationType,NONE,EXPONENTIAL);
	makeFinite(polynom,RRVec4(0));
	makeFinite(fallOffExponent,1);
	makeFinite(spotExponent,1);
	RR_CLAMP(fallOffAngleRad,0,outerAngleRad);
}

} // namespace
