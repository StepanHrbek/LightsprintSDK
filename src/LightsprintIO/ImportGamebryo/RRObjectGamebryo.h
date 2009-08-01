// --------------------------------------------------------------------------
// Lightsprint adapters for Gamebryo scene.
// Copyright (C) 2008-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTGAMEBRYO_H
#define RROBJECTGAMEBRYO_H

#include "Lightsprint/RRScene.h"

namespace efd
{
	class ServiceManager;
}

//! Creates Lightsprint interface for Gamebryo geometry + materials in memory.
//
//! \param scene
//!  NiScene with meshes to adapt.
//! \param aborting
//!  Import may be asynchronously aborted by setting *aborting to true.
//! \param emissiveMultiplier
//!  Multiplies emittance in all materials. Default 1 keeps original values.
rr::RRObjects* adaptObjectsFromGamebryo(class NiScene* scene, bool& aborting, float emissiveMultiplier = 1);
//! Creates Lightsprint interface for Gamebryo geometry + materials in memory.
//
//! Implemented only for Gamebryo 3.0.
//! Accepts ServiceManager* instead of NiScene*, suitable for Toolbench plugins.
//! You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
rr::RRObjects* adaptObjectsFromGamebryo(class efd::ServiceManager* serviceManager, bool& aborting, float emissiveMultiplier);

//! Creates Lightsprint interface for Gamebryo lights in memory.
rr::RRLights* adaptLightsFromGamebryo(class NiScene* scene);
//! Creates Lightsprint interface for Gamebryo lights in memory.
//
//! Implemented only for Gamebryo 3.0.
//! Accepts ServiceManager* instead of NiScene*, suitable for Toolbench plugins.
//! You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
rr::RRLights* adaptLightsFromGamebryo(efd::ServiceManager* serviceManager);

//! Creates scene in toolbench.
//
//! Implemented only for Gamebryo 3.0.
//! Accepts ServiceManager* instead of NiScene*, suitable for Toolbench plugins.
//! You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
rr::RRScene* adaptSceneFromGamebryo(class efd::ServiceManager* serviceManager, bool initGamebryo, bool& aborting, float emissiveMultiplier = 1);

//! Makes it possible to load .gsa scenes from disk via rr::RRScene::RRScene().
void registerLoaderGamebryo();

#endif // RROBJECTGAMEBRYO_H
