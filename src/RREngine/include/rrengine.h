#ifndef _RRENGINE_H
#define _RRENGINE_H

#include "RRIntersect.h"

namespace rrEngine
{

	#ifdef _MSC_VER
	#pragma comment(lib,"RREngine.lib")
	#endif

	//using namespace rrIntersect;
	using rrIntersect::OBJECT_HANDLE;
	#define real float//!!!
	#define U8 unsigned char//!!!

	//////////////////////////////////////////////////////////////////////////////
	//
	// material aspects of space and surfaces

	#define C_COMPS 3
	#define C_INDICES 256

	typedef real RRColor[C_COMPS]; // r,g,b 0..1

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
		U8          sides; // 1 if surface is 1-sided, 2 for 2-sided
		real        diffuseReflectance;
		RRColor     diffuseReflectanceColor;
		real        diffuseTransmittance;
		RRColor     diffuseTransmittanceColor;
		real        diffuseEmittance;
		RRColor     diffuseEmittanceColor;
		RREmittance diffuseEmittanceType;
		//Point3  diffuseEmittancePoint;
		real        specularReflectance;
		RRColor     specularReflectanceColor;
		real        specularReflectanceRoughness;
		real        specularTransmittance;
		RRColor     specularTransmittanceColor;
		real        specularTransmittanceRoughness;
		real        refractionReal;
		real        refractionImaginary;

		real    _rd;//needed when calculating different illumination for different components
		real    _rdcx;
		real    _rdcy;
		real    _ed;//needed by turnLight
	};

	//////////////////////////////////////////////////////////////////////////////
	//
	// surface behaviour bits

	struct SideBits
	{
		U8 renderFrom:1;  // is visible from that halfspace
		U8 emitTo:1;      // emits energy to that halfspace
		U8 catchFrom:1;   // stops rays from that halfspace and performs following operations: (otherwise ray continues as if nothing happened)
		U8 receiveFrom:1; //  receives energy from that halfspace
		U8 reflect:1;     //  reflects energy from that halfspace to that halfspace
		U8 transmitFrom:1;//  transmits energy from that halfspace to other halfspace
	};

	extern SideBits sideBits[3][2];


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

		// import geometry
		OBJECT_HANDLE objectCreate(RRSceneObjectImporter* importer);
		void          objectDestroy(OBJECT_HANDLE object);

		// get intersection
		typedef       rrIntersect::RRHit INTERSECT(rrIntersect::RRRay*);
		typedef       OBJECT_HANDLE ENUM_OBJECTS(rrIntersect::RRRay*, INTERSECT);
		void          setObjectEnumerator(ENUM_OBJECTS enumerator);
		rrIntersect::RRHit         intersect(rrIntersect::RRRay* ray);
		
		// calculate radiosity
		typedef       bool ENDFUNC(class Scene*);
		void          sceneResetStatic();
		bool          sceneImproveStatic(ENDFUNC endfunc);

		// read results
		float         triangleGetRadiosity(OBJECT_HANDLE object, unsigned triangle, unsigned vertex);

		// misc
		void          compact();
		
		// development
		void*         getScene();
		void*         getObject(OBJECT_HANDLE object);

	private:
		void*         _scene;
	};

	// BSP+INTERSECT
	// kam je dat?

	// scenare pouziti
	// 1. pocitat radiositu (tixe 3d, rr, plugin na predpocet radiosity)
	// 2. pocitat radiositu a intersecty (editor, hra s rr)
	// 3. pocitat intersecty (hra bez rr)

	// oddelena knihovna RRIntersect
	// RRScene ji zavola pokud bude chtit
	// hra pouzije jenom ji pokud bude chtit


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
