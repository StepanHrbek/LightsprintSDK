// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for FCollada document.
// --------------------------------------------------------------------------

#ifndef RROBJECTFCOLLADA_H
#define RROBJECTFCOLLADA_H

#include "Lightsprint/RRScene.h"

//! Creates Lightsprint interface for Collada objects (geometry+materials).
//! You can search for textures in given directory (with trailing slash) rather than in default one.
//! Textures will be loaded from proper paths specified by collada, but if it fails, second attempt will be made
//! directly in pathToTextures directory, with original image paths stripped.
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document, const rr::RRFileLocator* textureLocator);

//! Creates Lightsprint interface for Collada lights.
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document);

//! Makes it possible to load .dae scenes from disk via rr::RRScene::RRScene().
void registerLoaderFCollada();

#endif
