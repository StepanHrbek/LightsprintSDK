//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/DemoEngine.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

//! Runs viewer of scene.
//
//! Warning: doesn't return, for debugging only. To be fixed.
//!
//! \param solver
//!  Scene to be displayed.
//! \param createWindow
//!  True=initializes GL context and creates window. False=expects context and window already exist.
//! \param pathToShaders
//!  Shaders are loaded from pathToShaders with trailing slash (or backslash).
//!  Texture projected by spotlights is loaded from pathToShaders + "../maps/spot0.png".
//! \param honourExpensiveLightingShadowingFlags
//!  False=all objects are lit and cast shadows, fast. True=honours lighting or shadowing disabled by rr::RRMesh::getTriangleMaterial(), slower.
void RR_GL_API sceneViewer(rr::RRDynamicSolver* solver, bool createWindow, const char* pathToShaders, bool honourExpensiveLightingShadowingFlags);

}; // namespace

#endif
