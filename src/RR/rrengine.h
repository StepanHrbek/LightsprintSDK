#ifndef _RRENGINE_H
#define _RRENGINE_H

#include "rrintersect.h"

#include "surface.h"
typedef Surface  RRSurface;

//////////////////////////////////////////////////////////////////////////////
//
// RRSceneImporter - abstract class for importing your data into RRScene.
//
// Derive to import YOUR geometry and surfaces.
// Can also be used to import data into RRObject.

class RRSceneObjectImporter : virtual public RRObjectImporter
{
public:
	// must not change during object lifetime
	virtual RRSurface*   getSurface(unsigned s) = 0;
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
	typedef       RRHit INTERSECT(RRRay*);
	typedef       OBJECT_HANDLE ENUM_OBJECTS(RRRay*, INTERSECT);
	void          setObjectEnumerator(ENUM_OBJECTS enumerator);
	RRHit         intersect(RRRay* ray);
	
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

#endif
