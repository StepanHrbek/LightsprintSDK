#ifndef RRMATERIAL_H
#define RRMATERIAL_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMaterial.h
//! \brief LightsprintCore | material
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRLight.h" // RRScaler
#include "RRBuffer.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// material aspects of space and surfaces
	//
	// RRVec3              - rgb color
	// RRRadiometricMeasure - radiometric measure
	// RRSideBits           - 1bit attributes of one side
	// RRMaterial           - material properties of a surface
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Specification of radiometric measure; what is measured and what units are used.
	struct RRRadiometricMeasure
	{
		RRRadiometricMeasure()
			: exiting(0), scaled(0), flux(0), direct(0), indirect(1), smoothed(1) {};
		RRRadiometricMeasure(bool aexiting, bool ascaled, bool aflux, bool adirect, bool aindirect)
			: exiting(aexiting), scaled(ascaled), flux(aflux), direct(adirect), indirect(aindirect), smoothed(1) {};
		bool exiting : 1; ///< Selects between [0] incoming radiation (does not include emittance) and [1] exiting radiation (includes emittance). \n Typical setting: 0.
		bool scaled  : 1; ///< Selects between [0] physical scale (W) and [1] custom scale provided by RRScaler. \n Typical setting: 1.
		bool flux    : 1; ///< Selects between [0] radiant intensity (W/m^2) and [1] radiant flux (W). \n Typical setting: 0.
		bool direct  : 1; ///< Makes direct radiation and emittance (your inputs) part of result. \n Typical setting: 0.
		bool indirect: 1; ///< Makes indirect radiation (computed) part of result. \n Typical setting: 1.
		bool smoothed: 1; ///< Selects between [0] raw results for debugging purposes and [1] smoothed results.
	};
	// Shortcuts for typical measures.
	#define RM_IRRADIANCE_PHYSICAL_INDIRECT rr::RRRadiometricMeasure(0,0,0,0,1)
	#define RM_IRRADIANCE_CUSTOM_INDIRECT   rr::RRRadiometricMeasure(0,1,0,0,1)
	#define RM_IRRADIANCE_PHYSICAL          rr::RRRadiometricMeasure(0,0,0,1,1)
	#define RM_IRRADIANCE_CUSTOM            rr::RRRadiometricMeasure(0,1,0,1,1)
	#define RM_EXITANCE_PHYSICAL            rr::RRRadiometricMeasure(1,0,0,1,1)
	#define RM_EXITANCE_CUSTOM              rr::RRRadiometricMeasure(1,1,0,1,1)

	//! Boolean attributes of one side of a surface. Usually exist in array of two elements, for front and back side.
	struct RRSideBits
	{
		unsigned char renderFrom:1;  ///< 1=this side of surface is visible. Information only for renderer, not for solver.
		unsigned char emitTo:1;      ///< 1=this side of surface emits energy according to diffuseEmittance and diffuseReflectance. If both sides emit, 50% to each side is emitted.
		unsigned char catchFrom:1;   ///< 1=surface catches photons coming from this side. Next life of catched photon depends on receiveFrom/reflect/transmitFrom/legal. For transparent pixels in alpha-keyed textures, simply disable catchFrom, light will come through without regard to other flags.
		unsigned char legal:1;       ///< 0=catched photons are considered harmful, their presence is masked away. It is usually used for back sides of solid 1sided faces.
		unsigned char receiveFrom:1; ///< 1=catched photons are reflected according to diffuseReflectance. Reflected photon splits and leaves to all sides with emitTo.
		unsigned char reflect:1;     ///< 1=catched photons are reflected according to specularReflectance. Reflected photon leaves to the same side.
		unsigned char transmitFrom:1;///< 1=catched photons are transmitted according to specularTransmittance and refractionIndex. Transmitted photon leaves to other side.
	};

	//! Description of material properties of a surface.
	//
	//! It is minimal set of properties needed by global illumination solver,
	//! so it not necessarily complete material description,
	//! custom renderer may use additional custom information stored elsewhere.
	//!
	//! Values may be in physical or any other scale, depends on context, who uses it.
	//! - Adapters create materials in custom scale (usually sRGB, so material properties are screen colors).
	//! - Realtime renderer uses custom scale materials.
	//!   Renderer expects, that custom scale is sRGB (physical->sRGB conversion is offloaded from CPU to GPU).
	//!   We don't enforce validation, so you can safely create and render unrealistic materials.
	//! - GI solver internally creates material copies converted to physical scale and validated,
	//!   you can access them via RRDynamicSolver::getMultiObjectPhysical().
	struct RR_API RRMaterial : public RRUniformlyAllocatedNonCopyable
	{
		//! What to do with completely uniform textures.
		enum UniformTextureAction
		{
			UTA_KEEP,   ///< Keep uniform texture.
			UTA_DELETE, ///< Delete uniform texture.
			UTA_NULL    ///< NULL pointer to uniform texture, but don't delete it.
		};

		//! Part of material description.
		struct RR_API Property
		{
			RRVec3                 color;    ///< Material property expressed as 3 floats. If texture is present, this is average color of texture.
			RRBuffer*              texture;  ///< Material property expressed as a texture. Not deleted in destructor. Shallow copied in assignment operator and copy constructor.
			unsigned               texcoord; ///< Texcoord channel used by texture. Call RRMesh::getTriangleMapping(texcoord) to get mapping for texture.

			//! Clears property to default zeroes.
			Property()
			{
				color = RRVec3(0);
				texture = NULL;
				texcoord = 0;
			}
			//! Changes property to property*multiplier+addend.
			//! Both color and texture are changed. 8bit texture may be changed to floats to avoid clamping.
			void multiplyAdd(RRVec4 multiplier, RRVec4 addend);
			//! If texture exists, updates color to average color in texture and returns standard deviation of color in texture.
			RRReal updateColorFromTexture(const RRScaler* scaler, bool isTransmittanceInAlpha, UniformTextureAction uniformTextureAction);
			//! If texture does not exist, creates 1x1 stub texture from color. Returns number of textures created, 0 or 1.
			unsigned createTextureFromColor(bool isTransmittance);
		};

		//! Resets material to fully diffuse gray (50% reflected, 50% absorbed).
		//
		//! Also behaviour of both front and back side (sideBits) is reset to defaults.
		//! - Back side of 1-sided material is not rendered and it does not emit or reflect light.
		//! - Back side of 1-sided material stops light:
		//!   Imagine interior with 1-sided walls. Skylight should not get in through back sides od walls.
		//!   Photons that hit back side are removed from simulation.
		//! - However, back side of 1-sided material allows transmittance and refraction:
		//!   Imagine previous example with alpha-keyed window in 1-sided wall.
		//!   Skylight should get through window in both directions.
		//!   It gets through according to material transmittance and refraction index.
		//! - Back side of 1-sided material does not emit or reflect light.
		//! - Sides of 2-sided material behave nearly identically, only back side doesn't have diffuse reflection.
		void          reset(bool twoSided);

		//! Gathers information from textures, updates color for all Properties with texture. Updates also minimalQualityForPointMaterials.
		//
		//! \param scaler
		//!  Textures are expected in custom scale of this scaler.
		//!  Average colors are computed in the same scale.
		//!  Without scaler, computed averages may slightly differ from physically correct averages.
		//! \param uniformTextureAction
		//!  What to do with textures of constant color. Removing them may make rendering/calculations faster.
		void          updateColorsFromTextures(const RRScaler* scaler, UniformTextureAction uniformTextureAction);
		//! Creates stub 1x1 textures for properties without texture.
		//
		//! LightsprintCore fully supports materials without textures, and working with flat colors instead of textures is faster.
		//! But in case of need, this function would create 1x1 textures out of material colors.
		//! Don't call it unless you know what you are doing, risk of reduced performance.
		//! \return Number of textures created.
		unsigned      createTexturesFromColors();
		//! Updates specularTransmittanceKeyed.
		//
		//! Looks at specularTransmittance and tries to guess what user expects when realtime rendering, 1-bit keying or smooth blending.
		//! If you know what user prefers, set it instead of calling this function.
		void          updateKeyingFromTransmittance();
		//! Updates sideBits, clears bits with relevant color black. This may make rendering faster.
		void          updateSideBitsFromColors();

		//! Changes material to closest physically valid values. Returns whether changes were made.
		//
		//! In physical scale, diffuse+specular+transmission must be below 1 (real world materials are below 0.98)
		//! and this function enforces it. We call it automatically from solver.
		//! In custom scale, it's legal to have diffuse+specular+transmission higher (up to roughly 1.7),
		//! so we don't call validate on custom scale materials.
		bool          validate(RRReal redistributedPhotonsLimit=0.98f);

		//! Converts material properties from physical to custom scale.
		void          convertToCustomScale(const RRScaler* scaler);
		//! Converts material properties from custom to physical scale.
		void          convertToPhysicalScale(const RRScaler* scaler);

		//! True if renderer needs blending to render the material.
		bool          needsBlending() const;

		//! Defines material behaviour for front (sideBits[0]) and back (sideBits[1]) side.
		RRSideBits    sideBits[2];
		//! Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Diffuse_reflection">diffuse reflection</a> (each channel separately).
		Property      diffuseReflectance;
		//! Radiant emittance in watts per square meter (each channel separately). (Adapters usually create materials in sRGB scale, so that this is screen color.)
		Property      diffuseEmittance;
		//! Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Specular_reflection">specular reflection</a> (each channel separately).
		Property      specularReflectance;
		//! Fraction of energy that continues through surface (with direction possibly changed by refraction).
		//
		//! Whether light gets through translucent object, e.g. sphere, it depends on material sphere is made of
		//! - standard 2sided material -> ray gets through interacting with both front and back faces, creating correct caustics
		//! - standard 1sided material -> ray is deleted when it hits back side of 1sided face
		//! - 1sided material with sideBits[1].catchFrom=1 -> ray gets through, interacting with front face, skipping back face, creating incorrect caustics
		//!
		//! Note that higher transmittance does not decrease reflectance and emittance, they are independent properties.
		//! There's single exception to this rule: higher transmittance decreases diffuseReflectance.texture (we multiply rgb from diffuse texture by opacity on the fly).
		Property      specularTransmittance;
		//! Whether specular transmittance is in specularTransmittance.texture's Alpha (0=transparent) or in RGB (1,1,1=transparent). It is irrelevant when specularTransmittance.texture==NULL.
		bool          specularTransmittanceInAlpha;
		//! Whether 1-bit alpha keying is used instead of smooth alpha blending in realtime render. 1-bit alpha keying is faster but removes semi-translucency. Smooth alpha blend renders semi-translucency, but artifacts appear on meshes where semi-translucent faces overlap.
		bool          specularTransmittanceKeyed;
		//! Refractive index of matter in front of surface divided by refractive index of matter behind surface. <a href="http://en.wikipedia.org/wiki/List_of_indices_of_refraction">Examples.</a>
		RRReal        refractionIndex;
		//! Texcoord channel with unwrap for lightmaps. To be used in RRMesh::getTriangleMapping().
		unsigned      lightmapTexcoord;
		//! Hint for solver, material tells solver to use RRObject::getPointMaterial()
		//! if desired lighting quality is equal or higher than this number.
		//! Inited to UINT_MAX (=never use point materials), automatically adjusted by updateColorsFromTextures().
		//!
		//! Warning: If you don't call updateColorsFromTextures(), make sure you adjust minimalQualityForPointMaterials on per-material basis.
		//! Keeping UINT_MAX would make keyed objects cast solid shadows to lightmaps.
		//! Setting always 0 would make lightmap build very slow.
		unsigned      minimalQualityForPointMaterials;
		//! Optional name of material.
		RRString      name;
	};

} // namespace

#endif
