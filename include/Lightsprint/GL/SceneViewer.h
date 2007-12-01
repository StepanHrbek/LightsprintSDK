//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/RRDynamicSolverGL.h"

namespace rr_gl
{

//! Runs viewer of scene in newly created window. Doesn't return, for debugging only.
void sceneViewer(RRDynamicSolverGL* solver, const char* pathToShaders);

}; // namespace

#endif
