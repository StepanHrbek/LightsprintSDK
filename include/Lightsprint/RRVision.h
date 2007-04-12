#ifndef RRVISION_H
#define RRVISION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVision.h
//! \brief RRVision - library for fast global illumination calculations
//! \version 2007.3.20
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRCollider.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#ifdef NDEBUG
			#pragma comment(lib,"RRVision_sr.lib")
		#else
			#pragma comment(lib,"RRVision_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_VISION
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#ifdef NDEBUG
	#pragma comment(lib,"RRVision.lib")
#else
	#pragma comment(lib,"RRVision_dd.lib")
#endif
#	endif
#	endif
#endif

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// primitives
	//
	// RRMatrix3x4 - matrix 3x4
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Matrix of 3x4 real numbers in row-major order.
	//
	//! Translation is stored in m[x][3].
	//! Rotation and scale in the rest.
	//! \n We have chosen this format because it contains only what we need, is smaller than 4x4
	//! and its shape makes no room for row or column major ambiguity.
	struct RR_API RRMatrix3x4
	{
		RRReal m[3][4];

		//! Returns position in 3d space transformed by matrix.
		RRVec3 transformedPosition(const RRVec3& a) const;
		//! Transforms position in 3d space by matrix.
		RRVec3& transformPosition(RRVec3& a) const;
		//! Returns direction in 3d space transformed by matrix.
		RRVec3 transformedDirection(const RRVec3& a) const;
		//! Transforms direction in 3d space by matrix.
		RRVec3& transformDirection(RRVec3& a) const;
		//! Returns determinant of first 3x3 elements.
		RRReal determinant3x3() const;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// material aspects of space and surfaces
	//
	// RRColor              - rgb color
	// RRRadiometricMeasure - radiometric measure
	// RREmittanceType      - type of emission
	// RRSideBits           - 1bit attributes of one side
	// RRMaterial           - material properties of a surface
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Color representation, r,g,b 0..1.
	typedef RRVec3 RRColor; 

	//! Specification of radiometric measure; what is measured and what units are used.
	struct RRRadiometricMeasure
	{
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
	#define RM_IRRADIANCE_PHYSICAL_INDIRECT RRRadiometricMeasure(0,0,0,0,1)
	#define RM_IRRADIANCE_CUSTOM_INDIRECT   RRRadiometricMeasure(0,1,0,0,1)
	#define RM_IRRADIANCE_PHYSICAL          RRRadiometricMeasure(0,0,0,1,1)
	#define RM_IRRADIANCE_CUSTOM            RRRadiometricMeasure(0,1,0,1,1)
	#define RM_EXITANCE_PHYSICAL            RRRadiometricMeasure(1,0,0,1,1)
	#define RM_EXITANCE_CUSTOM              RRRadiometricMeasure(1,1,0,1,1)


	//! Boolean attributes of one side of a surface. Usually exist in array of two elements, for front and back side.
	struct RRSideBits
	{
		unsigned char renderFrom:1;  ///< Should surface be visible from its side? Information only for renderer, not for radiosity solver.
		unsigned char emitTo:1;      ///< Should surface emit energy to its side? When false, it disables emittance.
		unsigned char catchFrom:1;   ///< Should surface catch photons coming from its side? When photon is catched, receiveFrom, reflect and transmitFrom are tested.
		unsigned char receiveFrom:1; ///< When photon is catched, should surface receive its energy?
		unsigned char reflect:1;     ///< When photon is catched, should surface reflect it? When false, it disables reflectance.
		unsigned char transmitFrom:1;///< When photon is catched, should surface refract it? When false, it disables transmittance.
	};

	//! Description of material properties of a surface.
	//
	//! It is minimal set of properties needed by global illumination solver,
	//! so it not complete material for rendering (no textures).
	//! Values could be in physical or any other scale, depends on who uses it.
	struct RR_API RRMaterial
	{
		void          reset(bool twoSided);          ///< Resets material to fully diffuse gray (50% reflected, 50% absorbed).
		bool          validate();                    ///< Changes material to closest physically valid values. Returns if any changes were made.

		RRSideBits    sideBits[2];                   ///< Defines material behaviour for front (sideBits[0]) and back (sideBits[1]) side.
		RRColor       diffuseReflectance;            ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Diffuse_reflection">diffuse reflection</a> (each channel separately).
		RRColor       diffuseEmittance;              ///< Radiant emittance in watts per square meter (each channel separately).
		RRReal        specularReflectance;           ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Specular_reflection">specular reflection</a> (without color change).
		RRReal        specularTransmittance;         ///< Fraction of energy that continues through surface (without color change).
		RRReal        refractionIndex;               ///< Refractive index of material in front of surface (surface is a boundary between two materials) divided by refractive index of material behind surface. <a href="http://en.wikipedia.org/wiki/List_of_indices_of_refraction">Examples.</a>
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRScaler
	//! Interface for physical <-> custom space transformer.
	//
	//! RRScaler may be used to transform irradiance/emittance/exitance 
	//! between physical W/m^2 space and custom user defined space.
	//! Without scaler, all inputs/outputs work with specified physical units.
	//! With appropriate scaler, you may directly work for example with screen colors
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
	//! Contains built-in support for typical RGB monitor space, see createRgbScaler().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRScaler
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! Converts color from physical scale (W/m^2) value to user defined scale.
		virtual void getCustomScale(RRColor& physicalScale) const = 0;

		//! Converts color from user defined scale to physical scale (W/m^2).
		virtual void getPhysicalScale(RRColor& customScale) const = 0;

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
	//! - baking additional (primary) illumination into object, see createObjectWithIllumination()
	//!
	//! \section s3 Links between objects
	//! RRStaticSolver -> RRObject -> RRCollider -> RRMesh
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
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRObject() {}


		//
		// must not change during object lifetime
		//

		//! Returns collider of underlying mesh. It is also access to mesh itself (via getCollider()->getMesh()).
		//! Must always return valid collider, implementation is not allowed to return NULL.
		virtual const RRCollider* getCollider() const = 0;

		//! Returns triangle's material description.
		//
		//! Returned pointer must stay valid and constant for whole life of object.
		//! Function may return NULL when material description is not available.
		virtual const RRMaterial* getTriangleMaterial(unsigned t) const = 0;


		//
		// may change during object lifetime
		//

		//! Writes t-th triangle additional measure to out.
		//
		//! Although each triangle has its RRMaterial::diffuseEmittance,
		//! it may be inconvenient to create new RRMaterial for each triangle when only emissions differ.
		//! So this is way how to provide additional emissivity for each triangle separately.
		//! \n Default implementation always returns 0.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param measure Specifies requested radiometric measure. Scaled must be 1. Direct/indirect may be ignored.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested measure is written to out. For invalid t, out stays unmodified.
		virtual void                getTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor& out) const;

		//! Returns object transformation.
		//
		//! Allowed transformations are composed of translation, rotation, scale.
		//! Scale has not been extensively tested yet, problems with negative or non-uniform 
		//! scale may eventually appear, but they would be fixed in future versions.
		//! \n There is default implementation that always returns NULL, meaning no transformation.
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
		RRObject* createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation);

		//! Creates and returns union of multiple objects (contains geometry and materials from all objects).
		//
		//! Created instance (MultiObject) doesn't require additional memory, 
		//! but it depends on all objects from array, they must stay alive for whole life of MultiObject.
		//! \n This can be used to accelerate calculations, as one big object is nearly always faster than multiple small objects.
		//! \n This can be used to simplify calculations, as processing one object may be simpler than processing array of objects.
		//! \n For array with 1 element, pointer to that element may be returned.
		//! \n\n For description how to access original triangles and vertices in MultiObject, 
		//!  see RRMesh::createMultiMesh(). Note that for non-negative maxStitchDistance,
		//!  some vertices may be optimized out, so prefer PreImpport<->PostImport conversions.
		//! \param objects
		//!  Array of objects you want to create multiobject from.
		//!  Objects from array should stay alive for whole life of multiobjects (this is your responsibility).
		//!  Array alone may be destructed immediately by you.
		//! \param numObjects
		//!  Number of objects in array.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param maxStitchDistance
		//!  Distance in world units. Vertices with lower or equal distance will be stitched into one vertex.
		//!  Zero stitches only identical vertices, negative value means no action.
		//! \param optimizeTriangles
		//!  True removes degenerated triangles.
		//!  It is always good to get rid of degenerated triangles (true), but sometimes you know
		//!  there are no degenerated triangles at all and you can save few cycles by setting false.
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		static RRObject* createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation);

		//! Creates and returns object with space for per-triangle user-defined additional illumination.
		//
		//! Created instance contains buffer for per-triangle additional illumination that is initialized to 0 and changeable via setTriangleIllumination.
		//! \n It is typically used to store detected direct illumination generated by custom renderer.
		//! \n Created instance depends on original object and scaler, so it is not allowed to delete original object and scaler before deleting newly created instance.
		//! Illumination defined here overrides original object's illumination (if it has any).
		//! \param scaler
		//!  Scaler used for physical scale <-> custom scale conversions.
		//!  Provide the same scaler you use for the rest of calculation.
		class RRObjectWithIllumination* createObjectWithIllumination(const RRScaler* scaler);

		//! Creates and returns object with materials converted to physical space.
		//
		//! Created instance contains copy of all materials, converted and adjusted to fit in physical space.
		//! \n It is typically used to convert user provided objects in custom scale to physical scale.
		//! \n Created instance depends on original object and scaler, so it is not allowed to delete original object and scaler before deleting newly created instance.
		//! \param scaler
		//!  Scaler used for custom scale -> physical scale conversion.
		//!  Provide the same scaler you use for the rest of calculation.
		class RRObjectWithPhysicalMaterials* createObjectWithPhysicalMaterials(const RRScaler* scaler);


		// collision helper

		//! Creates and returns collision handler,
		//! that accepts first hit to visible side
		//! (according to RRMaterial::sideBits::render).
		RRCollisionHandler* createCollisionHandlerFirstVisible();
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjectWithIllumination
	//! Interface for object importer with user-defined additional per-triangle illumination.
	//
	//! Helper interface.
	//! Instance may be created by RRMesh::createObjectWithIllumination().
	//! Typically used for storage of object's direct illumination detected by application.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRObjectWithIllumination : public RRObject
	{
	public:
		//! Sets triangle's illumination.
		//
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param measure Radiometric measure used for power. Direct/indirect may be ignored.
		//! \param illumination Amount of additional illumination for triangle t in units specified by measure.
		//! \return True on success, false on invalid inputs.
		virtual bool                setTriangleIllumination(unsigned t, RRRadiometricMeasure measure, RRColor illumination) = 0;
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




	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRStaticSolver
	//! 3d scene where illumination can be calculated.
	//
	//! %RRStaticSolver covers the most important functionality of library -
	//! illumination calculation. It allows you to setup multiple objects in space,
	//! setup other states that affect calculations, perform calculations
	//! and read back results.
	//!
	//! Common scenario is to perform calculations while allowing 
	//! user to continue work and see illumination improving on the background.
	//! This could be done by inserting following code into your main loop
	//! \code
	//    if(!paused) {
	//      scene->illuminationImprove(endIfUserWantsToInteract);
	//      if(once a while) read back results using scene->getTriangleMeasure();
	//    } \endcode
	//! \n For smooth integration without feel of slowing down, it is good to pause 
	//! calculation after each user interaction and resume it again once there is
	//! no user interaction for at least 0.5 sec.
	//! \n When user modifies scene, calculation may be restarted using illuminationReset().
	//!  For scenes small enough, this creates global illumination in dynamic scene 
	//!  with full interactivity.
	//!
	//! Thread safe: It is legal to create multiple scenes and
	//!  use them in parallel, in multiple threads.
	//!  It is legal to read results from one scene in multiple threads at once.
	//!  It is not allowed to calculate in one scene in multiple threads at once.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRStaticSolver
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		//
		// import geometry
		//

		//! Illumination smoothing parameters.
		struct SmoothingParameters
		{
			//! Speed of surface subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast.
			//! \n Set 0 for the fastest results and realtime responsiveness. Illumination will be available in scene vertices.
			//! \n Set 1 for higher quality, precalculations. Illumination will be available in adaptively subdivided surfaces.
			//!    You can set slower subdivision for higher quality results with less noise, calculation will be slower.
			//!    If you set higher speed, calculation will be faster, but results will contain high frequency noise.
			float subdivisionSpeed;
			//! Selects smoothing mode, valid options are: 0,1,2.
			//! \n 0 = Normal independent smoothing, old. Depends on maxSmoothAngle.
			//! \n 1 = Normal independent smoothing, new. Depends on maxSmoothAngle.
			//! \n 2 = Smoothing defined by object normals. Faces with the same normal on shared vertex are smoothed. (disabled for now, falls back to 1)
			unsigned smoothMode;
			//! Distance in world units. Vertices with lower or equal distance will be internally stitched into one vertex.
			//! Zero stitches only identical vertices, negative value generates no action.
			//! Non-stitched vertices at the same location create illumination discontinuity.
			//! \n If your geometry is clean and needs no stitching, make sure to set negative value, calculation will be faster.
			float stitchDistance;
			//! Distance in world units. Smaller features will be smoothed. This could be imagined as a kind of blur.
			//! Use 0 for no smoothing and watch for possible artifacts in areas with small geometry details
			//! and 'needle' triangles. Increase until artifacts disappear.
			//! 15cm (0.15 for game with 1m units) could be good for typical interior game.
			float minFeatureSize;
			//! Angle in radians, controls automatic smoothgrouping.
			//! Edges with smaller angle between face normals are smoothed out.
			//! Optimal value depends on your geometry, but reasonable value could be 0.33.
			float maxSmoothAngle;
			//! Minimal allowed angle in triangle (rad), sharper triangles are ignored.
			//! Helps prevent problems from degenerated triangles.
			//! 0.001 is reasonable value.
			float ignoreSmallerAngle;
			//! Minimal allowed area of triangle, smaller triangles are ignored.
			//! Helps prevent precision problems from degenerated triangles.
			//! For typical game interior scenes and world in 1m units, 1e-10 is reasonable value.
			float ignoreSmallerArea;
			//! Intersection technique used for smoothed object.
			//! Techniques differ by speed and memory requirements.
			RRCollider::IntersectTechnique intersectTechnique;
			//! Sets default values at creation time.
			//! These values are suitable for typical interior scenes with 1m units.
			SmoothingParameters()
			{
				subdivisionSpeed = 0;
				smoothMode = 2;
				stitchDistance = 0.01f;
				minFeatureSize = 0.15f;
				maxSmoothAngle = 0.33f;
				ignoreSmallerAngle = 0.001f;
				ignoreSmallerArea = 1e-10f;
				intersectTechnique = RRCollider::IT_BSP_FASTER;
			}
		};

		//! Creates new static scene.
		//
		//! For highest performance, stay with low number of possibly big objects 
		//! rather than high number of small ones.
		//! One big object can be created out of many small ones using RRObject::createMultiObject().
		//!
		//! Different objects always belong to different smoothgroups. So with flat wall cut into two objects,
		//! sharp edge will possibly appear between them.
		//! This can be fixed by merging standalone objects into one object using RRObject::createMultiObject().
		//! \param object
		//!  World-space object wrapper that defines object shape and material.
		//!  \n If object has transformation different from identity, pass object->createWorldSpaceObject()
		//!  rather than object itself, otherwise object transformation will be ignored.
		//!  \n object->getTriangleMaterial() should return values in physical scale.
		//! \param smoothing
		//!  Illumination smoothing parameters.
		RRStaticSolver(RRObject* object, const SmoothingParameters* smoothing);

		//! Destructs static scene.
		~RRStaticSolver();
		
		//////////////////////////////////////////////////////////////////////////////
		//
		// calculate radiosity
		//

		//! Describes result of illumination calculation.
		enum Improvement
		{
			IMPROVED,       ///< Illumination was improved during this call.
			NOT_IMPROVED,   ///< Although some calculations were done, illumination was not yet improved during this call.
			FINISHED,       ///< Correctly finished calculation (probably no light in scene). Further calls for improvement have no effect.
			INTERNAL_ERROR, ///< Internal error, probably caused by invalid inputs (but should not happen). Further calls for improvement have no effect.
		};

		//! Reset illumination to original state defined by objects.
		//
		//! There is no need to reset illumination right after scene creation, it is already reset.
		//! \param resetFactors
		//!  True: Resetting factors at any moment means complete restart of calculation with all expenses.
		//!  \n False: With factors preserved, part of calculation is reused, but you must ensure, that
		//!  geometry and materials were not modified. This is especially handy when only primary
		//!  lights move, but objects and materials stay unchanged.
		//! \param resetPropagation
		//!  Both options work well in typical situations, but well choosen value may improve performance.
		//!  \n True: Illumination already propagated using old factors is reset.
		//!     It is faster option when illumination changes 
		//!     significantly -> use after big light movement, light on/off.
		//!  \n False: Illumination already propagated using old factors is preserved for 
		//!     future calculation. It is only updated. It is faster option when illumination 
		//!     changes by small amount -> use after tiny light movement, small color/intensity change.
		//! \return Calculation state, see Improvement.
		Improvement   illuminationReset(bool resetFactors, bool resetPropagation);

		//! Improve illumination until endfunc returns true.
		//
		//! If you want calculation as fast as possible, make sure that most of time
		//! is spent here. You may want to interleave calculation by reading results, rendering and processing
		//! event queue, but stop rendering and idle looping when nothing changes and user doesn't interact,
		//! don't read results too often etc.
		//! \param endfunc Callback used to determine whether to stop or continue calculating.
		//!  It should be very fast, just checking system variables like time or event queue length.
		//!  It is called very often, slow endfunc may have big impact on performance.
		//! \param context Value is passed to endfunc without any modification.
		//! \return Calculation state, see Improvement.
		Improvement   illuminationImprove(bool endfunc(void*), void* context);

		//! Returns illumination accuracy in proprietary scene dependent units. Higher is more accurate.
		RRReal        illuminationAccuracy();


		//////////////////////////////////////////////////////////////////////////////
		//
		// read results
		//

		//! Reads illumination of triangle's vertex in units given by measure.
		//
		//! Reads results in format suitable for fast vertex based rendering without subdivision.
		//! \param triangle Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param vertex Index of triangle's vertex you want to get results for. Valid vertices are 0, 1, 2.
		//!  For invalid vertex number, average value for whole triangle is taken instead of smoothed value in vertex.
		//! \param measure
		//!  Specifies what to measure, using what units.
		//! \param scaler
		//!  Custom scaler for results in non physical scale. Scale conversion is enabled by measure.scaled.
		//! \param out
		//!  For valid inputs, illumination level is stored here. For invalid inputs, nothing is changed.
		//! \return
		//!  True if out was successfully filled. False may be caused by invalid inputs.
		bool          getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRColor& out) const;

		//! Illumination information for triangle's subtriangle.
		//
		//! Subtriangle is triangular area inside triangle given by three 2d coordinates in triangle space.
		//! If triangle has no subdivision, the only subtriangle fills whole triangle with following coordinates:
		//!  RRVec2(0,0), RRVec2(1,0), RRVec2(0,1)
		//! Illumination is recorded in subtriangle vertices.
		struct SubtriangleIllumination
		{
			RRVec2 texCoord[3]; ///< Subtriangle vertices positions in triangle space, triangle vertex0 is in 0,0, vertex1 is in 1,0, vertex2 is in 0,1.
			RRColor measure[3]; ///< Subtriangle vertices illumination.
		};
		//! Callback for passing multiple SubtriangleIlluminations to you.
		typedef void (SubtriangleIlluminationEater)(const SubtriangleIllumination& si, void* context);
		//! Reads illumination of triangle's subtriangles in units given by measure.
		//
		//! Reads results in format suitable for high quality texture based rendering (with adaptive subdivision).
		//! \param triangle
		//!  Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param measure
		//!  Specifies what to measure, using what units.
		//! \param scaler
		//!  Custom scaler for results in non physical scale. Scale conversion is enabled by measure.scaled.
		//! \param callback
		//!  Your callback that will be called for each triangle's subtriangle.
		//! \param context
		//!  Value is passed to callback without any modification.
		//! \return
		//!  Number of subtriangles processed.
		unsigned      getSubtriangleMeasure(unsigned triangle, RRRadiometricMeasure measure, const RRScaler* scaler, SubtriangleIlluminationEater* callback, void* context) const;


	private:
		class Scene*  scene;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLicense
	//! Everything related to your license number.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRLicense
	{
	public:
		//! States of your license.
		enum LicenseStatus
		{
			VALID,       ///< Valid license.
			EXPIRED,     ///< Expired license.
			WRONG,       ///< Wrong license.
			NO_INET,     ///< No internet connection to verify license.
			UNAVAILABLE, ///< Temporarily unable to verify license, try later.
		};
		//! Loads license from file.
		//
		//! This must be called before any other work with library.
		//! \return Result of license check.
		static LicenseStatus loadLicense(const char* filename);
	};

} // namespace

#endif
