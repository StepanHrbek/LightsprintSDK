#ifndef RRVISION_RRVISION_H
#define RRVISION_RRVISION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVision.h
//! \brief RRVision - library for fast global illumination calculations
//! \version 2006.2.26
//! \author Copyright (C) Stepan Hrbek 1999-2006
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRVISION_STATIC
		// use static library
		#define RRVISION_API
		#ifdef NDEBUG
			#pragma comment(lib,"RRVision_s.lib")
		#else
			#pragma comment(lib,"RRVision_sd.lib")
		#endif
	#else
	#ifdef RRVISION_DLL_BUILD
		// build dll
		#define RRVISION_API __declspec(dllexport)
	#else
		// use dll
		#define RRVISION_API __declspec(dllimport)
		#pragma comment(lib,"RRVision.lib")
	#endif
	#endif
#else
	// use static library
	#define RRVISION_API
#endif

#include "RRCollider.h"

namespace rrVision /// Encapsulates whole RRVision library.
{

	//////////////////////////////////////////////////////////////////////////////
	//
	// primitives
	//
	// RRReal      - real number
	// RRVec2      - vector in 2d
	// RRVec3      - vector in 3d
	// RRMatrix4x4 - matrix 4x4
	//
	//////////////////////////////////////////////////////////////////////////////

	//! Real number used in most of calculations.
	typedef rrCollider::RRReal RRReal;

	//! Vector of 2 real numbers.
	typedef rrCollider::RRVec2 RRVec2;

	//! Vector of 3 real numbers.
	typedef rrCollider::RRVec3 RRVec3;

	//! Matrix of 4x4 real numbers.
	struct RRVISION_API RRMatrix4x4
	{
		RRReal m[4][4];

		RRVec3 transformed(const RRVec3& a) const;
		RRVec3& transform(RRVec3& a) const;
		RRVec3 rotated(const RRVec3& a) const;
		RRVec3& rotate(RRVec3& a) const;
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

	enum RRRadiometricMeasure
	{
		RM_INCIDENT_FLUX, ///< incoming W
		RM_IRRADIANCE,    ///< incoming W/m^2
		RM_EXITING_FLUX,  ///< outgoing W
		RM_EXITANCE,      ///< outgoing W/m^2
	};

	enum RREmittanceType
	{
		diffuseLight=0, ///< face emitting like ideal diffuse emitor
		pointLight  =1, ///< point P emitting equally to all directions (P=diffuseEmittancePoint)
		spotLight   =2, ///< face emitting only in direction from point P
		dirLight    =3, ///< face emiting only in direction P
	};

	//! Boolean attributes of front or back side of surface.
	struct RRSideBits
	{
		unsigned char renderFrom:1;  ///< is visible from that halfspace
		unsigned char emitTo:1;      ///< emits energy to that halfspace
		unsigned char catchFrom:1;   ///< stops rays from that halfspace and performs following operations: (otherwise ray continues as if nothing happened)
		unsigned char receiveFrom:1; ///<  receives energy from that halfspace
		unsigned char reflect:1;     ///<  reflects energy from that halfspace to that halfspace
		unsigned char transmitFrom:1;///<  transmits energy from that halfspace to other halfspace
	};

	//! Description of surface.
	struct RRSurface
	{
		RRSideBits    sideBits[2];                   ///< defines surface behaviour for front(0) and back(1) side
		RRReal        diffuseReflectance;            ///< Fraction of energy that is diffuse reflected (all channels averaged).
		RRColor       diffuseReflectanceColor;       ///< Fraction of energy that is diffuse reflected (each channel separately).
		RRReal        diffuseTransmittance;          ///< Currently not used.
		RRColor       diffuseTransmittanceColor;     ///< Currently not used.
		RRReal        diffuseEmittance;              ///< \ Multiplied = Radiant emittance in watts per square meter. Never scaled by RRScaler.
		RRColor       diffuseEmittanceColor;         ///< / 
		RREmittanceType emittanceType;
		RRVec3        emittancePoint;
		RRReal        specularReflectance;           ///< Fraction of energy that is mirror reflected (without color change).
		RRColor       specularReflectanceColor;      ///< Currently not used.
		RRReal        specularReflectanceRoughness;  ///< Currently not used.
		RRReal        specularTransmittance;         ///< Fraction of energy that is transmitted (without color change).
		RRColor       specularTransmittanceColor;    ///< Currently not used.
		RRReal        specularTransmittanceRoughness;///< Currently not used.
		RRReal        refractionReal;
		RRReal        refractionImaginary;           ///< Currently not used.

