// --------------------------------------------------------------------------
// Imports BSP model (quake3) into RRRealtimeRadiosity
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "RRRealtimeRadiosity.h"
#include "Q3Loader.h"
#include "RRGPUOpenGL/RendererOfRRObject.h"

//! Imports all 3d objects from model into RRRealtimeRadiosity.
void insertBspToRR(TMapQ3* model,const char* pathToTextures,rr::RRRealtimeRadiosity* app, const rr::RRScene::SmoothingParameters* smoothing);

//! Deletes objects previously inserted by insert3dsToRR().
void deleteBspFromRR(rr::RRRealtimeRadiosity* app);

#endif
