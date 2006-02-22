#ifndef RRVISION_RRVISION_H
#define RRVISION_RRVISION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRVision.h
//! RRVision - library for fast global illumination calculations
//! \version 2006.2.23
//!
//! - optimized for speed, usage in interactive environments
//! - progressive refinement with first approximative global illumination after 1ms
//! - automatic mesh optimizations
//! - works with your units (screen colors or radiometry or photometry units or anything else)
//! - display independent, purely numerical API
//!
//! \author Copyright (C) Stepan Hrbek 1999-2006
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// General rules
//
// If not otherwise specified, all inputs must be finite numbers.
// With Inf or NaN, result is undefined.
//
// Parameters that need to be destructed are always destructed by caller.
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
	//! Interface for importing your object into RRScene.
	//
	//! Derive to import YOUR objects into RRScene.
	//!
	//! RRObject -> RRObjectImporter -> RRCollider -> RRMeshImporter
	//! where A -> B means that
	//!  - A has pointer to B
	//!  - note that there is no automatic reference counting in B and no automatic destruction of B from A
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
		class RRAdditionalObjectImporter* createAdditionalExitance();
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
	//! Statistics for scene. Retrieve by scene->getSceneStatistics().
	//
	//! All values are cumulative, most of them only increases.
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
		unsigned numRayTracePhotonHitsNotReceived;
		unsigned numGatherFrontHits;
		unsigned numGatherBackHits;
		unsigned numCallsTriangleMeasureOk;   ///< getTriangleMeasure returned true
		unsigned numCallsTriangleMeasureFail; ///< getTriangleMeasure returned false
		unsigned numIrradianceCacheHits;
		unsigned numIrradianceCacheMisses;
		// numbers of errors
		unsigned numDepthOverflows;        ///< Number of depth overflows in recursive photon tracing. Caused by physically incorrect scenes.
		// amounts of distributed radiance
		RRColor  sumDistribInput;
		RRReal   sumDistribFactorClean;
		RRColor  sumDistribFactorMaterial;
		RRColor  sumDistribOutput;
		// tools
		RRSceneStatistics();               ///< Resets all values to zero.
		void     Reset();                  ///< Resets all values to zero.
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRScene
	//! Base for your RR calculations.
	//
	//! You can create multiple RRScenes and perform independent calculations.
	//! But only serially, code is not thread safe.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRVISION_API RRScene
	{
	public:
		RRScene();
		~RRScene();

		// i/o settings (optional)
		void          setScaler(RRScaler* scaler);

		// import geometry
		typedef       unsigned ObjectHandle;
		ObjectHandle  objectCreate(RRObjectImporter* importer);
		void          objectDestroy(ObjectHandle object);

		// import light (optional)
		void          setSkyLight(RRSkyLight* skyLight);
		
		// calculate radiosity
		enum Improvement
		{
			IMPROVED,       ///< Lighting was improved during this call.
			NOT_IMPROVED,   ///< Although some calculations were done, lighting was not yet improved during this call.
			FINISHED,       ///< Correctly finished calculation (probably no light in scene). Further calls for improvement have no effect.
			INTERNAL_ERROR, ///< Internal error, probably caused by invalid inputs (but should not happen). Further calls for improvement have no effect.
		};
		void          sceneFreezeGeometry(bool yes);
		Improvement   sceneResetStatic(bool resetFactors);
		Improvement   sceneImproveStatic(bool endfunc(void*), void* context);
		RRReal        sceneGetAccuracy();

		// read results
		struct InstantRadiosityPoint
		{
			RRVec3    pos;
			RRVec3    dir;
			RRColor   col;
		};
		bool          getTriangleMeasure(ObjectHandle object, unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRColor& out); // vertex=0..2
		unsigned      getPointRadiosity(unsigned n, InstantRadiosityPoint* point);

		// misc: development
		void          getInfo(char* buf, unsigned type);
		void          getStats(unsigned* faces, RRReal* sourceExitingFlux, unsigned* rays, RRReal* reflectedIncidentFlux) const;
		static RRSceneStatistics* getSceneStatistics();
		void*         getScene();
		void*         getObject(ObjectHandle object);

	private:
		void*         _scene;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// RRStates
	//
	//////////////////////////////////////////////////////////////////////////////

	enum RRSceneState
	{
		RRSS_USE_CLUSTERS,         ///< !0 = use clustering
		RRSSF_SUBDIVISION_SPEED,   ///< speed of subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast
		RRSS_GET_SOURCE,           ///< results from getXxxRadiantExitance contain input emittances
		RRSS_GET_REFLECTED,        ///< results from getXxxRadiantExitance contain additional exitances calculated by radiosity
		RRSS_GET_SMOOTH,           ///< results from getXxxRadiantExitance are enhanced by smoothing
		RRSS_GET_FINAL_GATHER,     ///< results from getXxxRadiantExitance are enhanced by final gather
		RRSSF_MIN_FEATURE_SIZE,    ///< smaller features may be lost
		RRSSF_MAX_SMOOTH_ANGLE,    ///< smaller angles between faces may be smoothed/interpolated
		RRSSF_IGNORE_SMALLER_AREA, ///< minimal allowed area of triangle (m^2), smaller triangles are ignored
		RRSSF_IGNORE_SMALLER_ANGLE,///< minimal allowed angle in triangle (rad), sharper triangles are ignored
		RRSS_FIGHT_NEEDLES,        ///< 0 = normal, 1 = try to hide artifacts cause by needle triangles(must be set before objects are created, no slowdown), 2 = as 1 but enhanced quality while reading results (reading may be slow)
		RRSSF_FIGHT_SMALLER_AREA,  ///< smaller triangles (m^2) will be assimilated when FIGHT_NEEDLES
		RRSSF_FIGHT_SMALLER_ANGLE, ///< sharper triangles (rad) will be assimilated when FIGHT_NEEDLES
		RRSS_LAST
	};

	void     RRVISION_API RRResetStates();
	unsigned RRVISION_API RRGetState(RRSceneState state);
	unsigned RRVISION_API RRSetState(RRSceneState state, unsigned value);
	RRReal   RRVISION_API RRGetStateF(RRSceneState state);
	RRReal   RRVISION_API RRSetStateF(RRSceneState state, RRReal value);


	// -- temporary --
	struct DbgRay
	{
		RRReal eye[3];
		RRReal dir[3];
		RRReal dist;
	};
	#define MAX_DBGRAYS 10000
	extern RRVISION_API DbgRay dbgRay[MAX_DBGRAYS];
	extern RRVISION_API unsigned dbgRays;


	//////////////////////////////////////////////////////////////////////////////
	//
	// License
	//
	//////////////////////////////////////////////////////////////////////////////

	enum LicenseStatus
	{
		VALID,       ///< Valid license.
		EXPIRED,     ///< Expired license.
		WRONG,       ///< Wrong license.
		NO_INET,     ///< No internet connection to verify license.
		UNAVAILABLE, ///< Temporarily unable to verify license, try later.
	};
	LicenseStatus RRVISION_API registerLicense(char* licenseOwner, char* licenseNumber);


} // namespace

#endif
