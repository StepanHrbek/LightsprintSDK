// --------------------------------------------------------------------------
// Lightsprint adapters for 3DS scene.
// Copyright (C) 2005-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RROBJECT3DS_H
#define RROBJECT3DS_H

#include "Lightsprint/RRScene.h"

//! Creates Lightsprint interface for 3DS objects in memory.
rr::RRObjects* adaptObjectsFrom3DS(class Model_3DS* model);

//! Makes it possible to load .3ds scenes from disk via rr::RRScene::RRScene().
void registerLoader3DS();

#endif
