// --------------------------------------------------------------------------
// Lightsprint import isolation.
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRSCENEISOLATION_H
#define RRSCENEISOLATION_H

#include "Lightsprint/RRScene.h"

//! Makes scene import use isolated process.
void registerLoaderIsolationStep1(int argc, char** argv);
void registerLoaderIsolationStep2(int argc, char** argv);

#endif
