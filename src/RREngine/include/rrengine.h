#ifndef RRENGINE_RRENGINE_H
#define RRENGINE_RRENGINE_H

//////////////////////////////////////////////////////////////////////////////
// RREngine - library for realtime radiosity calculations
// version 2005.05.10
// http://dee.cz/rr
//
// Copyright (C) Stepan Hrbek 1999-2005
// This work is protected by copyright law, 
// using it without written permission from Stepan Hrbek is forbidden.
//////////////////////////////////////////////////////////////////////////////

#include "RRIntersect.h"

namespace rrEngine
{
	#ifdef _MSC_VER
	#pragma comment(lib,"RREngine.lib")
	#endif

	typedef rrIntersect::RRReal RRReal;


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
		RRReal        diffuseReflectance;
		RRColor       diffuseReflectanceColor;
		RRReal        diffuseTransmittance;
		RRColor       diffuseTransmittanceColor;
		RRReal        diffuseEmittance;
		RRColor       diffuseEmittanceColor;
		RREmittanceType emittanceType;
		RRReal        emittancePoint[3];
		RRReal        specularReflectance;
		RRColor       specularReflectanceColor;
		RRReal        specularReflectanceRoughness;
		RRReal        specularTransmittance;
		RRColor       specularTransmittanceColor;
		RRReal        specularTransmittanceRoughness;
		RRReal        refractionReal;
		RRReal        refractionImaginary;

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
	// RRSceneImporter - abstract class for importing your data into RRScene.
	//
	// Derive to import YOUR geometry and surfaces.
	// Can also be used to import data into RRObject.

	class RRSceneObjectImporter : virtual public rrIntersect::RRObjectImporter
	{
	public:
		// must not change during object lifetime
		virtual unsigned     getTriangleSurface(unsigned t) const = 0;
		virtual RRSurface*   getSurface(unsigned s) = 0;
		virtual RRReal       getTriangleAdditionalEnergy(unsigned t) const = 0;

		// may change during object lifetime
		virtual const RRReal* getWorldMatrix() = 0;
		virtual const RRReal* getInvWorldMatrix() = 0;

		virtual ~RRSceneObjectImporter() {};
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// RRScene - base for your RR calculations.
	//
	// You can create multiple RRScenes and perform independent calculations.
	// But only serially, code is not thread safe.

	class RRScene
	{
	public:
		RRScene();
		~RRScene();

		typedef unsigned OBJECT_HANDLE;
		struct InstantRadiosityPoint
		{
			RRReal pos[3];
			RRReal norm[3];
			RRReal col[3];
		};

		// import geometry
		OBJECT_HANDLE objectCreate(RRSceneObjectImporter* importer);
		void          objectDestroy(OBJECT_HANDLE object);

		// get intersection
		typedef       bool INTERSECT(rrIntersect::RRRay*);
		typedef       OBJECT_HANDLE ENUM_OBJECTS(rrIntersect::RRRay*, INTERSECT);
		void          setObjectEnumerator(ENUM_OBJECTS enumerator);
		bool          intersect(rrIntersect::RRRay* ray);
		
		// calculate radiosity
		typedef       bool ENDFUNC(void*);
		void          sceneResetStatic();
		bool          sceneImproveStatic(ENDFUNC endfunc);

		// read results
		RRReal        getVertexRadiosity(OBJECT_HANDLE object, unsigned vertex);
		RRReal        getTriangleRadiosity(OBJECT_HANDLE object, unsigned triangle, unsigned vertex);
		unsigned      getPointRadiosity(unsigned n, InstantRadiosityPoint* point);

		// misc: misc
		void          compact();
		
		// misc: development
		void          getInfo(char* buf, unsigned type);
		void*         getScene();
		void*         getObject(OBJECT_HANDLE object);

	private:
		void*         _scene;
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// RRStates

	enum RRSceneState
	{
		RRSS_USE_CLUSTERS,
		RRSS_FIGHT_NEEDLES,
		RRSS_NEEDLE,
		RRSSF_SUBDIVISION_SPEED,
		RRSS_GET_SOURCE,
		RRSS_GET_REFLECTED,
		RRSS_LAST
	};

	void          RRResetStates();
	unsigned      RRGetState(RRSceneState state);
	unsigned      RRSetState(RRSceneState state, unsigned value);
	RRReal        RRGetStateF(RRSceneState state);
	RRReal        RRSetStateF(RRSceneState state, RRReal value);


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
