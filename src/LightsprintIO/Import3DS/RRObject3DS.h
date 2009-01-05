// --------------------------------------------------------------------------
// Creates Lightsprint interface for 3DS scene
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECT3DS_H
#define RROBJECT3DS_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Model_3DS.h"

//! Creates Lightsprint interface for 3DS scene.
rr::RRObjects* adaptObjectsFrom3DS(Model_3DS* model);

#endif
