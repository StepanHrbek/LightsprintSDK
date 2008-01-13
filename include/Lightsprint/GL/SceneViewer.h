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

//! Runs interactive scene viewer.
//
//! It renders scene with realtime GI or precomputed GI in layer 0.
//!
//! Controls:
//! - right mouse button ... menu
//! - mouse and wsadqzxc ... manipulate selected camera or light
//! - wheel              ... zoom
//! - + - * /            ... brightness/contrast
//! - left mouse button  ... toggle selection between camera and light
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
