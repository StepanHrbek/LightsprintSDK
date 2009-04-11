#ifndef RROBJECT_H
#define RROBJECT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRObject.h
//! \brief LightsprintCore | 3d object with geometry, materials, position etc
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRCollider.h"
#include "RRLight.h" // RRScaler
#include "RRIllumination.h" // RRIlluminatedObject

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
		bool exiting : 1; ///< Selects between [0] incoming radiation+diffuseEmittance/diffuseReflectance and [1] exiting radiation. \n Typical setting: 0.
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
	struct RR_API RRMaterial
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
			class RRBuffer*        texture;  ///< Material property expressed as a texture. Not deleted in destructor. Shallow copied in assignment operator and copy constructor.
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
		//! if desired lighting quality is equal or higher to this number.
		//! Inited to UINT_MAX (=never use point materials), automatically adjusted by updateColorsFromTextures().
		unsigned      minimalQualityForPointMaterials;
		//! Optional name of material, may be NULL. Not freed/deleted in destructor. Shallow copied in assignment operator and copy constructor.
		const char*   name;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObject
	//! Common interface for all proprietary object formats.
	//
	//! \section s1 Provided information
	//! %RRObject provides information about
	//! - object material properties
	//! - object collider for fast ray-mesh intersections
	//! - indirectly also object geometry (via getCollider()->getMesh())
	//! - optionally object transformation matrix
	//! - optionally unwrap (for future versions)
	//!
	//! \section s2 Creating instances
	//! The only way to create first instance is to derive from %RRObject.
	//! This is caused by lack of standard material description formats,
	//! everyone uses different description and needs his own object importer.
	//!
	//! Once you have object importers, there is built-in support for 
	//! - pretransforming mesh, see createWorldSpaceMesh()
	//! - baking multiple objects together, see createMultiObject()
	//!
	//! \section s3 Links between objects
	//! solver -> RRObject -> RRCollider -> RRMesh
	//! \n where A -> B means that
	//!  - A has pointer to B
	//!  - there is no automatic reference counting in B and no automatic destruction of B from A
	//! \n This means you may have multiple objects sharing one collider and mesh.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObject : public RRUniformlyAllocatedNonCopyable
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRObject() {}


		//
		// must not change during object lifetime
		//

		//! Returns collider of underlying mesh. It is also access to mesh itself (via getCollider()->getMesh()).
		//! Must always return valid collider, implementation is not allowed to return NULL.
		virtual const RRCollider* getCollider() const = 0;

		//! Returns average material description for given triangle.
		//
		//! Although optional per-pixel material queries may be implemented in getPointMaterial(),
		//! it's mandatory to implement basic getTriangleMaterial() with average values.
		//! Returned pointer must stay valid and constant for whole life of object.
		//! \param t
		//!  Triangle number.
		//! \param light
		//!  NULL or one of lights in scene.
		//!  With light==NULL, returned material must be always the same non-NULL.
		//!  With light!=NULL, it is allowed to return NULL to disable lighting or shadowing, see receiver for details.
		//! \param receiver
		//!  NULL or one of static objects in scene.
		//!  Used only when light!=NULL, controls properties of given light.
		//!  When receiver==NULL, you may return NULL to make triangle invisible for given light (disables both direct lighting and shadow-casting).
		//!  When receiver!=NULL, you may return NULL to disable direct shadow casting of triangle for given light and receiver.
		virtual const RRMaterial* getTriangleMaterial(unsigned t, const class RRLight* light, const RRObject* receiver) const = 0;

		//! Returns material description for point on object's surface.
		//
		//! Use it to query material properties for any given pixel.
		//! This is higher quality but slower per-pixel version of faster per-triangle getTriangleMaterial().
		//! \n\n Default implementation takes point details from optional textures in material returned by getTriangleMaterial().
		//! \n\n Offline GI solver uses getPointMaterial() only if requested lightmap quality>=getTriangleMaterial()->minimalQualityForPointMaterials.
		//! Realtime GI solvers never call getPointMaterial().
		//! \param t
		//!  Triangle number.
		//! \param uv
		//!  2D coordinates of point, in triangle's space. Triangle vertices are in 0,0 1,0 0,1.
		//! \param out
		//!  For default scaler=NULL, out is undefined on input, function fills it with material.
		//!  For special case of scaler!=NULL, see explanation in scaler.
		//! \param scaler
		//!  You are expected to keep it NULL, only solver calls it with scaler.
		//!  When called normally without scaler, function goal is to fill out in adapter's native(custom) scale.
		//!  When called with scaler, out is already filled with per-triangle material in physical scale
		//!  (colors in physical scale, textures are in adapter's default scale to save memory),
		//!  function's goal is to modify colors in physical scale. Default implementation
		//!  reads custom scale colors from textures and converts them to physical scale using scaler.
		virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out, const RRScaler* scaler = NULL) const;

		//! Information about single object, what LOD it is.
		struct LodInfo
		{
			//! Two objects are LODs of the same model only if they have identical bases.
			const void* base;
			//! LODs of the same model always differ in level. There must be always at least level 0 for each base.
			unsigned level;
			//! Object is rendered only if its distance satisfies distanceMin<=distance<distanceMax.
			RRReal distanceMin;
			//! Object is rendered only if its distance satisfies distanceMin<=distance<distanceMax.
			RRReal distanceMax;
		};
		//! Returns information about single object, what LOD it is.
		//
		//! In Lightsprint, LODs are completely separated objects without any pointers linking them.
		//! The only information that connects them comes from this function.
		//! \n Default implementation makes all objects unique, they return different bases and level 0.
		//! \param t
		//!  Triangle number, relevant only for multiobjects, individual triangles in multiobject may return
		//!  different results.
		//! \param out
		//!  Caller provided storage for result.
		//!  For valid t, requested LOD info is written to out. For invalid t, out stays unmodified.
		virtual void getTriangleLod(unsigned t, LodInfo& out) const;


		//
		// may change during object lifetime
		//

		//! Returns object transformation from local to world space.
		//
		//! Default implementation always returns NULL, which means no transformation.
		//! \return Pointer to matrix that transforms object space to world space.
		//!  May return NULL for identity/no transformation. 
		//!  Pointer must be constant and stay valid for whole life of object.
		//!  Matrix may change during object life.
		virtual const RRMatrix3x4*  getWorldMatrix();


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory

		//! Creates and returns RRMesh that describes mesh after transformation to world space.
		//
		//! Newly created instance allocates no additional memory, but depends on
		//! original object, so it is not allowed to let new instance live longer than original object.
		RRMesh* createWorldSpaceMesh();

		//! Creates and returns RRObject that describes object after transformation to world space.
		//
		//! Newly created instance has no transformation matrix, but it is still on the same 
		//! place in world space, because all vertices are transformed.
		//! \n Newly created instance allocates no additional memory, but depends on
		//! original object, so it is not allowed to let new instance live longer than the original object.
		//! \param negScaleMakesOuterInner
		//!  After negative scale, singlesided box visible only from outside will be visible only from inside.
		//!  \n\n Implementation details:
		//!  \n Both original and transformed object share the same mesh and materials, so both 
		//!  objects contain triangles with the same vertex order (e.g. ABC, 
		//!  not ACB) and materials visible for example from outside.
		//!  Negative scale naturally makes the object visible from inside
		//!  and rays collide with the inner side. This is the case of negScaleMakesOuterInner=true.
		//!  \n However one may want to change this behaviour. 
		//!  \n To get the transformed object visible from the opposite side and rays collide with the opposite side,
		//!  one can change the mesh (vertex order in all triangles) and share materials
		//!  or share the mesh and change materials.
		//!  It is more efficient to share the mesh and change materials.
		//!  So transformed object shares the mesh but when it detects negative scale,
		//!  it swaps sideBits[0] and sideBits[1] in all materials.
		//!  \n\n Note that shared RRMesh knows nothing about your local negScaleMakesOuterInner setting,
		//!  it is encoded in RRObject materials,
		//!  so if you calculate singlesided collision on mesh from newly created object,
		//!  give it a collision handler object->createCollisionHandlerFirstVisible()
		//!  which scans object's materials and responds to your local negScaleMakesOuterInner.
		//!  \n\n With negScaleMakesOuterInner=false, all materials sideBits[0] and [1] are swapped,
		//!  so where your system processes hits to front side on original object, it processes 
		//!  hits to back side on negatively scaled object.
		//!  Note that forced singlesided test (simple test without collision handler, see RRRay::TEST_SINGLESIDED) 
		//!  detects always front sides, so it won't work with negative scale and negScaleMakesOuterInner=false.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		RRObject* createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, const char* cacheLocation);

		//! Creates and returns union of multiple objects (contains geometry and materials from all objects).
		//
		//! Created instance (MultiObject) doesn't require additional memory, 
		//! but it depends on all objects from array, they must stay alive for whole life of MultiObject.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n For array with 1 element, pointer to that element may be returned.
		//! \n\n For description how to access original triangles and vertices in MultiObject, 
		//!  see RRMesh::createMultiMesh(). Note that for non-negative vertexWeldDistance,
		//!  some vertices may be optimized out, so prefer PreImpport<->PostImport conversions.
		//! \param objects
		//!  Array of objects you want to create multiobject from.
		//!  Objects from array should stay alive for whole life of multiobjects (this is your responsibility).
		//!  Array alone may be destructed immediately by you.
		//! \param numObjects
		//!  Number of objects in array.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		//! \param vertexWeldDistance
		//!  Distance in world units. Vertices with lower or equal distance will be stitched into one vertex.
		//!  Zero stitches only identical vertices, negative value means no action.
		//! \param optimizeTriangles
		//!  True removes degenerated triangles.
		//!  It is always good to get rid of degenerated triangles (true), but sometimes you know
		//!  there are no degenerated triangles at all and you can save few cycles by setting false.
		//! \param speed
		//!  Could make object faster, but needs additional memory.
		//!  \n 0 = normal speed, 0bytes/triangle overhead
		//!  \n 1 = +speed, 12bytes/triangle overhead
		//!  \n 2 = ++speed, 156bytes/triangle overhead
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		static RRObject* createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting, float vertexWeldDistance, bool optimizeTriangles, unsigned speed, const char* cacheLocation);

		//! Creates and returns object with materials converted to physical space.
		//
		//! Created instance contains copy of all materials, converted and adjusted to fit in physical space.
		//! \n It is typically used to convert user provided objects in custom scale to physical scale.
		//! \n Created instance depends on original object and scaler, so it is not allowed to delete original object and scaler before deleting newly created instance.
		//! \param scaler
		//!  Scaler used for custom scale -> physical scale conversion.
		//!  Provide the same scaler you use for the rest of calculation.
		//! \param aborting
		//!  May be set asynchronously, aborts creation.
		class RRObjectWithPhysicalMaterials* createObjectWithPhysicalMaterials(const class RRScaler* scaler, bool& aborting);


		// collision helper

		//! Creates and returns collision handler, that finds closest visible surface.
		//
		//! \n Finds closest surface with RRMaterial::sideBits::render.
		//! \n It is suitable e.g for picking objects in rendering window, only rendered pixels collide.
		//!
		//! Thread safe: this function yes, but created collision handler no.
		//! (typical use case: for n threads, use 1 collider, n rays and n handlers.)
		RRCollisionHandler* createCollisionHandlerFirstVisible() const;


		// camera helper

		//! Generates position and direction suitable for automatically placed camera.
		//
		//! All parameters are filled by function.
		//! outMaxdist is upper bound for distance between two points in scene,
		//! it could be used for setting camera far.
		void generateRandomCamera(RRVec3& outPos, RRVec3& outDir, RRReal& outMaxdist) const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjectWithPhysicalMaterials
	//! Interface for object wrapper that converts materials from custom to physical scale.
	//
	//! Helper interface.
	//! Instance may be created by RRMesh::createObjectWithPhysicalMaterials().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRObjectWithPhysicalMaterials : public RRObject
	{
	public:
		//! Updates materials in physical scale according to actual scaler and materials in custom scale.
		//! \param aborting
		//!  May be set asynchronously, aborts update.
		virtual void update(bool& aborting) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRIlluminatedObject
	//! 3d object with storage space for calculated illumination.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRIlluminatedObject
	{
		RRObject* object;
		RRObjectIllumination* illumination;
		RRIlluminatedObject(RRObject* o, RRObjectIllumination* i) : object(o), illumination(i) {};
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjects
	//! Set of illuminated objects with interface similar to std::vector.
	//
	//! This is usual product of adapter that creates Lightsprint interface for external 3d scene.
	//! You may use it for example to
	//! - send it to RRDynamicSolver and calculate global illumination
	//! - manipulate this set before sending it to RRDynamicSolver, e.g. remove moving objects
	//! - render it immediately, without calculating global illumination
	//! - render it when global illumination is calculated
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjects : public RRVector<RRIlluminatedObject>
	{
	public:
		//! Structure used by recommendLayerParameters().
		struct LayerParameters
		{
			// inputs to RRObjects::recommendLayerParameters()
			int            objectIndex; ///< -1 = global params for all objects; 0,1,2... params specific for given object
			unsigned       suggestedMapSize;
			unsigned       suggestedMinMapSize;
			unsigned       suggestedMaxMapSize;
			float          suggestedPixelsPerWorldUnit;
			const char*    suggestedPath;
			const char*    suggestedExt;

			// outputs of RRObjects::recommendLayerParameters()
			RRBufferType   actualType;
			unsigned       actualWidth;
			unsigned       actualHeight;
			RRBufferFormat actualFormat;
			bool           actualScaled;
			char*          actualFilename; ///< NULL in constructor, malloced in RRObjects::recommendLayerParameters(), freed in destructor.

			// tools
			LayerParameters()
			{
				objectIndex = -1;
				suggestedMapSize = 256;
				suggestedMinMapSize = 32;
				suggestedMaxMapSize = 1024;
				suggestedPixelsPerWorldUnit = 1;
				suggestedPath = "";
				suggestedExt = "png";
				actualFilename = NULL;
			}

			//! Creates buffer from actualXxx fields (filled by RRObjects::recommendLayerParameters())
			RRBuffer* createBuffer() const
			{
				return RRBuffer::create(actualType,actualWidth,actualHeight,1,actualFormat,actualScaled,NULL);
			}

			~LayerParameters()
			{
				free(actualFilename);
			}
		};

		//! Recommends layer parameters (resolution, filename etc).
		//
		//! Nearly all tools and samples need to decide what lightmap resolutions to use, what lightmap filenames
		//! to use etc.
		//! Samples are free to use arbitrary resolutions and filenames and often they do.
		//! But for convenience of undecided tools and samples, this function is provided as
		//! a central point of their decision making.
		//!
		//! Applications often have some opinion, so this function takes their suggections on resolution,
		//! path, extension etc, stored in layerParameters (suggestedXxx fields).
		//! Recommended new resolution, filename etc are output into the same structure (actualXxx fields).
		//!
		//! Custom implementations of this function use different decision making rules.
		//! For example adapter of Gamebryo .gsa scene never recommends vertex buffer,
		//! because Gamebryo doesn't support lighting in vertex buffers yet.
		//! \param layerParameters
		//!  Structure of both inputs (suggestedXxx) and outputs (actualXxx).
		//!  Outputs are filled by this function.
		virtual void recommendLayerParameters(RRObjects::LayerParameters& layerParameters) const;
		//! Loads illumination layer from disk.
		//
		//! It is shortcut for calling illumination->getLayer() = rr::RRBuffer::load() on all elements in this container.
		//! \param layerNumber
		//!  Layer to load, nothing is done for negative number.
		//! \param path
		//!  Where to read files, should have trailing slash.
		//! \param ext
		//!  File format of maps to load, e.g. "png".
		//!  Vertex buffers are always loaded from .vbu, without regard to ext.
		//! \remark
		//!  rr_io::registerLoaders() must be called for image saves/loads to work.
		virtual unsigned loadLayer(int layerNumber, const char* path, const char* ext) const;

		//! Saves illumination layer to disk.
		//
		//! It is shortcut for calling illumination->getLayer()->save() on all elements in this container.
		//! \param layerNumber
		//!  Layer to save, nothing is done for negative number.
		//! \param path
		//!  Where to store files, should have trailing slash. Subdirectories are not created.
		//! \param ext
		//!  File format of maps to save, e.g. "png".
		//!  Vertex buffers are always saved to .vbu, without regard to ext.
		//! \remark
		//!  rr_io::registerLoaders() must be called for image saves/loads to work.
		virtual unsigned saveLayer(int layerNumber, const char* path, const char* ext) const;

		virtual ~RRObjects() {};
	};

} // namespace

#endif
