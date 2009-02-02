#ifndef RRLIGHT_H
#define RRLIGHT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRLight.h
//! \brief LightsprintCore | light
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
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
	//! RRScaler may be used to transform irradiance/emittance/exitance/reflectance/transmittance
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

		//! Converts irradiance/emittance/exitance from physical scale (W/m^2) to user defined scale (usually sRGB, screen colors).
		virtual void getCustomScale(RRReal& value) const;
		//! Converts irradiance/emittance/exitance from physical scale (W/m^2) to user defined scale (usually sRGB, screen colors).
		virtual void getCustomScale(RRVec3& value) const = 0;

		//! Converts irradiance/emittance/exitance from user defined scale (usually sRGB, screen colors) to physical scale (W/m^2).
		virtual void getPhysicalScale(RRReal& value) const;
		//! Converts irradiance/emittance/exitance from user defined scale (usually sRGB, screen colors) to physical scale (W/m^2).
		virtual void getPhysicalScale(RRVec3& value) const = 0;

		//! Converts reflectance/transmittance from physical scale to user defined scale.
		virtual void getCustomFactor(RRReal& value) const;
		//! Converts reflectance/transmittance from physical scale to user defined scale.
		virtual void getCustomFactor(RRVec3& value) const;

		//! Converts reflectance/transmittance from user defined scale to physical scale.
		virtual void getPhysicalFactor(RRReal& value) const;
		//! Converts reflectance/transmittance from user defined scale to physical scale.
		virtual void getPhysicalFactor(RRVec3& value) const;

		virtual ~RRScaler() {}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//
		// instance factory
		//

		//! Creates and returns scaler for sRGB space - screen colors.
		//
		//! Scaler converts between radiometry units (W/m^2) and screen colors.
		//! \param power
		//!  Exponent in formula screenSpace = physicalSpace^power.
		//!  Use default value for typical screens or tweak it for different contrast.
		static RRScaler* createRgbScaler(RRReal power=0.45f);

		//! As createRgbScaler(), but slightly faster, with undefined results for negative numbers.
		static RRScaler* createFastRgbScaler(RRReal power=0.45f);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Direct light source, directional/point/spot light with programmable function.
	//
	//! Standard point/spot/dir lights with physical, polynomial
	//! or exponential distance attenuations are supported via createXxx functions.
	//!
	//! Custom lights with this interface may be created.
	//! Offline renderer in LightsprintCore accesses #type, #position, #direction and getIrradiance().
	//! Realtime renderer in LightsprintGL accesses all attributes.
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
			DIRECTIONAL = 0,
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
		//! Be careful, setting unnormalized value would break lightmap building.
		RRVec3 direction;

		//! Outer cone angle in radians. Relevant only for SPOT light. Read/write.
		//
		//! Insight: This is half width of spot, FOV/2. \n
		//! Light rays go in directions up to outerAngleRad far from direction.
		//! \n Valid range: (0,pi/2). Out of range values are clamped at construction time,
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
			//! Intensity in custom scale is color/MAX(polynom[0]+polynom[1]*distance+polynom[2]*distance^2,polynom[3]). Used in fixed pipeline engines. polynom[3] was added to support Gamebryo.
			POLYNOMIAL,
			//! Intensity in physical scale is color*pow(MAX(0,1-(distance/radius)^2),fallOffExponent). Used in UE3.
			EXPONENTIAL,
		};
		//! Type of distance attenuation. Read only (lights from createXxx can't change type on the fly).
		DistanceAttenuationType distanceAttenuationType;

		//! Relevant only for distanceAttenuation==POLYNOMIAL.
		//! Distance attenuation in custom scale is computed as colorCustom/MAX(polynom[0]+polynom[1]*distance+polynom[2]*distance^2,polynom[3])
		RRVec4 polynom;

		//! Relevant only for distanceAttenuation==EXPONENTIAL.
		//! Distance attenuation in custom scale is computed as colorCustom*pow(MAX(0,1-(distance/radius)^2),fallOffExponent).
		RRReal fallOffExponent;

		//! Relevant only for type=SPOT.
		//! Exponent that controls attenuation from innerAngle to outerAngle in spotlight.
		RRReal spotExponent;

		//! Outer-inner code angle in radians. Relevant only for SPOT light. Read/write.
		//
		//! Light rays with direction diverted less than outerAngleRad from direction,
		//! but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//! If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		//! \n Valid range: (0,innerAngleRad>. Out of range values are clamped at construction time,
		//! changes you make later are accepted without checks.
		RRReal fallOffAngleRad;

		//! Whether light casts shadows.
		//
		//! Shadows are enabled by default, so light behaves realistically.
		//! You can disable shadows and break realism.
		//! But then, you should consider what is purpose of disabled shadows.
		//! Global illumination replaces fill lights, ambient lights and other fakes,
		//! you should remove such lights.
		bool castShadows;


		//////////////////////////////////////////////////////////////////////////////
		// Realtime render
		//////////////////////////////////////////////////////////////////////////////

		//! Filename of projected texture, with colors in custom scale.
		//
		//! Relevant only for realtime render, only for type=SPOT.
		//!
		//! Initialized to NULL, free()d in destructor.
		//! You may set/change it at any time.
		//!
		//! Works as a replacement for spotlight parameters outerAngleRad, fallOffAngleRad, spotExponent. 
		//! When set, realtime spotlight is modulated only by texture.
		char* rtProjectedTextureFilename;

		//! Limits area where shadows are computed.
		//
		//! Relevant only for realtime render, only for type=DIRECTIONAL.
		//!
		//! This is temporary control, it will be removed later.
		//! Default value is 1000, so area with shadows computed is 1000x1000 or smaller.
		float rtMaxShadowSize;


		//////////////////////////////////////////////////////////////////////////////
		// Misc
		//////////////////////////////////////////////////////////////////////////////

		//! For your private use, not accessed by Lightsprint. Initialized to NULL.
		void* customData;

		//! Destruct light.
		virtual ~RRLight() {}


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
		//!  Lights must return finite number, even when lighting model
		//!  predicts infinite number (some types of attenuation + zero distance).
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

		//! Creates omnidirectional point light without distance attenuation.
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		static RRLight* createPointLightNoAtt(const RRVec3& position, const RRVec3& irradianceAtDistance1);

		//! Creates omnidirectional point light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorPhysical
		//!  Color of light, irradiance in physical (linear) scale of lit surface before applying distance attenuation.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  Light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-(distance/radius)^2),fallOffExponent).
		static RRLight* createPointLightRadiusExp(const RRVec3& position, const RRVec3& colorPhysical, RRReal radius, RRReal fallOffExponent);

		//! Creates omnidirectional point light with polynom based distance attenuation (physically incorrect).
		//
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/MAX(polynom[0]+polynom[1]*distance+polynom[2]*distance^2,polynom[3]).
		static RRLight* createPointLightPoly(const RRVec3& position, const RRVec3& colorCustom, RRVec4 polynom);

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
		//!  Insight: This is half width of spot, FOV/2. \n
		//!  Angle in radians, (0,pi/2) range. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Insight: This is width of blurry part. \n
		//!  Angle in radians, (0,outerAngleRad> range.
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLight(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light without distance attenuation.
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param irradianceAtDistance1
		//!  Irradiance in physical scale at distance 1, assuming that receiver is oriented towards light.
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Insight: This is half width of spot, FOV/2. \n
		//!  Angle in radians, (0,pi/2) range. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Insight: This is width of blurry part. \n
		//!  Angle in radians, (0,outerAngleRad> range.
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLightNoAtt(const RRVec3& position, const RRVec3& irradianceAtDistance1, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with radius/exponent based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorPhysical
		//!  Light color, irradiance in physical (linear) scale of lit surface before applying distance attenuation.
		//! \param radius
		//!  Distance in world space, where light disappears due to its distance attenuation.
		//!  Light has effect in sphere of given radius.
		//! \param fallOffExponent
		//!  Distance attenuation in custom scale is computed as pow(MAX(0,1-(distance/radius)^2),fallOffExponent).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Insight: This is half width of spot, FOV/2. \n
		//!  Angle in radians, (0,pi/2) range. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Insight: This is width of blurry part. \n
		//!  Angle in radians, (0,outerAngleRad> range. 
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		static RRLight* createSpotLightRadiusExp(const RRVec3& position, const RRVec3& colorPhysical, RRReal radius, RRReal fallOffExponent, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad);

		//! Creates spot light with polynom based distance attenuation (physically incorrect).
		//
		//! Light rays start in position and go in directions up to outerAngleRad diverting from major direction.
		//! \param position
		//!  Position of light source in world space, start of all light rays.
		//! \param colorCustom
		//!  Irradiance in custom scale (usually screen color) of lit surface before applying distance attenuation.
		//! \param polynom
		//!  Distance attenuation in custom scale is computed as 1/MAX(polynom[0]+polynom[1]*distance+polynom[2]*distance^2,polynom[3]).
		//! \param majorDirection
		//!  Major direction of light in world space.
		//! \param outerAngleRad
		//!  Insight: This is half width of spot, FOV/2. \n
		//!  Angle in radians, (0,pi/2) range. Light rays go in directions up to outerAngleRad far from major majorDirection.
		//! \param fallOffAngleRad
		//!  Insight: This is width of blurry part. \n
		//!  Angle in radians, (0,outerAngleRad> range.
		//!  Light rays with direction diverted less than outerAngleRad from majorDirection,
		//!  but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//!  If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		//! \param spotExponent
		//!  Insight: This is how intensity changes in blurry part. Default 1 makes it linear. \n
		//!  Exponent in (0,inf) range. \n
		//!  Changes attenuaton from linear with 0 in outerAngle and 1 in innerAngle to exponential: linearAttenuation^spotExponent.
		static RRLight* createSpotLightPoly(const RRVec3& position, const RRVec3& colorCustom, RRVec4 polynom, const RRVec3& majorDirection, RRReal outerAngleRad, RRReal fallOffAngleRad, RRReal spotExponent);

		//! Creates mutable light.
		//
		//! You can turn mutable light into any light type with any attenuation model,
		//! it has getIrradiance() supporting all types and attenuations.
		//! Mutability doesn't make any difference in realtime solver.
		//! In offline solver, lights created by other createXxx() are slightly faster because they 
		//! have light type and attenuation hardcoded.
		static RRLight* createMutableLight();
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
