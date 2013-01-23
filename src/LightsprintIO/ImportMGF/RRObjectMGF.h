// --------------------------------------------------------------------------
// Lightsprint adapters for MGF scene (testing only)
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECTMGF_H
#define RROBJECTMGF_H

#include "Lightsprint/RRScene.h"

//! Creates Lightsprint interface for MGF scene.
//
//! Included in Lightsprint SDK for testing worst case scenarios
//! - mgf scenes don't have unwrap -> bad for per-pixel lightmaps
//! - many mgf scenes contain T vertices -> bad for per-vertex lighting
//!
//! Known limitations:
//! - only convex polygons are triangulated properly
rr::RRObjects* adaptObjectsFromMGF(const rr::RRString& filename);

//! Makes it possible to load .mgf scenes from disk via RRScene::RRScene().
void registerLoaderMGF();

#endif
