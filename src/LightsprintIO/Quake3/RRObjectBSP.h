// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for Quake3 map (.bsp).
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "Lightsprint/RRScene.h"
#include "Q3Loader.h"

//! Creates Lightsprint interface for Quake3 map.
//
//! Known limitations: 
//! - does not import lights
//! - does not import curved surfaces
//! - does not import script-based materials
rr::RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model, rr::RRFileLocator* textureLocator);

//! Makes it possible to load .bsp scenes from disk via rr::RRScene::RRScene().
void registerLoaderQuake3();

#endif
