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
	//! \param filename
	//!  Filename of scene.
	//! \param scale
	//!  If it is .3ds/.obj, geometry is scaled by scale3dsObj.
	//!  .3ds/.obj formats don't contain information about units,
	//!  different files might need different scale to convert to meters.
	//! \param stripPaths
	//!  You can flatten eventual directory structure in texture names
	//!  and search all textures in the same directory as scene file.
	ImportScene(const char* filename, float scale = 1, bool stripPaths = false);
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
