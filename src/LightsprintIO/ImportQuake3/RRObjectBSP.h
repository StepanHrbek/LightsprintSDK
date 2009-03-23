// --------------------------------------------------------------------------
// Lightsprint adapters for Quake3 map (.bsp).
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "Lightsprint/RRScene.h"
#include "Q3Loader.h"

//! Creates Lightsprint interface for Quake3 map.
//
//! If you use standard Quake3 directory structure and scene was loaded from "foo/bar/maps/map1.bsp",
//! set pathToTextures to "foo/bar/maps/".
//! Textures will be loaded from proper paths specified by .bsp, but if it fails, second attempt will be made
//! directly in pathToTextures directory, with original image paths stripped.
rr::RRObjects* adaptObjectsFromTMapQ3(TMapQ3* model, const char* pathToTextures, rr::RRBuffer* missingTexture);

//! Makes it possible to load .bsp scenes from disk via rr::RRScene::RRScene().
void registerLoaderQuake3();

#endif
