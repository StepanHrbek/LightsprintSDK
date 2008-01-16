#ifndef IMPORTSCENE_H
#define IMPORTSCENE_H

#include "Lightsprint/RRDynamicSolver.h"

/////////////////////////////////////////////////////////////////////////////
//
// Scene

//! Loads scene from file. Supported formats:
//! - .dae (collada)
//! - .3ds
//! - .bsp (quake3)
//! - .mgf
class ImportScene
{
public:
	//! Loads .dae .3ds .bsp .mgf scene from file.
	//
	//! If it is .3ds, geometry is scaled by scale3ds.
	//! .3ds format doesn't contain information about units, different files might need different scale to convert to meters.
	ImportScene(const char* filename, float scale3ds = 1);
	~ImportScene();

	const rr::RRObjects* getObjects() {return objects;}
	const rr::RRLights* getLights() {return lights;}

protected:
	rr::RRObjects*    objects;
	rr::RRLights*     lights;
	class Model_3DS*  scene_3ds;
	struct TMapQ3*    scene_bsp;
	class FCDocument* scene_dae;
};

#endif
