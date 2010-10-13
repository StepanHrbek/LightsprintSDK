// --------------------------------------------------------------------------
// Lightsprint adapters for OBJ scene.
// Copyright (C) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

// OBSOLETED: Assimp contains more complete .obj loader.
//  Still this loader may help learn basics as it is very simple.

#ifndef RROBJECTOBJ_H
#define RROBJECTOBJ_H

#include "Lightsprint/RRScene.h"

//! Creates Lightsprint interface for .OBJ file.
//
//! All sizes are multiplied by scale at load time, use scale to normalize
//! unit to recommended 1 meter. OBJ doesn't specify unit length.
rr::RRObjects* adaptObjectsFromOBJ(const char* filename);

//! Makes it possible to load .obj scenes from disk via rr::RRScene::RRScene().
void registerLoaderOBJ();

#endif
