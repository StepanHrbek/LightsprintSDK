// --------------------------------------------------------------------------
// Creates Lightsprint interface for MGF scene (testing only)
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008
// --------------------------------------------------------------------------

#ifndef RROBJECTMGF_H
#define RROBJECTMGF_H

#include "Lightsprint/RRDynamicSolver.h"

//! Creates Lightsprint interface for MGF scene.
//
//! Included in Lightsprint SDK for testing worst case scenarios
//! - mgf scenes don't have unwrap -> bad for per-pixel lightmaps
//! - many mgf scenes contain T vertices -> bad for per-vertex lighting
//!
//! Loader doesn't contain proper triangulation,
//! polygons more complicated than quad are often incorrectly triangulated.
rr::RRObjects* adaptObjectsFromMGF(const char* filename);

#endif
