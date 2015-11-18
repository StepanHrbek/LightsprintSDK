#ifndef RRMATERIAL_H
#define RRMATERIAL_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRMaterial.h
//! \brief LightsprintCore | material
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2014
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
		bool operator ==(const RRRadiometricMeasure& a) const;
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
		bool operator ==(const RRSideBits& a) const;
	};

	//! Description of material properties of a surface.
	//
	//! It is a set of common material properties relevant for global illumination solver.
	//! It is not necessarily complete material description,
	//! custom renderers can use additional custom information stored elsewhere.
	//!
	//! Textures are owned and deleted by material.
	//! To change texture on the fly, delete old one before assigning new one.
	struct RR_API RRMaterial : public RRUniformlyAllocatedNonCopyable
	{
		//! What to do with completely uniform textures (single color).
		enum UniformTextureAction
		{
			UTA_KEEP,   ///< Keep uniform texture.
			UTA_DELETE, ///< Delete uniform texture.
			UTA_NULL    ///< NULL pointer to uniform texture, but don't delete it.
		};

		//! Part of material description.
		struct RR_API Property
		{
			//! Material property expressed as 3 floats. If texture is present, this is average color of the texture.
			//
			//! It is color in custom (usually sRGB) scale. Used by importers, exporters and realtime renderes (except for our pathtracer).
			//! We don't enforce any kind of color validation, so you can create unrealistic materials.
			RRVec3                 color;
			//! Color converted to physical (linear) scale and validated. Used by GI solvers and our pathtracer.
			//
			//! Solver needs it filled, but usually it happens automatically.
			//! To keep colorPhysical synchronized with color, solver calls RRObjects::updateColorPhysical() on incoming objects.
			//! Only if you modify color already passed to solver, you need to call RRMaterial::convertToPhysicalScale() to update colorPhysical.
			RRVec3                 colorPhysical;
			//! Material property expressed as a texture or video.
			//
			//! Texture is owned and deleted by RRMaterial, so in order to change texture,
			//! delete old one before assigning new one.
			//! Assignment and copy constructor in Property make only shallow copy, ~Property() doesn't delete texture.
			//! Assignment and copy constructor in RRMaterial are disabled, ~RRMaterial() deletes textures.
			RRBuffer*              texture;
			//! Texcoord channel used by texture. Call RRMesh::getTriangleMapping(texcoord) to get mapping for texture.
			unsigned               texcoord;

			//! Clears property to default zeroes.
			Property()
			{
				color = RRVec3(0);
				colorPhysical = RRVec3(0);
				texture = NULL;
				texcoord = 0;
			}
			//! Changes property to property*multiplier+addend.
			//! Both color and texture are changed. 8bit texture may be changed to floats to avoid clamping.
			void multiplyAdd(RRVec4 multiplier, RRVec4 addend);
			//! If texture exists, updates color to average color in texture and returns standard deviation of color in texture.
			RRReal updateColorFromTexture(const RRScaler* scaler, bool isTransmittanceInAlpha, UniformTextureAction uniformTextureAction, bool updateEvenFromStub);
			//! If texture does not exist, creates 1x1 stub texture from color. Returns number of textures created, 0 or 1.
			unsigned createTextureFromColor(bool isTransmittance);
			//! Returns true if both properties are identical (including using the same texture).
			bool operator ==(const RRMaterial::Property& a) const;
		};

		//! Default constructor, initializes only pointers in material, call reset() to initialize the rest of data.
		RRMaterial();
		//! Copies given material to this material.
		//
		//! Don't call it from multiple threads at the same time,
		//! it is thread unsafe under very rare circumstances
		//! (that's why we didn't make it "operator =", people expect safety in assignment).
		void copyFrom(const RRMaterial& from);

		//! Returns true if both materials look the same (and use the same textures).
		bool operator ==(const RRMaterial& a) const;

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
		//!
		//! Sidedness of the most common materials:
		//!  - glass sphere or any other closed object should be 1-sided, light refracts differently when front and back sides are hit
		//!  - glass window should be made of single 2-sided face, we simulate multiple bounces between virtual front and back sides, light refracts identically from both sides
		//!
		//! Illumination produced by updateLightmap[s]() for faces with 2-sided material depends on solver.
		//! Realtime solvers sum illumination of both sides, offline solver outputs front side illumination.
		//! (Changing offline solver to work like realtime one would probably create more
		//! confusion than it would fix, it's not clear how directional lightmaps would look etc.)
		void          reset(bool twoSided);

		//! Gathers information from textures, updates color for all Properties with texture. Updates also minimalQualityForPointMaterials.
		//
		//! \param scaler
		//!  Textures are expected in custom scale of this scaler.
		//!  Average colors are computed in the same scale.
		//!  Without scaler, computed averages may slightly differ from physically correct averages.
		//! \param uniformTextureAction
		//!  What to do with textures of constant color. Removing them may make rendering/calculations faster.
		//! \param updateEvenFromStubs
		//!  True=updates color even if texture is a stub. Pass true if you don't know color; stub texture is also wrong, but at least they will match.
		//!  Pass false if you know color and don't want it to be overwritten by stub.
		void          updateColorsFromTextures(const RRScaler* scaler, UniformTextureAction uniformTextureAction, bool updateEvenFromStubs);
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
		//! If you know what user prefers, set or clear specularTransmittanceKeyed yourself instead of calling this function.
		void          updateKeyingFromTransmittance();
		//! Updates sideBits, clears bits with relevant color black. This may make rendering faster.
		void          updateSideBitsFromColors();
		//! Updates bumpMapTypeHeight, tries to guess what type it is by looking at contents of bumpMap.texture.
		void          updateBumpMapType();

		//! Changes material's colorPhysical values to closest physically valid ones. Returns whether changes were made.
		//
		//! In physical scale, diffuse+specular+transmission must be below 1 (real world materials are below 0.98)
		//! and this function enforces it. We call it automatically from solver.
		//! In custom scale, real world materials have diffuse+specular+transmission higher (up to roughly 1.7),
		//! but we don't enforce this at all, color stays unchanged.
		bool          validate(RRReal redistributedPhotonsLimit=0.98f);

		//! Converts material properties from physical to custom scale (colorPhysical -> color).
		void          convertToCustomScale(const RRScaler* scaler);
		//! Converts material properties from custom to physical scale (color -> colorPhysical).
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
		//
		//! When texture is set, its color is used as a specular reflectance color, and its alpha is used to modulate material's specularShininess/roughness.
		//! For alpha=1 there is no change in shininess/roughness. As alpha goes down to 0, shininess/roughness also goes down to its minimal value (1 or 0).
		//! Whether it is rough or shiny depends on specularModel. So in case of need, you can invert effect of alpha channel by changing specularModel.
		Property      specularReflectance;
		enum SpecularModel
		{
			PHONG                  = 0, ///< as in http://en.wikipedia.org/wiki/Phong_shading (shininess in 1..inf)
			BLINN_PHONG            = 1, ///< as in http://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_shading_model (shininess in 1..inf)
			TORRANCE_SPARROW       = 2, ///< as in Zara J.: Pocitacova grafika (1992) (roughness in 0..1)
			BLINN_TORRANCE_SPARROW = 3, ///< as in http://www.siggraph.org/education/materials/HyperGraph/illumin/specular_highlights/blinn_model_for_specular_reflect_1.htm (roughness in 0..1)
		};
		//! Selects what model / distribution function to use for specular reflectance.
		SpecularModel specularModel;
		//! Interpreted as shininess (0..inf) or roughness (0..1), according to specularModel.
		RRReal        specularShininess;
		//! Fraction of energy that continues through surface (with direction possibly changed by refraction).
		//
		//! Whether light gets through translucent object, e.g. sphere, it depends on material sphere is made of
		//! - standard 2sided material -> ray gets through interacting with both front and back faces, creating correct caustics
		//! - standard 1sided material -> ray is deleted when it hits back side of 1sided face
		//! - 1sided material with sideBits[1].catchFrom=1 -> ray gets through, interacting with front face, skipping back face, creating incorrect caustics
		//!
		//! Note that higher transmittance does not decrease reflectance and emittance, they are independent properties.
		//! There's single exception to this rule: higher transmittance decreases diffuseReflectance.texture (we multiply rgb from diffuse texture by opacity on the fly).
		//!
		//! Material with transparency stored in alpha of diffuse texture is initialized like \code
		//!	diffuseReflectance.texture = RRBuffer::load(foo);
		//!	specularTransmittance.texture = diffuseReflectance.texture->createReference();
		//!	specularTransmittanceInAlpha = true;
		//! \endcode
		Property      specularTransmittance;
		//! Whether specular transmittance is in specularTransmittance.texture's Alpha (0=transparent) or in RGB (1,1,1=transparent). It is irrelevant when specularTransmittance.texture==NULL.
		bool          specularTransmittanceInAlpha;
		//! Whether 1-bit alpha keying is used instead of smooth alpha blending in realtime render. 1-bit alpha keying is faster but removes semi-translucency. Smooth alpha blend renders semi-translucency, but artifacts appear on meshes where semi-translucent faces overlap.
		bool          specularTransmittanceKeyed;
		//! If specularTransmittanceKeyed, transmittance in 0..1 range is tested against this threshold. Values above threshold are considered fully transparent, values below threshold are considered fully opaque.
		RRReal        specularTransmittanceThreshold;
		//! Whether specular transmittance map is inverted. True = values read from map should be inverted before use. This inversion is implemented in getPointMaterial() and in shaders.
		bool          specularTransmittanceMapInverted;
		//! For 1-sided faces, it is refractive index of matter behind surface divided by refractive index of matter in front of surface
		//! (1-sided surfaces are treated as volume boundaries, this index tells what happens when light leaves matter in front of boundary and enters matter behind).
		//! For 2-sided faces, it is refractive index of matter inside thin layer divided by refractive index of matter around
		//! (2-sided surfaces are treated as thin layers made of different matter, renderer accounts for multiple front/back interreflection).
		//! Real world surfaces have index from 0.25 to 4. <a href="http://en.wikipedia.org/wiki/List_of_indices_of_refraction">Examples.</a>
		RRReal        refractionIndex;
		//! Optional bump map modulates surface normals.
		//
		//! RGB maps are interpreted as normal maps, grayscale maps (or the same map as diffuseReflectance.texture, or c\@pture) as heightmaps.
		//! bumpMap.color.x is used as a multiplier of normal steepness, y multiplies height in parallax mapping, defaults are 1, negative values are legal.
		Property      bumpMap;
		//! True = bump map is height/displacement map, false = bump map is normal map.
		bool          bumpMapTypeHeight;
		//! Texcoord channel with unwrap for lightmaps. To be used in RRMesh::getTriangleMapping().
		//
		//! Note that for proper lighting, unwrap must have all coordinates in <0..1> range and triangles must not overlap.
		//! Unwrap may be imported or automatically generated by RRObjects::buildUnwrap().
		unsigned      lightmapTexcoord;
		//! Hint/optimization for offline solver, material tells solver to use RRObject::getPointMaterial()
		//! if desired lighting quality is equal or higher than this number.
		//! Inited to UINT_MAX (=never use point materials), automatically adjusted by updateColorsFromTextures().
		//!
		//! Warning: If you don't call updateColorsFromTextures(), make sure you adjust minimalQualityForPointMaterials on per-material basis.
		//! Keeping UINT_MAX would make keyed objects cast solid shadows to lightmaps.
		//! Setting always 0 would make lightmap build very slow.
		//!
		//! Not used by realtime solver; you don't have to update it each time your realtime app changes texture.
		unsigned      minimalQualityForPointMaterials;
		//! Optional name of material.
		//
		//! If name contains "water" and static normal map is set, realtime renderer animates mapping
		//! to simulate flow of waves.
		RRString      name;
		//! Optional image of material, for use e.g. by material library. It is owned by material, deleted in dtor. Not saved to .rrmaterial.
		RRBuffer*     preview;

		//! Deletes textures (yes, textures are owned by RRMaterial).
		~RRMaterial();


		//////////////////////////////////////////////////////////////////////////////
		// Pathtracing queries
		//////////////////////////////////////////////////////////////////////////////

		enum BrdfType
		{
			BRDF_NONE     = 0,
			BRDF_DIFFUSE  = 1,
			BRDF_SPECULAR = 2,
			BRDF_TRANSMIT = 4,
			BRDF_ALL      = 7,
			NUM_BRDFS     = 3,
		};

		struct Response
		{
			RRVec3 dirIn;     ///< normalized direction of incoming light
			RRVec3 dirNormal; ///< normalized surface normal
			RRVec3 dirOut;    ///< normalized direction of exiting light
			RRVec3 colorOut;  ///< color of leaving light (incoming light is 1)
			RRReal pdf;       ///< probability density function
			BrdfType brdfType;///< type of response
		};

		//! Calculates color of light exiting surface in response to incoming white light (dirIn, dirNormal, dirOut -> colorOut)
		void getResponse(Response& response, BrdfType type = BRDF_ALL) const;

		//! Calculates direction and color of light exiting surface in response to incoming white light (dirNormal, dirOut -> dirIn, colorOut, pdf, brdfType
		void sampleResponse(Response& response, const RRVec3& randomness, BrdfType type = BRDF_ALL) const;


		//////////////////////////////////////////////////////////////////////////////
		// Loaders/Savers
		//////////////////////////////////////////////////////////////////////////////

		//! Loads first material from file, returns true on success.
		bool load(const RRString& filename, RRFileLocator* textureLocator);
		//! Saves material to file, returns true on success.
		bool save(const RRString& filename) const;
	};

	//! RRMaterial optimized for use in RRObject::getPointMaterial(), not for general use.
	//
	//! RRPointMaterial creates and destructs faster than RRMaterial, because it does not own textures and name,
	//! it's just shallow copy for immediate consumption. So never manipulate textures and name in point materials.
	struct RR_API RRPointMaterial : public RRMaterial
	{
		//! Fast and thread safe copy. getPointMaterial() implementations use it to copy triangle material to point material.
		void operator =(const RRMaterial& a);
		//! Makes it possible to store pointmaterials in vector.
		RRPointMaterial& operator =(const RRPointMaterial& a);
		//! Unlike RRMaterial, RRPointMaterial does not delete textures and name.
		~RRPointMaterial();

	private:
		// Ensures that respective RRMaterial functions are not called.
		void copyFrom(const RRMaterial& from) {}
		void updateColorsFromTextures(const RRScaler* scaler, UniformTextureAction uniformTextureAction, bool updateEvenFromStubs) {}
		unsigned createTexturesFromColors() {return 0;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Material library, collection of materials.
	//
	//! Material collection that can be loaded or saved to disk.
	//! Individual materials are not owned by collection, they are not deleted in destructor.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRMaterials : public RRVector<RRMaterial*>
	{
	public:
#ifdef RR_SUPPORTS_RVALUE_REFERENCES
		// Inherits move ctor and operator.
		using RRVector::RRVector;
		using RRVector::operator=;
#endif

		//////////////////////////////////////////////////////////////////////////////
		// Loaders/Savers
		//////////////////////////////////////////////////////////////////////////////

		//! Loads materials from file, returns true on success.
		static RRMaterials* load(const RRString& filename, RRFileLocator* textureLocator);
		//! Saves materials to file, returns true on success.
		bool save(const RRString& filename) const;

		//! Template of custom material loader.
		typedef RRMaterials* Loader(const RRString& filename, RRFileLocator* textureLocator, bool* aborting);
		//! Template of custom material saver.
		typedef bool Saver(const RRMaterials* materials, const RRString& filename);

		//! Registers material loader so it can be used by RRMaterial::load().
		//
		//! Extensions are case insensitive, in "*.rrmaterial;*.mtl" format.
		//!
		//! Several loaders are implemented in LightsprintIO library,
		//! rr_io::registerLoaders() will register all of them for you (by calling this function several times).
		//!
		//! Multiple loaders may be registered, even for the same extension.
		//! If first loader fails to load scene, second one is tried etc.
		static void registerLoader(const char* extensions, Loader* loader);
		//! Similar to registerLoader().
		static void registerSaver(const char* extensions, Saver* saver);
		//! Returns list of supported loader extensions in "*.rrmaterial;*.mtl" format.
		//
		//! All extensions of registered loaders are returned in one static string, don't free() it.
		//! NULL is returned if no loaders were registered.
		static const char* getSupportedLoaderExtensions();
		//! Returns list of supported saver extensions in "*.rrmaterial;*.mtl" format.
		//
		//! All extensions of registered savers are returned in one static string, don't free() it.
		//! NULL is returned if no savers were registered.
		static const char* getSupportedSaverExtensions();
	};

} // namespace

#endif
