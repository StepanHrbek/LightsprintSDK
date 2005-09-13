#ifndef RRENGINE_RRENGINE_H
#define RRENGINE_RRENGINE_H

//////////////////////////////////////////////////////////////////////////////
// RREngine - library for realtime radiosity calculations
// version 2005.09.13
// http://dee.cz/rr
//
// Copyright (C) Stepan Hrbek 1999-2005
// This work is protected by copyright law, 
// using it without written permission from Stepan Hrbek is forbidden.
//////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#ifdef RRENGINE_EXPORT
		// build dll
		#define RRENGINE_API __declspec(dllexport)
	#elif RRENGINE_IMPORT
		// use dll
		#define RRENGINE_API __declspec(dllimport)
		#pragma comment(lib,"RREngine.lib")
	#else
		// use static library
		#define RRENGINE_API
		#pragma comment(lib,"RREngine.lib")
	#endif
#else
	// use static library
	#define RRENGINE_API
#endif

#include "RRCollider.h"

namespace rrEngine
{

	typedef rrCollider::RRReal RRReal;


	//////////////////////////////////////////////////////////////////////////////
	//
	// material aspects of space and surfaces

	#define C_COMPS 3
	#define C_INDICES 256

	typedef RRReal RRColor[C_COMPS]; // r,g,b 0..1

	enum RREmittanceType
	{
		diffuseLight=0, // face emitting like ideal diffuse emitor
		pointLight  =1, // point P emitting equally to all directions (P=diffuseEmittancePoint)
		spotLight   =2, // face emitting only in direction from point P
		dirLight    =3, // face emiting only in direction P
	};

	struct RRSurface
	{
		unsigned char sides; // 1 if surface is 1-sided, 2 for 2-sided
		RRReal        diffuseReflectance;            // Fraction of energy that is diffuse reflected (all channels averaged).
		RRColor       diffuseReflectanceColor;       // Fraction of energy that is diffuse reflected (each channel separately).
		RRReal        diffuseTransmittance;          // Currently not used.
		RRColor       diffuseTransmittanceColor;     // Currently not used.
		RRReal        diffuseEmittance;              // \ Multiplied = Radiant emittance in watts per square meter.
		RRColor       diffuseEmittanceColor;         // / 
		RREmittanceType emittanceType;
		RRReal        emittancePoint[3];
		RRReal        specularReflectance;           // Fraction of energy that is mirror reflected (without color change).
		RRColor       specularReflectanceColor;      // Currently not used.
		RRReal        specularReflectanceRoughness;  // Currently not used.
		RRReal        specularTransmittance;         // Fraction of energy that is transmitted (without color change).
		RRColor       specularTransmittanceColor;    // Currently not used.
		RRReal        specularTransmittanceRoughness;// Currently not used.
		RRReal        refractionReal;
		RRReal        refractionImaginary;           // Currently not used.

		RRReal    _rd;//needed when calculating different illumination for different components
		RRReal    _rdcx;
		RRReal    _rdcy;
		RRReal    _ed;//needed by turnLight
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// surface behaviour bits

	struct RRSideBits
	{
		unsigned char renderFrom:1;  // is visible from that halfspace
		unsigned char emitTo:1;      // emits energy to that halfspace
		unsigned char catchFrom:1;   // stops rays from that halfspace and performs following operations: (otherwise ray continues as if nothing happened)
		unsigned char receiveFrom:1; //  receives energy from that halfspace
		unsigned char reflect:1;     //  reflects energy from that halfspace to that halfspace
		unsigned char transmitFrom:1;//  transmits energy from that halfspace to other halfspace
	};

	extern RRSideBits sideBits[3][2];

	//////////////////////////////////////////////////////////////////////////////
	//
	// RRObjectImporter - abstract class for importing your object into RRScene.
	//
	// Derive to import YOUR objects into RRScene.
	//
	// RRObject -> RRObjectImporter -> RRCollider -> RRMeshImporter
	// where A -> B means that
	//  - A has pointer to B
	//  - note that there is no automatic reference counting in B and no automatic destruction of B from A

	class RRENGINE_API RRObjectImporter
	{
	public:
		virtual ~RRObjectImporter() {}

		// must not change during object lifetime
		virtual const rrCollider::RRCollider* getCollider() const = 0;
		virtual unsigned     getTriangleSurface(unsigned t) const = 0;
		virtual RRSurface*   getSurface(unsigned s) = 0;
		virtual const RRReal* getTriangleAdditionalRadiantExitance(unsigned t) const {return 0;} // radiant exitance in watts per square meter. 
		virtual const RRReal* getTriangleAdditionalRadiantExitingFlux(unsigned t) const {return 0;} // radiant flux in watts. implement only one of these two methods.

