// --------------------------------------------------------------------------
// Lightsprint adapters for accesing Gamebryo scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTGAMEBRYO_H
#define RROBJECTGAMEBRYO_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for Gamebryo geometry + materials in memory.
//
//! \param scene
//!  NiScene with meshes to adapt.
//! \param aborting
//!  Import may be asynchronously aborted by setting *aborting to true.
//! \param emissiveMultiplier
//!  Multiplies emittance in all materials. Default 1 keeps original values.
rr::RRObjects* adaptObjectsFromGamebryo(class NiScene* scene, bool& aborting, float emissiveMultiplier = 1);

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
	//! \param aborting
	//!  Import may be asynchronously aborted by setting *aborting to true.
	//! \param emissiveMultiplier
	//!  Multiplies emittance in all materials. Default 1 keeps original values.
	ImportSceneGamebryo(const char* filename, bool initGamebryo, bool& aborting, float emissiveMultiplier = 1);
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
