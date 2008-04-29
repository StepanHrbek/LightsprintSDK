// --------------------------------------------------------------------------
// Imports Collada model into RRDynamicSolver
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008
// --------------------------------------------------------------------------

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for Collada objects (geometry+materials).
//! You can search for textures in given directory (with trailing slash) rather than in default one.
//! You can remove original paths from texture filenames with stripPaths.
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document, const char* pathToTextures=NULL, bool stripPaths=false);

//! Creates Lightsprint interface for Collada lights.
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document);

#endif
