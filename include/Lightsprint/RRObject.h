#ifndef RROBJECT_H
#define RROBJECT_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRObject.h
//! \brief LightsprintCore | 3d object with geometry, materials, position etc
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2008
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRCollider.h"
#include "RRLight.h" // RRScaler

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
		unsigned char pointDetails:1;///< 1=material has important per-pixel details. It's hint for solver to use per-pixel materials. Solver always starts with fast RRObject::getTriangleMaterial(), but if it has pointDetails set, slower but more detailed getPointMaterial() is used instead. Save calculation time by enabling pointDetails only for materials with strong per-pixel differences, e.g. trees with transparency in alpha.
	};

	//! Description of material properties of a surface.
	//
	//! It is minimal set of properties needed by global illumination solver,
	//! so it not complete material for rendering (no textures).
	//!
	//! Values could be in physical or any other scale, depends on who uses it.
	//! Adapters usually create materials in sRGB scale -> material properties are screen colors.
	//! Solver always converts materials to physical scale, you can access them via RRDynamicSolver::getMultiObjectPhysical().
	struct RR_API RRMaterial
	{
		//! Part of material description.
		struct Property
		{
			RRVec3                 color;    ///< Material property expressed as 3 floats. If texture is present, this is average color from texture.
			class RRBuffer*        texture;  ///< Material property expressed as texture. Read texcoords via object->getChannelData(RRObject::CHANNEL_TRIANGLE_VERTICES_xxx_UV,triangle,uv,sizeof(uv)).
			unsigned               texcoord; ///< Texcoord channel used by texture. For adapter's internal use, don't use directly for reading texcoords.

			//! Clears property to default zeroes.
			Property()
			{
				color = RRVec3(0);
				texture = NULL;
				texcoord = 0;
			}
		};

		//! Resets material to fully diffuse gray (50% reflected, 50% absorbed).
		//
		//! In 1sided version, back side is not rendered, but light doesn't get through it.
		//! It is good for making solid (3d) objects. Light that accidentally gets inside object and hits
		//! invisible back side is deleted.
		//!
		//! In 2sided version, back side is rendered, spec.reflects, refracts, emits, but doesn't dif.reflect.
		//! It makes good glass, but bad thin dif.reflecting wall.
		void          reset(bool twoSided);

		//! Changes material to closest physically valid values. Returns true when changes were made.
		bool          validate();

		//! Converts material properties from physical to custom scale.
		void          convertToCustomScale(const RRScaler* scaler);
		//! Converts material properties from custom to physical scale.
		void          convertToPhysicalScale(const RRScaler* scaler);

		RRSideBits    sideBits[2];                   ///< Defines material behaviour for front (sideBits[0]) and back (sideBits[1]) side.
		Property      diffuseReflectance;            ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Diffuse_reflection">diffuse reflection</a> (each channel separately).
		Property      diffuseEmittance;              ///< Radiant emittance in watts per square meter (each channel separately). (Adapters usually create materials in sRGB scale, so that this is screen color.)
		RRReal        specularReflectance;           ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Specular_reflection">specular reflection</a> (without color change).
		Property      specularTransmittance;         ///< Fraction of energy that continues through surface (with direction possibly changed by refraction).
		bool          specularTransmittanceInAlpha;  ///< Whether specular transmittance is in specularTransmittance.texture's Alpha (0=transparent) or in RGB (1=transparent). It is irrelevant when specularTransmittance.texture==NULL.
		bool          specularTransmittanceKeyed;    ///< Whether alpha keying is used instead of alpha blend while realtime rendering. Alpha keying is faster but removes semi-translucency. Alpha blend renders semi-translucency, but artifacts appear on meshes where semi-translucent faces overlap.
		RRReal        refractionIndex;               ///< Refractive index of matter in front of surface divided by refractive index of matter behind surface. <a href="http://en.wikipedia.org/wiki/List_of_indices_of_refraction">Examples.</a>
		const char*   name;                          ///< Optional name of material, may be NULL.
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

	class RR_API RRObject : public RRChanneledData
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Channels
		//////////////////////////////////////////////////////////////////////////////

		enum
		{
			CHANNEL_TRIANGLE_VERTICES_DIFFUSE_UV      = RRMesh::INDEXED_BY_TRIANGLE+8, ///< channel contains RRVec2[3] for each triangle
			CHANNEL_TRIANGLE_VERTICES_EMISSIVE_UV     = RRMesh::INDEXED_BY_TRIANGLE+9, ///< channel contains RRVec2[3] for each triangle
			CHANNEL_TRIANGLE_VERTICES_TRANSPARENCY_UV = RRMesh::INDEXED_BY_TRIANGLE+10, ///< channel contains RRVec2[3] for each triangle
			CHANNEL_TRIANGLE_OBJECT_ILLUMINATION      = RRMesh::INDEXED_BY_TRIANGLE+11, ///< channel contains RRObjectIllumination* for each triangle
		};

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
		//! This is slower per-pixel version of faster per-triangle getTriangleMaterial().
		//! \n\n Lighting computed with getPointMaterial() is usually only slightly better, but takes much longer time
		//! to converge than lighting computed with getTriangleMaterial().
		//! So you should use getPointMaterial() only if you know it returns important additional details.
		//! \n\n Solver uses getPointMaterial() when getTriangleMaterial()->sideBits[].pointDetails is set.
		//! \n\n Default implementation returns average triangle's material.
		//! \param t Triangle number.
		//! \param uv 2D coordinates of point (triangle vertices are in 0,0 1,0 0,1).
		//! \param out Returned material in given point, undefined on input, to be filled by implementation.
		virtual void getPointMaterial(unsigned t, RRVec2 uv, RRMaterial& out) const;

		//! Information about single object, what LOD it is.
		struct LodInfo
		{
			//! LODs belong together only if they have identical bases.
			void* base;
			//! LODs that belong together always differ in level. There must be always at least level 0 for each base.
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

		//! Returns object transformation.
		//
		//! Allowed transformations are composed of translation, rotation, scale.
		//! \n There is default implementation that always returns NULL, which means no transformation.
		//! \return Pointer to matrix that transforms object space to world space.
		//!  May return NULL for identity/no transformation. 
		//!  Pointer must be constant and stay valid for whole life of object.
		//!  Matrix may change during object life.
		virtual const RRMatrix3x4*  getWorldMatrix();

		//! Differs from getWorldMatrix() only by returning _inverse_ matrix.
		virtual const RRMatrix3x4*  getInvWorldMatrix();


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
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		//!  It is passed to RRCollider::create(), so
		//!  default NULL caches in temp, "*" or any other invalid path disables caching, any valid is path where to cache colliders.
		RRObject* createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, const char* cacheLocation);

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
		//!  For now, all objects must have the same data channels (see RRChanneledData).
		//! \param numObjects
		//!  Number of objects in array.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
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
		static RRObject* createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float vertexWeldDistance, bool optimizeTriangles, unsigned speed, const char* cacheLocation);

		//! Creates and returns object with materials converted to physical space.
		//
		//! Created instance contains copy of all materials, converted and adjusted to fit in physical space.
		//! \n It is typically used to convert user provided objects in custom scale to physical scale.
		//! \n Created instance depends on original object and scaler, so it is not allowed to delete original object and scaler before deleting newly created instance.
		//! \param scaler
		//!  Scaler used for custom scale -> physical scale conversion.
		//!  Provide the same scaler you use for the rest of calculation.
		class RRObjectWithPhysicalMaterials* createObjectWithPhysicalMaterials(const class RRScaler* scaler);


		// collision helper

		//! Creates and returns collision handler, that finds closest visible surface.
		//
		//! If RRObject::getTriangleMaterials()->sideBits[].pointDetails is set, point details (e.g. alpha keying)
		//! provided by RRObject::getPointMaterial() are used.
		//! \n Finds closest surface with RRMaterial::sideBits::render.
		//! \n It is suitable e.g for picking objects in rendering window, only rendered pixels collide.
		//!
		//! Thread safe: this function yes, but created collision handler no.
		//! (typical use case: for n threads, use 1 collider, n rays and n handlers.)
		RRCollisionHandler* createCollisionHandlerFirstVisible();


		// camera helper

		//! Generates position and direction suitable for automatically placed camera.
		//
		//! All parameters are filled by function.
		//! outMaxdist is upper bound for distance between two points in scene,
		//! it could be used for setting camera far.
		void generateRandomCamera(RRVec3& outPos, RRVec3& outDir, RRReal& outMaxdist);
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
		virtual void update() = 0;
	};


} // namespace

#endif
