#ifndef RRENGINE_RRENGINE_H
#define RRENGINE_RRENGINE_H

//////////////////////////////////////////////////////////////////////////////
// RREngine - library for realtime radiosity calculations
//
//////////////////////////////////////////////////////////////////////////////

#include "RRIntersect.h"

namespace rrEngine
{

	#ifdef _MSC_VER
	#pragma comment(lib,"RREngine.lib")
	#endif

	#define RRreal float

	//////////////////////////////////////////////////////////////////////////////
	//
	// material aspects of space and surfaces

	#define C_COMPS 3
	#define C_INDICES 256

	typedef RRreal RRColor[C_COMPS]; // r,g,b 0..1

	enum RREmittance
	{
		areaLight   =0, // svitici plocha
		pointLight  =1, // zdroj uprostred plochy, bod sviti do vsech smeru
		nearLight   =2, // zdroj v point, plocha sviti smerem od pointu
		distantLight=3, // zdroj v -nekonecno*point, plocha sviti smerem point
		nearLight2  =4, // docasne napraseno kvuli dvere.bsp
	};

	struct RRSurface
	{
		unsigned char sides; // 1 if surface is 1-sided, 2 for 2-sided
		RRreal        diffuseReflectance;
		RRColor       diffuseReflectanceColor;
		RRreal        diffuseTransmittance;
		RRColor       diffuseTransmittanceColor;
		RRreal        diffuseEmittance;
		RRColor       diffuseEmittanceColor;
		RREmittance   diffuseEmittanceType;
		//Point3  diffuseEmittancePoint;
		RRreal        specularReflectance;
		RRColor       specularReflectanceColor;
		RRreal        specularReflectanceRoughness;
		RRreal        specularTransmittance;
		RRColor       specularTransmittanceColor;
		RRreal        specularTransmittanceRoughness;
		RRreal        refractionReal;
		RRreal        refractionImaginary;

		RRreal    _rd;//needed when calculating different illumination for different components
		RRreal    _rdcx;
		RRreal    _rdcy;
		RRreal    _ed;//needed by turnLight
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
		virtual RRSurface*   getSurface(unsigned s) = 0;

		// may change during object lifetime
		virtual const float* getWorldMatrix() = 0;
		virtual const float* getInvWorldMatrix() = 0;
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

		// import geometry
		OBJECT_HANDLE objectCreate(RRSceneObjectImporter* importer);
		void          objectDestroy(OBJECT_HANDLE object);

		// get intersection
		typedef       bool INTERSECT(rrIntersect::RRRay*);
		typedef       OBJECT_HANDLE ENUM_OBJECTS(rrIntersect::RRRay*, INTERSECT);
		void          setObjectEnumerator(ENUM_OBJECTS enumerator);
		bool          intersect(rrIntersect::RRRay* ray);
		
		// calculate radiosity
		typedef       bool ENDFUNC(class Scene*);
		void          sceneResetStatic();
		bool          sceneImproveStatic(ENDFUNC endfunc);

		// read results
		float         triangleGetRadiosity(OBJECT_HANDLE object, unsigned triangle, unsigned vertex);

		// misc: misc
		void          compact();
		
		// misc: development
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
		RRSS_LAST
	};

	void          RRResetStates();
	unsigned      RRGetState(RRSceneState state);
	unsigned      RRSetState(RRSceneState state, unsigned value);
	RRreal        RRGetStateF(RRSceneState state);
	RRreal        RRSetStateF(RRSceneState state, RRreal value);


	// INSTANCOVANI
	// umoznit instancovani -> oddelit statickou geometrii a akumulatory
	// komplikace: delat subdivision na vsech instancich?
	//  do max hloubky na geometrii
	//  jen do nutne hlubky na instancich ?

	// DEMA
	// viewer
	// grabber+player

} // namespace

#endif