		RRReal        _ed;                           ///< For internal use.
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRObjectImporter
	//! Common interface for all proprietary object formats.
	//
	//! \section s1 Provided information
	//! #RRObjectImporter provides information about
	//! - object material properties
	//! - object collider for fast ray-mesh intersections
	//! - indirectly also object geometry (via getCollider()->getImporter())
	//! - optionally object transformation matrix
	//! - optionally unwrap (for future versions)
	//!
	//! \section s2 Creating instances
	//! The only way to create first instance is to derive from #RRObjectImporter.
	//! This is caused by lack of standard material description formats,
	//! everyone uses different description and needs his own object importer.
	//!
	//! Once you have object importers, there is built-in support for 
	//! - pretransforming mesh, see createWorldSpaceMesh()
	//! - baking multiple objects together, see createMultiObject()
	//! - baking additional (primary) illumination into object, see createAdditionalIllumination()
	//!
	//! \section s3 Links between objects
	//! /n RRObject -> RRObjectImporter -> RRCollider -> RRMeshImporter
	//! /n where A -> B means that
	//!  - A has pointer to B
	//!  - there is no automatic reference counting in B and no automatic destruction of B from A
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRObjectImporter
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual ~RRObjectImporter() {}

		// must not change during object lifetime
		virtual const rrCollider::RRCollider* getCollider() const = 0;
		virtual unsigned            getTriangleSurface(unsigned t) const = 0;
		virtual const RRSurface*    getSurface(unsigned s) const = 0;
		// optional
		struct TriangleNormals      {RRVec3 norm[3];}; ///< 3x normal in object space
		struct TriangleMapping      {RRVec2 uv[3];}; ///< 3x uv
		virtual void                getTriangleNormals(unsigned t, TriangleNormals& out); ///< normalized vertex normals in local space
		virtual void                getTriangleMapping(unsigned t, TriangleMapping& out); ///< unwrap into 0..1 x 0..1 space
		virtual void                getTriangleAdditionalMeasure(unsigned t, RRRadiometricMeasure measure, RRColor& out) const;

