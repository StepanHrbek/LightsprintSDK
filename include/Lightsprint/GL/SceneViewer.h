//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/DemoEngine.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

//! Runs viewer of scene in newly created window.
//! Warning: doesn't return, for debugging only. To be fixed.
void RR_GL_API sceneViewer(rr::RRDynamicSolver* solver, const char* pathToShaders);

}; // namespace

#endif
