#ifndef RRVISION_H
#define RRVISION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVision.h
//! \brief RRVision - library for fast global illumination calculations
//! \version 2006.9.20
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRCollider.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
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
#pragma comment(lib,"RRVision.lib")
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
	// RRSurface            - material properties of surface
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Color representation, r,g,b 0..1.
	typedef RRVec3 RRColor; 

	// Identifiers for radiometric measures.
	enum RRRadiometricMeasure
	{
		RM_INCIDENT_FLUX, ///< Incoming radiant flux [W].
		RM_IRRADIANCE,    ///< Irradiance [W/m^2].
		RM_EXITING_FLUX,  ///< Exiting radiant flux [W].
		RM_EXITANCE,      ///< Radiant exitance [W/m^2].
	};


	//! Boolean attributes of front or back side of surface.
	struct RRSideBits
	{
		unsigned char renderFrom:1;  ///< Should surface be visible from that halfspace? Information only for renderer, not for radiosity solver.
		unsigned char emitTo:1;      ///< Should surface emit energy to that halfspace?
		unsigned char catchFrom:1;   ///< Should surface catch photons incoming from that halfspace? When photon is catched, receiveFrom, reflect and transmitFrom are tested.
		unsigned char receiveFrom:1; ///< When photon is catched, should surface receive energy?
		unsigned char reflect:1;     ///< When photon is catched, should surface reflect energy to that halfspace?
		unsigned char transmitFrom:1;///< When photon is catched, should surface transmit energy to other halfspace?
	};

	//! Description of surface material properties.
	struct RR_API RRSurface
	{
		void          reset(bool twoSided);          ///< Resets surface to fully diffuse gray (50% reflected, 50% absorbed).
		bool          validate();                    ///< Changes surface to closest physically possible values. Returns if any changes were made.

		RRSideBits    sideBits[2];                   ///< Defines surface behaviour for front(0) and back(1) side.
		RRColor       diffuseReflectance;            ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Diffuse_reflection">diffuse reflection</a> (each channel separately).
		RRColor       diffuseEmittance;              ///< Radiant emittance in watts per square meter (each channel separately). Never scaled by RRScaler.
		RRReal        specularReflectance;           ///< Fraction of energy that is reflected in <a href="http://en.wikipedia.org/wiki/Specular_reflection">specular reflection</a> (without color change).
		RRReal        specularTransmittance;         ///< Fraction of energy that continues through surface (without color change).
		RRReal        refractionIndex;               ///< Refractive index of material in front of surface divided by refractive index of material behind surface. <a href="http://en.wikipedia.org/wiki/List_of_indices_of_refraction">Examples.</a>
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
	//! - baking additional (primary) illumination into object, see createAdditionalIllumination()
	//!
	//! \section s3 Links between objects
	//! RRScene -> RRObject -> RRCollider -> RRMesh
	//! \n where A -> B means that
	//!  - A has pointer to B
	//!  - there is no automatic reference counting in B and no automatic destruction of B from A
	//! \n This means you may have multiple objects sharing one collider and mesh.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObject
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRObject() {}

		//
		// must not change during object lifetime
		//
		//! Returns collider of underlying mesh. It is also access to mesh itself (via collider->getMesh()).
		virtual const RRCollider* getCollider() const = 0;
		//! Returns triangle's surface id.
		//
		//! It is not necessary for triangle surface ids to be small numbers,
		//! and thus no one is expected to create array of all surfaces indexed by surface id.
		virtual unsigned            getTriangleSurface(unsigned t) const = 0;
		//! Returns s-th surface material description.
		//
		//! \param s Id of surface. Valid s is any number returned by getTriangleSurface() for valid t.
		//! \returns For valid s, pointer to s-th surface. For invalid s, pointer to any surface. 
		//!  In both cases, surface must exist for whole life of object.
		virtual const RRSurface*    getSurface(unsigned s) const = 0;
		//
		// optional
		//
		//! Three normals for three vertices in triangle. In object space, normalized.
		struct TriangleNormals      {RRVec3 norm[3];};
		//! Three uv-coords for three vertices in triangle.
		struct TriangleMapping      {RRVec2 uv[3];};
		//! Writes to out vertex normals of triangle. In object space, normalized.
		//
		//! Future versions of Vision may use normals for smoothing results. Currently they are not used, smoothing is automatic.
		//! \n There is default implementation that writes all vertex normals equal to triangle plane normal.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested normals are written to out. For invalid t, out stays unmodified.
		virtual void                getTriangleNormals(unsigned t, TriangleNormals& out) const;
		//! Writes t-th triangle mapping for object unwrap into 0..1 x 0..1 space.
		//
		//! Unwrap may be used for returning results in texture (ambient map).
		//! \n There is default implementation that automatically generates objects unwrap of low quality.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested mapping is written to out. For invalid t, out stays unmodified.
		virtual void                getTriangleMapping(unsigned t, TriangleMapping& out) const;
		//! Writes t-th triangle additional measure to out.
		//
		//! Although each triangle has its RRSurface::diffuseEmittance,
		//! it may be inconvenient to create new RRSurface for each triangle when only emissions differ.
		//! So this is way how to provide additional emissivity for each triangle separately.
		//! \n There is default implementation that always returns 0.
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param measure Specifies requested radiometric measure.
		//! \param out Caller provided storage for result.
		//!  For valid t, requested measure is written to out. For invalid t, out stays unmodified.
		virtual void                getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const;

		//
		// may change during object lifetime
		//
		//! Returns object transformation.
		//
		//! Allowed transformations are composed of translation, rotation, scale.
		//! Scale has not been extensively tested yet, problems with negative or non-uniform 
		//! scale may eventually appear, but they would be fixed in future versions.
		//! \n There is default implementation that always returns NULL, meaning no transformation.
		//! \returns Pointer to matrix that transforms object space to world space.
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
		//! original object, so it is not allowed to let new instance live longer than original object.
		//! \param negScaleMakesOuterInner
		//!  After negative scale, singlesided box visible only from outside will be visible only from inside.
		//!  \n\n Implementation details:
		//!  \n Both original and transformed object share the same mesh and materials, so both 
		//!  objects contain triangles with the same vertex order (eg. ABC, 
		//!  not ACB) and materials visible for example from outside.
		//!  Negative scale naturally makes the object visible from inside
		//!  and rays collide with the inner side. This is the case of negScaleMakesOuterInner=true.
		//!  \n However one may want to change this behaviour. 
		//!  \n To get the transformed object visible from the opposite side and rays collide with the opposite side,
		//!  one can change the mesh (vertex order in all triangles) and share surfaces 
		//!  or share the mesh and change surfaces.
		//!  It is more efficient to share the mesh and change surfaces.
		//!  So transformed object shares the mesh but when it detects negative scale,
		//!  it switches sideBits[0] and sideBits[1] in all surfaces.
		//!  \n\n Note that shared RRMesh knows nothing about your local negScaleMakesOuterInner setting,
		//!  it is encoded in RRObject surfaces,
		//!  so if you calculate singlesided collision on mesh from newly created object,
		//!  give it a collision handler object->createCollisionHandlerFirstVisible()
		//!  which scans object's surfaces and responds to your local negScaleMakesOuterInner.
		//!  \n\n With negScaleMakesOuterInner=false, all surfaces sideBits are switched,
		//!  so where your system processes hits to front side on original object, it processes 
		//!  hits to back side on negatively scaled object.
		//!  Note that forced singlesided test (simple test without collision handler, see RRRay::TEST_SINGLESIDED) 
		//!  detects always front sides, so it won't work with negative scale and negScaleMakesOuterInner=false.
		//! \param intersectTechnique
		//!  Technique used for collider construction.
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		RRObject* createWorldSpaceObject(bool negScaleMakesOuterInner, RRCollider::IntersectTechnique intersectTechnique, char* cacheLocation);
		//! Creates and returns union of multiple objects (contains geometry and surfaces from all objects).
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
		//!  Vertices with lower or equal distance will be stitched into one vertex.
		//!  Zero stitches only identical vertices, negative value means no action.
		//! \param optimizeTriangles
		//!  True removes degenerated triangles.
		//!  It is always good to get rid of degenerated triangles (true), but sometimes you know
		//!  there are no degenerated triangles at all and you can save few cycles by setting false.
		//! \param cacheLocation
		//!  Directory for caching intermediate files used by RRCollider.
		static RRObject*    createMultiObject(RRObject* const* objects, unsigned numObjects, RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, bool optimizeTriangles, char* cacheLocation);
		//! Creates and returns object importer with space for per-triangle user-defined additional illumination.
		//!
		//! Created instance depends on original object, so it is not allowed to delete original object before newly created instance.
		//! \n Created instance contains buffer for per-triangle additional illumination that is initialized to 0 and changeable via setTriangleAdditionalMeasure.
		//! Illumination defined here overrides original object's illumination.
		class RRObjectAdditionalIllumination* createAdditionalIllumination();

		// collision helper
		//! Creates and returns collision handler, that accepts first hit to visible side (according to surface sideBit 'render').
		RRCollisionHandler* createCollisionHandlerFirstVisible();
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjectAdditionalIllumination
	//! Interface for object importer with user-defined additional per-triangle illumination.
	//
	//! Helper interface.
	//! Instance may be created by RRMesh::createAdditionalIllumination().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRObjectAdditionalIllumination : public RRObject
	{
	public:
		//! Sets additional illumination for triangle.
		//
		//! \param t Index of triangle. Valid t is in range <0..getNumTriangles()-1>.
		//! \param measure Radiometric measure used for power.
		//! \param power Amount of additional illumination for triangle t in units specified by measure.
		//! \returns True on success, false on invalid inputs.
		virtual bool                setTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor power) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRScaler
	//! Interface for 1d space transformer.
	//
	//! RRScaler may be used by RRScene to transform irradiance/emittance/exitance 
	//! between physical W/m^2 space and your user defined space.
	//! It is just helper for your convenience, you may easily stay without RRScaler.
	//! Without scaler, all inputs/outputs work with specified physical units.
	//! With appropriate scaler, you may directly work for example with screen colors
	//! or photometric units.
	//!
	//! RRSurface::diffuseEmittance is never scaled.
	//!
	//! Contains built-in support for standard RGB monitor space, see createRgbScaler().
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRScaler
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		//! Converts standard space (W/m^2) value to scaled space.
		virtual RRReal getScaled(RRReal standard) const = 0;
		//! Converts scaled space value to standard space (W/m^2).
		virtual RRReal getStandard(RRReal scaled) const = 0;
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
	//  RRScene
	//! 3d scene where illumination can be calculated.
	//
	//! %RRScene covers the most important functionality of library -
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
	//! In future versions, it will be possible to have multiple scenes and work
	//! with them independently. For now, this option hasn't been tested.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRScene
	{
	public:
		RRScene();
		~RRScene();

		//////////////////////////////////////////////////////////////////////////////
		//
		// import geometry
		//

		//! Identifier of object in scene.
		typedef       unsigned ObjectHandle;
		//! Inserts object into scene.
		//
		//! For highest performance, stay with low number of possibly big objects 
		//! rather than high number of small ones.
		//! One big object can be created out of many small ones using RRObject::createMultiObject().
		//!
		//! Different objects always belong to different smoothgroups. So with flat wall cut into two objects,
		//! sharp edge will possibly appear between them.
		//! This can be fixed by merging standalone objects into one object using RRObject::createMultiObject().
		//! \param importer
		//!  Object importer that defines object shape and material.
		//! \param smoothMode
		//!  Selects smoothing mode, valid options are: 0,1,2.
		//!  \n 0 = Normal independent smoothing, old. Depends on MAX_SMOOTH_ANGLE.
		//!  \n 1 = Normal independent smoothing, new. Depends on MAX_SMOOTH_ANGLE.
		//!  \n 2 = Smoothing defined by object normals. Faces with the same normal on shared vertex are smoothed.
		//! \return ObjectHandle for accessing created object.
		ObjectHandle  objectCreate(RRObject* importer, unsigned smoothMode=2);
		
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
		//!  geometry and surfaces were not modified. This is especially handy when only primary
		//!  lights move, but objects and materials stay unchanged.
		//! \param resetPropagation
		//!  Both options work well in typical situations, but well choosen value may improve performance.
		//!  \n True: Illumination already propagated using old factors is reset.
		//!     It is faster option when illumination changes 
		//!     significantly -> use after big light movement, light on/off.
		//!  \n False: Illumination already propagated using old factors is preserved for 
		//!     future calculation. It is only updated. It is faster option when illumination 
		//!     changes by small amount -> use after tiny light movement, small color/intensity change.
		//! \returns Calculation state, see Improvement.
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
		//! \returns Calculation state, see Improvement.
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
		//! See also SceneStateU and SceneStateF for list of states, that may have influence
		//! on reading results.
		//! \param object Handle of object you want to get results for.
		//! \param triangle Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param vertex Index of triangle's vertex you want to get results for. Valid vertices are 0, 1, 2.
		//!  For invalid vertex number, average value for whole triangle is taken instead of smoothed value in vertex.
		//! \param measure Measure of illumination you want to get.
		//! \param out For valid inputs, illumination level is stored here. For invalid inputs, nothing is changed.
		//! \returns True when out was successfully filled. False may be caused by invalid inputs.
		bool          getTriangleMeasure(ObjectHandle object, unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRColor& out) const;

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
		//! See also SceneStateU and SceneStateF for list of states, that may have influence
		//! on reading results.
		//! \param object Handle of object you want to get results for.
		//! \param triangle Index of triangle you want to get results for. Valid triangle index is <0..getNumTriangles()-1>.
		//! \param measure Measure of illumination you want to get.
		//! \param callback Your callback that will be called for each triangle's subtriangle.
		//! \param context Value is passed to callback without any modification.
		//! \returns Number of subtriangles processed.
		unsigned      getSubtriangleMeasure(ObjectHandle object, unsigned triangle, RRRadiometricMeasure measure, SubtriangleIlluminationEater* callback, void* context);


		//////////////////////////////////////////////////////////////////////////////
		//
		// misc settings (optional)
		//

		//! Set scaler used by this scene i/o operations. This is option for your convenience. See RRScaler for details.
		void          setScaler(RRScaler* scaler);
		//! Returns scaler used by this scene i/o operations.
		RRScaler*     getScaler();

		//! Identifier of integer scene state.
		enum SceneStateU
		{
			GET_SOURCE,           ///< True = Results from getTriangleMeasure() contain source illumination from objects.
			GET_REFLECTED,        ///< True = Results from getTriangleMeasure() contain reflected illumination calculated by library.
			GET_SMOOTH,           ///< True = Results from getTriangleMeasure() are enhanced by smoothing (only reflected part).
			// following states are for testing only
			USE_CLUSTERS,         ///< For testing only. 0 = no clustering, !0 = use clusters.
			GET_FINAL_GATHER,     ///< For testing only. Results from getTriangleMeasure are enhanced by final gathering.
			FIGHT_NEEDLES,        ///< For testing only. 0 = normal, 1 = try to hide artifacts cause by needle triangles(must be set before objects are created, no slowdown), 2 = as 1 but enhanced quality while reading results (reading may be slow).
			SSU_LAST
		};
		//! Identifier of float scene state.
		enum SceneStateF
		{
			MIN_FEATURE_SIZE,    ///< Smaller features will be smoothed. This could be imagined as kind of blur.
			MAX_SMOOTH_ANGLE,    ///< Smaller angles between faces may be smoothed out. This controls automatic smoothgrouping.
			// following states are for testing only
			SUBDIVISION_SPEED,   ///< For testing only. Speed of subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast. Currently there is no way to read subdivision results back, so let it 0.
			IGNORE_SMALLER_AREA, ///< For testing only. Minimal allowed area of triangle (m^2), smaller triangles are ignored.
			IGNORE_SMALLER_ANGLE,///< For testing only. Minimal allowed angle in triangle (rad), sharper triangles are ignored.
			FIGHT_SMALLER_AREA,  ///< For testing only. Smaller triangles (m^2) will be assimilated when FIGHT_NEEDLES.
			FIGHT_SMALLER_ANGLE, ///< For testing only. Sharper triangles (rad) will be assimilated when FIGHT_NEEDLES.
			SSF_LAST
		};
		//! Reset scene states to default values.
		static void     resetStates();
		//! Return one of integer scene states.
		static unsigned getState(SceneStateU state);
		//! Set one of integer scene states.
		static unsigned setState(SceneStateU state, unsigned value);
		//! Return one of float scene states.
		static RRReal   getStateF(SceneStateF state);
		//! Set one of float scene states.
		static RRReal   setStateF(SceneStateF state, RRReal value);


	private:
		void*         _scene;
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
		static LicenseStatus loadLicense(char* filename);
	};

} // namespace

#endif
