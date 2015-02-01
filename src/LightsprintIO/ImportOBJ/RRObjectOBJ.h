// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Importer of .obj scene.
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
rr::RRObjects* adaptObjectsFromOBJ(const rr::RRString& filename);

//! Makes it possible to load .obj scenes from disk via rr::RRScene::RRScene().
void registerLoaderOBJ();

#endif
