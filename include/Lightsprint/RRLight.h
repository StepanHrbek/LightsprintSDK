#ifndef RRLIGHT_H
#define RRLIGHT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRLight.h
//! \brief LightsprintCore | light
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2012
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRBuffer.h"
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
		//! Be cautious and fall back to createRgbScaler() in case you find regression.
		static RRScaler* createFastRgbScaler(RRReal power=0.45f);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Direct light source, directional/point/spot light with programmable function.
	//
	//! Standard point/spot/dir lights with physical, polynomial
	//! or exponential distance attenuations are supported via createXxx functions.
	//!
	//! All light properties are public and you are free to change them at any moment.
	//!
	//! Custom lights with this interface may be created.
	//! Offline solver in LightsprintCore accesses #type, #position, #direction and getIrradiance().
	//! Realtime renderer in LightsprintGL accesses all attributes.
	//!
	//! If you wish to control light properties for direct and indirect illumination separately,
	//! you can do so by changing properties at the right moment during calculation.
	//! For realtime GI, solver->calculate() accesses light to calculate indirect illumination,
	//! while solver->renderScene() accesses light to calculate direct illumination.
	//! For offline GI, solver->updateLightmaps(-1,-1,-1,NULL,indirectParams,) accesses light to calculate indirect illumination,
	//! while solver->updateLightmaps(,,,directParams,NULL,) accesses light to calculate direct illumination.
	//! There are other ways of controlling intensity of indirect illumination,
	//! see RRDynamicSolverGL::setDirectIlluminationBoost().
	//
	//! Thread safe: yes, may be accessed by any number of threads simultaneously.
	//! All custom implementations must be thread safe too.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLight : public RRUniformlyAllocatedNonCopyable
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
		//! Type of light source.
		Type type;

		//! Position of light source in world space. Relevant only for POINT and SPOT light.
		RRVec3 position;

		//! Normalized direction of light in world space. Relevant only for DIRECTIONAL and SPOT light.
		//! Be careful, setting unnormalized value would break lightmap building.
		RRVec3 direction;

		//! Outer cone angle in radians. Relevant only for SPOT light.
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
		//! Type of distance attenuation.
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

		//! Outer-inner cone angle in radians. Relevant only for SPOT light.
		//
		//! Light rays with direction diverted less than outerAngleRad from direction,
		//! but more than outerAngleRad-fallOffAngleRad, are attenuated.
		//! If your data contain innerAngle, set fallOffAngle=outerAngle-innerAngle.
		//! \n Valid range: (0,innerAngleRad>. Out of range values are clamped at construction time,
		//! changes you make later are accepted without checks.
		RRReal fallOffAngleRad;

		//! Enables/disables light.
		//
		//! There are other slower ways to enable/disable light:
		//! - Add/remove light from solver. But it takes longer to add/remove light than to flip flag.
		//! - Set light color to 0. But it does not speed up calculation/rendering, light is not optimized out.
		bool enabled;

		//! Whether light casts shadows.
		//
		//! Shadows are enabled by default, so light behaves realistically.
		//! You can disable shadows and break realism.
		//! But then, you should consider what is purpose of disabled shadows.
		//! Global illumination replaces fill lights, ambient lights and other fakes,
		//! you should remove such lights.
		bool castShadows;

		//! Enables workaround for error in game engines.
		//
		//! False = realistic lighting, default.
		//! True = copy error that is present in 99% of game engines,
		//! calculate direct Lambertian reflection in sRGB, make offline calculated
		//! lightmaps more similar to realtime lighting in game engines.
		//! Affects offline solver only, realtime solvers integrate with arbitrary direct
		//! lighting without need for extra settings.
		bool directLambertScaled;


		//////////////////////////////////////////////////////////////////////////////
		// Realtime render
		//////////////////////////////////////////////////////////////////////////////

		//! Projected texture.
		//
		//! Relevant only for realtime renderer (has rt prefix), only for type=SPOT.
		//! You may set/change it at any time, renderer updates automatically.
		//!
		//! Works as a replacement for spotlight parameters outerAngleRad, fallOffAngleRad, spotExponent. 
		//! When set, realtime spotlight is modulated only by texture.
		//!
		//! When you set texture, ownership is passed to light, light deletes texture in destructor.
		RRBuffer* rtProjectedTexture;

		//! How many shadowmaps realtime renderer should use. More = higher quality, slower.
		//
		//! Relevant only for realtime renderer (has rt prefix).
		//! You may set/change it at any time, renderer updates automatically.
		//! 1 or 2 is optimal for small scenes and for viewing scenes from distance where additional details that 3 provides are hardly visible.
		unsigned rtNumShadowmaps;
		//! How large (width=height) shadowmaps realtime renderer should use. More = higher quality, slower.
		//
		//! Relevant only for realtime renderer (has rt prefix).
		//! You may set/change it at any time, renderer updates automatically.
		//! Set higher resolution for hard and sharper shadows, lower for area and more blurry shadows.
		unsigned rtShadowmapSize;

		//////////////////////////////////////////////////////////////////////////////
		// Misc
		//////////////////////////////////////////////////////////////////////////////

		//! Optional name of light.
		RRString name;

		//! Custom data, initialized to NULL, never accessed by LightsprintCore again. 
		//
		//! Adapters may use it to store pointer to original structure that was
		//! adapted for Lightsprint.
		void* customData;

		//! Initialize light to defaults.
		RRLight();
		//! Destruct light.
		virtual ~RRLight() {delete rtProjectedTexture;}


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
		virtual RRVec3 getIrradiance(const RRVec3& receiverPosition, const RRScaler* scaler) const;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Returns true if member variables of both lights match (even if class differs).
		bool operator ==(const RRLight& a) const;
		bool operator !=(const RRLight& a) const;

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
