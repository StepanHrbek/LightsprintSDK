// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RROBJECT3DS_H
#define RROBJECT3DS_H

#include "Lightsprint/RRRealtimeRadiosity.h"
#include "Lightsprint/DemoEngine/Model_3DS.h"

//! Creates Lightsprint interface for 3DS scene.
rr::RRRealtimeRadiosity::Objects* adaptObjectsFrom3DS(de::Model_3DS* model);

#endif