		// may change during object lifetime
		virtual const RRMatrix4x4*  getWorldMatrix() = 0; ///< may return identity as NULL 
		virtual const RRMatrix4x4*  getInvWorldMatrix() = 0; ///< may return identity as NULL 


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		rrCollider::RRMeshImporter* createWorldSpaceMesh();
		static RRObjectImporter*    createMultiObject(RRObjectImporter* const* objects, unsigned numObjects, rrCollider::RRCollider::IntersectTechnique intersectTechnique, float maxStitchDistance, char* cacheLocation);
		class RRAdditionalObjectImporter* createAdditionalIllumination();
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRAdditionalObjectImporter
	//! Helper interface
	//
	//! RRObjectImporter copy with set/get-able additional per-face exitance 
	//! (overrides exitance of original)
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRAdditionalObjectImporter : public RRObjectImporter
	{
	public:
		virtual bool                setTriangleAdditionalPower(unsigned t, RRRadiometricMeasure measure, RRColor power) = 0;
	};


#ifdef RR_DEVELOPMENT
	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRSkyLight
	//! Interface that defines your skylight.
	//
	//! Derive to import YOUR skylight
	//! into RRScene.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRSkyLight
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual RRColor getRadiance(RRVec3 dirFromSky) const = 0;
		virtual ~RRSkyLight() {}
	};
#endif


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
	//! Note that RRSurface::diffuseEmittance is never scaled.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRScaler
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		virtual RRReal getScaled(RRReal original) const = 0;
		virtual RRReal getOriginal(RRReal scaled) const = 0;
		virtual ~RRScaler() {}


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// instance factory
		static RRScaler* createGammaScaler(RRReal gamma);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRSceneStatistics
	//! Statistics for scene.
	//
	//! Statistics reflect inner states of radiosity solver and may 
	//! arbitrarily change when solver changes in future versions.
	//!
	//! Retrieve them by scene->getSceneStatistics().
	//! Currently all scenes work with the same statistics.
	//!
	//! All values are cumulative, most of them only increase.
	//! You should manually reset them to zero at the beginning of measurement.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRSceneStatistics
	{
	public:
		// numbers of calls
		unsigned numCallsImprove;
		unsigned numCallsBest;
		unsigned numCallsRefreshFactors;
		unsigned numCallsDistribFactors;
		unsigned numCallsDistribFactor;
		unsigned numRayTracePhotonFrontHits;
		unsigned numRayTracePhotonBackHits;
		//unsigned numRayTracePhotonHitsNotReceived;
		unsigned numRayTracePhotonHitsReceived;
		unsigned numRayTracePhotonHitsReflected;
		unsigned numRayTracePhotonHitsTransmitted;
		unsigned numHitInserts;
		unsigned numGatherFrontHits;
		unsigned numGatherBackHits;
		unsigned numCallsTriangleMeasureOk;   ///< getTriangleMeasure returned true
		unsigned numCallsTriangleMeasureFail; ///< getTriangleMeasure returned false
		unsigned numIrradianceCacheHits;
		unsigned numIrradianceCacheMisses;
		// numbers of errors
		unsigned numDepthOverflows;        ///< Number of depth overflows in recursive photon tracing. Caused by physically incorrect scenes.
		// amounts of energy
		//RRReal   sumRayTracePhotonHitPower;
		//RRReal   sumRayTracePhotonDifRefl;
		//RRColor  sumDistribInput;
		//RRReal   sumDistribFactorClean;
		//RRColor  sumDistribFactorMaterial;
		//RRColor  sumDistribOutput;
		// photon traces
		struct LineSegment {RRVec3 point[2];};
		enum { MAX_LINES = 10000 };
		unsigned numLineSegments;
		LineSegment lineSegments[MAX_LINES];
		// tools
		RRSceneStatistics();               ///< Resets all values to zero.
		void     Reset();                  ///< Resets all values to zero.
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRScene
	//! Description of your scene and radiosity solver.
	//
	//! To be written.
	//! 
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRScene
	{
	public:
		RRScene();
		~RRScene();

		//! Identifier of integer scene state.
		enum SceneStateU
		{
			RRSS_GET_SOURCE,           ///< Results from getTriangleMeasure contain source illumination from objects.
			RRSS_GET_REFLECTED,        ///< Results from getTriangleMeasure contain reflected illumination calculated by library.
			RRSS_GET_SMOOTH,           ///< Results from getTriangleMeasure are enhanced by smoothing (only reflected part).
			// following states are development only
			RRSS_USE_CLUSTERS,         ///< For testing only. 0 = no clustering, !0 = use clustering.
			RRSS_GET_FINAL_GATHER,     ///< For testing only. Results from getTriangleMeasure are enhanced by final gathering.
			RRSS_FIGHT_NEEDLES,        ///< For testing only. 0 = normal, 1 = try to hide artifacts cause by needle triangles(must be set before objects are created, no slowdown), 2 = as 1 but enhanced quality while reading results (reading may be slow).
			RRSS_LAST
		};
		//! Identifier of float scene state.
		enum SceneStateF
		{
			RRSSF_MIN_FEATURE_SIZE,    ///< Smaller features will be smoothed. This is kind of blur.
			RRSSF_MAX_SMOOTH_ANGLE,    ///< Smaller angles between faces may be smoothed/interpolated.
			// following states are development only
			RRSSF_SUBDIVISION_SPEED,   ///< For testing only. Speed of subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast. Currently there is no way to read subdivision results back, so let it 0.
			RRSSF_IGNORE_SMALLER_AREA, ///< For testing only. Minimal allowed area of triangle (m^2), smaller triangles are ignored.
			RRSSF_IGNORE_SMALLER_ANGLE,///< For testing only. Minimal allowed angle in triangle (rad), sharper triangles are ignored.
			RRSSF_FIGHT_SMALLER_AREA,  ///< For testing only. Smaller triangles (m^2) will be assimilated when FIGHT_NEEDLES.
			RRSSF_FIGHT_SMALLER_ANGLE, ///< For testing only. Sharper triangles (rad) will be assimilated when FIGHT_NEEDLES.
			RRSSF_LAST
		};
		static void     ResetStates();                               ///< Reset scene states to default values.
		static unsigned GetState(SceneStateU state);                 ///< Return one of integer scene states.
		static unsigned SetState(SceneStateU state, unsigned value); ///< Set one of integer scene states.
		static RRReal   GetStateF(SceneStateF state);                ///< Return one of float scene states.
		static RRReal   SetStateF(SceneStateF state, RRReal value);  ///< Set one of float scene states.

		// i/o settings (optional)
		void          setScaler(RRScaler* scaler);                   ///< Set scaler used by this scene i/o operations.

		// import geometry
		typedef       unsigned ObjectHandle;
		ObjectHandle  objectCreate(RRObjectImporter* importer);      ///< Insert object into scene.
#ifdef RR_DEVELOPMENT
		void          objectDestroy(ObjectHandle object);

		// import light (optional)
		void          setSkyLight(RRSkyLight* skyLight);
#endif
		
		// calculate radiosity
		enum Improvement
		{
			IMPROVED,       ///< Lighting was improved during this call.
			NOT_IMPROVED,   ///< Although some calculations were done, lighting was not yet improved during this call.
			FINISHED,       ///< Correctly finished calculation (probably no light in scene). Further calls for improvement have no effect.
			INTERNAL_ERROR, ///< Internal error, probably caused by invalid inputs (but should not happen). Further calls for improvement have no effect.
		};
#ifdef RR_DEVELOPMENT
		void          sceneFreezeGeometry(bool yes);
#endif
		Improvement   illuminationReset(bool resetFactors);                    ///< Reset illumination to original state.
		Improvement   illuminationImprove(bool endfunc(void*), void* context); ///< Improve illumination until endfunc returns true.
		RRReal        illuminationAccuracy();                                  ///< Return illumination accuracy in proprietary scene dependant units. Higher is more accurate.

		// read results
#ifdef RR_DEVELOPMENT
		struct InstantRadiosityPoint
		{
			RRVec3    pos;
			RRVec3    dir;
			RRColor   col;
		};
		unsigned      getPointRadiosity(unsigned n, InstantRadiosityPoint* point);
#endif
		bool          getTriangleMeasure(ObjectHandle object, unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRColor& out); ///< Return calculated illumination measure for triangle=0..getNumTriangles and vertex=0..2.

		// misc: development
		static RRSceneStatistics* getSceneStatistics(); ///< Return pointer to scene statistics. Currently there are only one global statistics for all scenes.
#ifdef RR_DEVELOPMENT
		void          getInfo(char* buf, unsigned type);
		void          getStats(unsigned* faces, RRReal* sourceExitingFlux, unsigned* rays, RRReal* reflectedIncidentFlux) const;
		void*         getScene();
		void*         getObject(ObjectHandle object);
#endif

	private:
		void*         _scene;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRLicense
	//! Provide your license number before any other work with library.
	//
	//! Computer must be connected to internet so license number
	//! may be verified.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRLicense
	{
	public:
		enum LicenseStatus
		{
			VALID,       ///< Valid license.
			EXPIRED,     ///< Expired license.
			WRONG,       ///< Wrong license.
			NO_INET,     ///< No internet connection to verify license.
			UNAVAILABLE, ///< Temporarily unable to verify license, try later.
		};
		static LicenseStatus registerLicense(char* licenseOwner, char* licenseNumber);
	};

} // namespace

#endif
