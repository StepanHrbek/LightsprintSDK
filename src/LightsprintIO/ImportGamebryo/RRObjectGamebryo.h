// --------------------------------------------------------------------------
// Lightsprint adapters for Gamebryo scene.
// Copyright (C) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
//! Suitable for scenes loaded from .gsa.
//! \param scene
//!  NiScene with meshes to adapt.
//! \param aborting
//!  Import may be asynchronously aborted by setting *aborting to true.
//! \param emissiveMultiplier
//!  Multiplies emittance in all materials. Default 1 keeps original values.
rr::RRObjects* adaptObjectsFromGamebryo(class NiScene* scene, bool& aborting, float emissiveMultiplier = 1);

//! Creates Lightsprint interface for Gamebryo lights in memory.
//
//! Suitable for scenes loaded from .gsa.
//! \param scene
//!  NiScene with lights to adapt.
rr::RRLights* adaptLightsFromGamebryo(class NiScene* scene);


//! Creates Lightsprint interface for Gamebryo geometry + materials in memory.
//
//! Implemented only for Gamebryo 3.0, suitable for Toolbench plugins.
//! \param serviceManager
//!  You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
//! \param aborting
//!  Import may be asynchronously aborted by setting *aborting to true.
rr::RRObjects* adaptObjectsFromGamebryo(class efd::ServiceManager* serviceManager, bool& aborting);

//! Creates Lightsprint interface for Gamebryo lights in memory.
//
//! Implemented only for Gamebryo 3.0, suitable for Toolbench plugins.
//! \param serviceManager
//!  You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
rr::RRLights* adaptLightsFromGamebryo(efd::ServiceManager* serviceManager);

//! Removes lights from the list, that don't have direct illumination enabled. Lights are not deleted.
void removeLightsWithoutDirectIllumination(rr::RRLights& lights);

//! Creates Lightsprint interface for Gamebryo environment in memory.
//
//! Implemented only for Gamebryo 3.0, suitable for Toolbench plugins.
//! \param serviceManager
//!  You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
rr::RRBuffer* adaptEnvironmentFromGamebryo(class efd::ServiceManager* serviceManager);

//! Creates Lightsprint interface for Gamebryo scene in memory.
//
//! Implemented only for Gamebryo 3.0, suitable for Toolbench plugins.
//! \param serviceManager
//!  You can get ServiceManager* from Emergent::Toolbench::Framework::FrameworkService::ActiveServiceManager::get().
//! \param aborting
//!  Import may be asynchronously aborted by setting *aborting to true.
rr::RRScene* adaptSceneFromGamebryo(class efd::ServiceManager* serviceManager, bool& aborting);


//! Makes it possible to load .gsa scenes from disk via rr::RRScene::RRScene().
void registerLoaderGamebryo();

#endif // RROBJECTGAMEBRYO_H
