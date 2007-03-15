// --------------------------------------------------------------------------
// Imports Model_3DS into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef _3DS2RR_H
#define _3DS2RR_H

#include "RRRealtimeRadiosity.h"
#include "DemoEngine/Model_3DS.h"

//! Imports all 3d objects from model into RRRealtimeRadiosity.
void insert3dsToRR(de::Model_3DS* model,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

//! Deletes objects previously inserted by insert3dsToRR().
void delete3dsFromRR(rr::RRRealtimeRadiosity* app);

#endif
