// --------------------------------------------------------------------------
// Adapts FCollada document.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for Collada objects (geometry+materials).
//! You can search for textures in given directory (with trailing slash) rather than in default one.
//! You can remove original paths from texture filenames with stripPaths.
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document, const char* pathToTextures=NULL, bool stripPaths=false, float emissiveMultiplier=1);

//! Creates Lightsprint interface for Collada lights.
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document);

#endif
