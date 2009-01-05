// --------------------------------------------------------------------------
// Creates Lightsprint interface for Quake3 map (.bsp)
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Q3Loader.h"

//! Creates Lightsprint interface for Quake3 map.
//
//! If you use standard Quake3 directory structure and scene was loaded from "foo/bar/maps/map1.bsp",
//! set pathToTextures to "foo/bar/".
//! Parts of path are present in texture names. You may remove them with stripPaths.
rr::RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model, const char* pathToTextures, bool stripPaths, rr::RRBuffer* missingTexture);


#endif
