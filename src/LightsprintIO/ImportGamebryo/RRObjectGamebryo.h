// --------------------------------------------------------------------------
// Lightsprint adapters for accesing Gamebryo scene.
// Copyright (C) Stepan Hrbek, Lightsprint, 2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef RROBJECTGAMEBRYO_H
#define RROBJECTGAMEBRYO_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for Gamebryo geometry + materials in memory.
//
//! \param scene
//!  NiScene with meshes to adapt.
//! \param aborting
//!  It could take up to several seconds for scene with many materials.
//!  It's possible to abort it asynchronously by setting aborting.
rr::RRObjects* adaptObjectsFromGamebryo(class NiScene* scene, bool& aborting);

//! Creates Lightsprint interface for Gamebryo lights in memory.
rr::RRLights* adaptLightsFromGamebryo(class NiScene* scene);

//! Creates Lightsprint interface for Gamebryo scene in .gsa file.
class ImportSceneGamebryo : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Imports scene from .gsa file.
	//! \param filename
	//!  Scene file name.
	//! \param initGamebryo
	//!  True to treat Gamebryo as completely unitialized and initialize/shutdown it.
	//!  To be used when importing .gsa into generic Lightsprint samples.
	//!  \n False to skip initialization/shutdown sequence,
	//!  to be used when running Lightsprint code from Gamebryo app, with Gamebryo already initialized.
	ImportSceneGamebryo(const char* filename, bool initGamebryo, bool& aborting);
	~ImportSceneGamebryo();

	rr::RRObjects* getObjects() {return objects;}
	rr::RRLights* getLights() {return lights;}

protected:
	void updateCastersReceiversCache();

	rr::RRObjects*            objects;
	rr::RRLights*             lights;
	bool                      initGamebryo;
	class NiScene*            pkEntityScene;
};


#endif // RROBJECTGAMEBRYO_H
