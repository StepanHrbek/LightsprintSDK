#ifndef RRLIGHT_H
#define RRLIGHT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRLight.h
//! \brief LightsprintCore | light
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMemory.h"
#include "RRVector.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRScaler
	//! Interface for physical <-> custom space transformer.
	//
	//! RRScaler may be used to transform irradiance/emittance/exitance 
	//! between physical linear W/m^2 space and custom user defined space.
	//! Without scaler, all inputs/outputs work with specified physical units.
	//! With appropriate scaler, you may directly work for example with screen colors (sRGB)
	//! or photometric units.
	//!
	//! For best results, scaler should satisfy following conditions for any x,y,z:
	//! \n getPhysicalScale(x)*getPhysicalScale(y)=getPhysicalScale(x*y)
	//! \n getPhysicalScale(x*y)*getPhysicalScale(z)=getPhysicalScale(x)*getPhysicalScale(y*z)
	//! \n getCustomScale is inverse of getPhysicalScale
	//!
	//! When implementing your own scaler, double check you don't generate NaNs or INFs,
	//! for negative inputs.
	//!
	//! Contains built-in support for screen colors / sRGB, see createRgbScaler().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRScaler : public RRUniformlyAllocated
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! Converts color from physical scale (W/m^2) value to user defined scale.
		virtual void getCustomScale(RRVec3& physicalScale) const = 0;

		//! Converts color from user defined scale to physical scale (W/m^2).
		virtual void getPhysicalScale(RRVec3& customScale) const = 0;

		virtual ~RRScaler() {}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//
		// instance factory
		//

		//! Creates and returns scaler for standard RGB monitor space.
		//
		//! Scaler converts between radiometry units (W/m^2) and displayable RGB values.
		//! It is only approximation, exact conversion would depend on individual monitor 
		//! and eye attributes.
		//! \param power Exponent in formula screenSpace = physicalSpace^power.
		static RRScaler* createRgbScaler(RRReal power=0.45f);

		//! As createRgbScaler(), but slightly faster, with undefined results for negative numbers.
		static RRScaler* createFastRgbScaler(RRReal power=0.45f);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Direct light source, directional/point/spot light with programmable function.
	//
	//! Standard point/spot/dir lights with physical, polynomial or exponential
	//! distance attenuations are supported via createXxx functions.
	//!
	//! Custom lights with this interface may be created.
	//! For offline rendering in LightsprintCore, only #type, #position, #direction and getIrradiance() are relevant.
	//! For realtime rendering in LightsprintGL, implement(set propely) all attributes.
	//!
	//! Contains atributes of all standard lights.
	//! Unused attributes (e.g. fallOffExponent when distanceAttenuation=NONE) are set to zero.
	//!
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//! All custom implementations must be thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLight : public RRUniformlyAllocated
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Light envelope, defines area affected by light
		//////////////////////////////////////////////////////////////////////////////

		//! Types of lights.
		enum Type
		{
			//! Infinitely distant light source, all light rays are parallel (direction).
			DIRECTIONAL,
			//! Point light source, all light rays start in one point (position).
			POINT,
			//! Spot light source, all light rays start in one point (position) and leave it in cone (direction).
			SPOT,
		};
		//! Type of light source. Read only (lights from createXxx can't change type on the fly).
		Type type;

		//! Position of light source in world space. Relevant only for POINT and SPOT light. Read/write.
		RRVec3 position;

		//! Normalized direction of light in world space. Relevant only for DIRECTIONAL and SPOT light. Read/write.
		RRVec3 direction;

		//! Outer cone angle in radians. Relevant only for SPOT light. Read/write.
		//
		//! Light rays go in directions up to outerAngleRad far from direction.
		//! \n Valid range: (0,1). Out of range values are fixed at construction time,
		//! changes you make later are accepted without checks.
		RRReal outerAngleRad;

		//! Relevant only for distanceAttenuation==EXPONENTIAL.
		//! Distance in world space, where light disappears due to its distance attenuation.
		//! Light has effect in sphere of given radius.
		RRReal radius;


		//////////////////////////////////////////////////////////////////////////////
		// Color
		//////////////////////////////////////////////////////////////////////////////

		//! Light color. Interpretation depends on #distanceAttenuationType.
		RRVec3 color;

		//! Types of distance attenuation. Defines how light color is computed for given distance.
		enum DistanceAttenuationType
		{
			//! Intensity in physical scale is color. Very good approximation of sunlight.
			NONE,
			//! Intensity in physical scale is color/distance^2. This is exactly how reality works.
			PHYSICAL,
			//! Intensity in custom scale is color/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2). Used in fixed pipeline engines.
			POLYNOMIAL,
			//! Intensity in custom scale is color*pow(MAX(0,1-distance/radius),fallOffExponent). Used in UE3.
			EXPONENTIAL,
		};
		//! Type of distance attenuation. Read only (lights from createXxx can't change type on the fly).
		DistanceAttenuationType distanceAttenuationType;

		//! Relevant only for distanceAttenuation==POLYNOMIAL.
		//! Distance attenuation in custom scale is computed as colorCustom/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2).
		RRVec3 polynom;

		//! Relevant only for distanceAttenuation==EXPONENTIAL.
		//!  Distance attenuation in custom scale is computed as colorCustom*pow(MAX(0,1-distance/radius),fallOffExponent).
		RRReal fallOffExponent;

		//! Outer-inner code angle in radians. Relevant only for SPOT light. Read/write.
		//
		//! Light rays with direction diverted less than outerAngleRad from direction,
		//! but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//! If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		//! \n Valid range: (0,innerAngleRad>. Out of range values are fixed at construction time,
		//! changes you make later are accepted without checks.
		RRReal fallOffAngleRad;


		//////////////////////////////////////////////////////////////////////////////
		// Misc
		//////////////////////////////////////////////////////////////////////////////

		//! For your private use, not accessed by Lightsprint. Initialized to NULL.
		void* customData;


		//////////////////////////////////////////////////////////////////////////////
		// Offline rendering interface
		//////////////////////////////////////////////////////////////////////////////

		//! Irradiance in physical scale, programmable color, distance and spotlight attenuation function.
		//
		//! \param receiverPosition
		//!  Position of point in world space illuminated by this light.
		//! \param scaler
		//!  Currently active custom scaler, provided for your convenience.
		//!  You may compute irradiance directly in physical scale and don't use it,
		//!  which is the most efficient way,
		//!  but if you calculate in custom scale, convert your result to physical
		//!  scale using scaler->getPhysicalScale() before returning it.
		//!  \n Lightsprint calculates internally in physical scale, that's why
		//!  it's more efficient to expect result in physical scale
		//!  rather than in screen colors or any other custom scale.
		//! \return
		//!  Irradiance at receiverPosition, in physical scale [W/m^2],
		//!  assuming that receiver is oriented towards light.
		virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const = 0;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates directional light without distance attenuation.
		//
		//! It is good approximation of sun, which is so distant, that its
		//! distance attenuation is hardly visible on our small scale.
		//! \param direction
		//!  Direction of light in world space.
		//! \param color
		//!  Irradiance at receiver, assuming it is oriented towards light.
		//! \param physicalScale
		//!  True for color in physical scale, false for color in sRGB (screen color).
		static RRLight* createDirectionalLight(const RRVec3& direction, const RRVec3& color, bool physicalScale);

		//! Creates omnidirectional point light with physically correct distance attenuation.
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		static RRLight* createPointLight(const RRVec3& position, const RRVec3& irradianceAtDistance1);

		//! Creates omnidirectional point light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  Light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-distance/radius),fallOffExponent).
		static RRLight* createPointLightRadiusExp(const RRVec3& position, const RRVec3& colorCustom, RRReal radius, RRReal fallOffExponent);

		//! Creates omnidirectional point light with polynom based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2).
		static RRLight* createPointLightPoly(const RRVec3& position, const RRVec3& colorCustom, RRVec3 polynom);

		//! Creates spot light with physically correct distance attenuation.
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  Light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-distance/radius),fallOffExponent).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLightRadiusExp(const RRVec3& position, const RRVec3& colorCustom, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with polynom based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/(polynom[0]+polynom[1]*distance+polynom[2]*distance^2).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Angle in radians. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Angle in radians. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLightPoly(const RRVec3& position, const RRVec3& colorCustom, RRVec3 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLights
	//! Set of lights with interface similar to std::vector.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving lights
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRLights : public RRVector<RRLight*>
	{
	public:
		virtual ~RRLights() {};
	};


} // namespace

#endif
