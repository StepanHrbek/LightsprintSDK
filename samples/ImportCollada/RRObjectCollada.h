// --------------------------------------------------------------------------
// Imports Collada model into RRDynamicSolver
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008
// --------------------------------------------------------------------------

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for Collada objects (geometry+materials).
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document);

//! Creates Lightsprint interface for Collada lights.
rr::RRLights* adaptLightsFromFCollada(class FCDocument* document);

#endif
