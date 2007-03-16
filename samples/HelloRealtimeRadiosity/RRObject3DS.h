// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RROBJECT3DS_H
#define RROBJECT3DS_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"

//! Imports all 3d objects from model into RRRealtimeRadiosity.
void insert3dsToRR(de::Model_3DS* model,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

//! Deletes objects previously inserted by insert3dsToRR().
void delete3dsFromRR(rr::RRRealtimeRadiosity* app);

#endif