		// may change during object lifetime
		virtual const RRReal* getWorldMatrix() = 0;
		virtual const RRReal* getInvWorldMatrix() = 0;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// RRScene - base for your RR calculations.
	//
	// You can create multiple RRScenes and perform independent calculations.
	// But only serially, code is not thread safe.

	class RRENGINE_API RRScene
	{
	public:
		RRScene();
		~RRScene();

		// import geometry
		typedef       unsigned ObjectHandle;
		ObjectHandle  objectCreate(RRObjectImporter* importer);
		void          objectDestroy(ObjectHandle object);

		// get intersection
		typedef       bool INTERSECT(rrCollider::RRRay*);
		typedef       ObjectHandle ENUM_OBJECTS(rrCollider::RRRay*, INTERSECT);
		void          setObjectEnumerator(ENUM_OBJECTS enumerator);
		bool          intersect(rrCollider::RRRay* ray);
		
		// calculate radiosity
		enum Improvement 
		{
			IMPROVED,       // Lighting was improved during this call.
			NOT_IMPROVED,   // Although some calculations were done, lighting was not yet improved during this call.
			FINISHED,       // Correctly finished calculation (probably no light in scene). Further calls for improvement have no effect.
			INTERNAL_ERROR, // Internal error, probably caused by invalid inputs (but should not happen). Further calls for improvement have no effect.
		};
		void          sceneSetColorFilter(const RRReal* colorFilter);
		Improvement   sceneResetStatic(bool resetFactors);
		Improvement   sceneImproveStatic(bool endfunc(void*), void* context);

		// read results
		struct InstantRadiosityPoint
		{
			RRReal pos[3];
			RRReal norm[3];
			RRReal col[3];
		};
		const RRReal* getVertexIrradiance(ObjectHandle object, unsigned vertex); // irradiance (incident power density) in watts per square meter
		const RRReal* getTriangleIrradiance(ObjectHandle object, unsigned triangle, unsigned vertex); // irradiance (incident power density) in watts per square meter
		const RRReal* getTriangleRadiantExitance(ObjectHandle object, unsigned triangle, unsigned vertex); // radiant exitance (leaving power density) in watts per square meter
		unsigned      getPointRadiosity(unsigned n, InstantRadiosityPoint* point);

		// misc
		void          compact();
		
		// misc: development
		void          getInfo(char* buf, unsigned type);
		void          getStats(unsigned* faces, RRReal* sourceExitingFlux, unsigned* rays, RRReal* reflectedIncidentFlux) const;
		void*         getScene();
		void*         getObject(ObjectHandle object);

	private:
		void*         _scene;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// RRStates

	enum RRSceneState
	{
		RRSS_USE_CLUSTERS,         // !0 = use clustering
		RRSSF_SUBDIVISION_SPEED,   // speed of subdivision, 0=no subdivision, 0.3=slow, 1=standard, 3=fast
		RRSS_GET_SOURCE,           // results from getXxxRadiantExitance contain input emittances
		RRSS_GET_REFLECTED,        // results from getXxxRadiantExitance contain additional exitances calculated by radiosity
		RRSS_INTERSECT_TECHNIQUE,  // IT_XXX, 0=most compact, 4=fastest
		RRSSF_IGNORE_SMALLER_AREA, // minimal allowed area of triangle (m^2), smaller triangles are ignored
		RRSSF_IGNORE_SMALLER_ANGLE,// minimal allowed angle in triangle (rad), sharper triangles are ignored
		RRSS_FIGHT_NEEDLES,        // 0 = normal, 1 = try to hide artifacts cause by needle triangles(must be set before objects are created, no slowdown), 2 = as 1 but enhanced quality while reading results (reading may be slow)
		RRSSF_FIGHT_SMALLER_AREA,  // smaller triangles (m^2) will be assimilated when FIGHT_NEEDLES
		RRSSF_FIGHT_SMALLER_ANGLE, // sharper triangles (rad) will be assimilated when FIGHT_NEEDLES
		// statistics
		RRSS_IMPROVE_CALLS,
		RRSS_BESTS,
		RRSS_DISTRIBS,
		RRSS_REFRESHES,
		RRSS_DEPTH_OVERFLOWS,    // accumulated number of depth overflows in photon tracing, caused by physically incorrect scenes
		RRSS_LAST
	};

	void     RRENGINE_API RRResetStates();
	unsigned RRENGINE_API RRGetState(RRSceneState state);
	unsigned RRENGINE_API RRSetState(RRSceneState state, unsigned value);
	RRReal   RRENGINE_API RRGetStateF(RRSceneState state);
	RRReal   RRENGINE_API RRSetStateF(RRSceneState state, RRReal value);


	// -- temporary --
	struct DbgRay
	{
		RRReal eye[3];
		RRReal dir[3];
		RRReal dist;
	};
	#define MAX_DBGRAYS 10000
	extern DbgRay dbgRay[MAX_DBGRAYS];
	extern unsigned dbgRays;

} // namespace

#endif
