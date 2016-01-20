// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint import isolation.
// --------------------------------------------------------------------------

#ifndef RRSCENEISOLATION_H
#define RRSCENEISOLATION_H

#include "Lightsprint/RRScene.h"

//! Makes scene import use isolated process.
void registerIsolationStep1(int argc, char** argv);
void registerIsolationStep2(int argc, char** argv);

#endif
