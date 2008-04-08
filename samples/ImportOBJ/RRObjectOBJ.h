// --------------------------------------------------------------------------
// Creates Lightsprint interface for OBJ scene
// Copyright (C) Stepan Hrbek, Lightsprint, 2008
// --------------------------------------------------------------------------

#ifndef RROBJECTOBJ_H
#define RROBJECTOBJ_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for .OBJ file.
//
//! All sizes are multiplied by scale at load time, use scale to normalize
//! unit to recommended 1 meter. OBJ doesn't specify unit length.
rr::RRObjects* adaptObjectsFromOBJ(const char* filename, float scale=1);

#endif
