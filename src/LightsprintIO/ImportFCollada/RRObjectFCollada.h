// --------------------------------------------------------------------------
// Lightsprint adapters for FCollada document.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTFCOLLADA_H
#define RROBJECTFCOLLADA_H

#include "Lightsprint/RRScene.h"

//! Creates Lightsprint interface for Collada objects (geometry+materials).
//! You can search for textures in given directory (with trailing slash) rather than in default one.
//! Textures will be loaded from proper paths specified by collada, but if it fails, second attempt will be made
//! directly in pathToTextures directory, with original image paths stripped.
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document, const char* pathToTextures=NULL);

//! Creates Lightsprint interface for Collada lights.
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document);

//! Makes it possible to load .dae scenes from disk via rr::RRScene::RRScene().
void registerLoaderFCollada();

#endif
