// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint adapters for MGF scene (testing only)
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
