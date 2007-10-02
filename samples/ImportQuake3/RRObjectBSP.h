// --------------------------------------------------------------------------
// Creates Lightsprint interface for Quake3 map (.bsp)
// Copyright (C) Stepan Hrbek, Lightsprint, 2007
// --------------------------------------------------------------------------

#ifndef RROBJECTBSP_H
#define RROBJECTBSP_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Q3Loader.h"
#include "Lightsprint/GL/RendererOfRRObject.h"

//! Creates Lightsprint interface for Quake3 map.
rr::RRStaticObjects* adaptObjectsFromTMapQ3(TMapQ3* model,const char* pathToTextures,rr_gl::Texture* missingTexture);

#endif
