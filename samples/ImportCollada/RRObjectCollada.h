// --------------------------------------------------------------------------
// Imports Collada model into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RROBJECTCOLLADA_H
#define RROBJECTCOLLADA_H

#include "Lightsprint/RRRealtimeRadiosity.h"

//! Creates Lightsprint interface for Collada scene.
rr::RRObjects* adaptObjectsFromFCollada(class FCDocument* document);

#endif
