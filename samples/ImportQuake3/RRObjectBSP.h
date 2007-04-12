// --------------------------------------------------------------------------
// Imports BSP model (quake3) into RRDynamicSolver
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "Lightsprint/RRRealtimeRadiosity.h"
#include "Q3Loader.h"
#include "Lightsprint/RRGPUOpenGL/RendererOfRRObject.h"

//! Creates Lightsprint interface for Quake3 scene.
rr::RRObjects* adaptObjectsFromTMapQ3(de::TMapQ3* model,const char* pathToTextures,de::Texture* missingTexture);

#endif
