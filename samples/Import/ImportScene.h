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
//! - .obj
//! - .mgf
class ImportScene
{
public:
	//! Loads .dae .3ds .bsp .obj .mgf scene from file.
	//
	//! If it is .3ds/.obj, geometry is scaled by scale3dsObj.
	//! .3ds/.obj formats don't contain information about units, different files might need different scale to convert to meters.
	ImportScene(const char* filename, float scale3dsObj = 1);
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
