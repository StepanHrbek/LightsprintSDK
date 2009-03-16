// --------------------------------------------------------------------------
// Lightsprint adapters for Gamebryo scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTGAMEBRYO_H
#define RROBJECTGAMEBRYO_H

#include "Lightsprint/RRScene.h"

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

//! Makes it possible to load .gsa scenes from disk via rr::RRScene::RRScene().
void registerLoaderGamebryo();

#endif // RROBJECTGAMEBRYO_H
