// --------------------------------------------------------------------------
// Creates Lightsprint interface for 3DS scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RROBJECT3DS_H
#define RROBJECT3DS_H

#include "Lightsprint/RRDynamicSolver.h"
#include "Model_3DS.h"

//! Creates Lightsprint interface for 3DS scene.
rr::RRStaticObjects* adaptObjectsFrom3DS(Model_3DS* model);

#endif
