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
//! It renders scene with realtime or precomputed GI, with interactive options.
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
//!  True=initializes GL context and creates window. False=expects that context and window already exist.
//! \param pathToShaders
//!  Shaders are loaded from pathToShaders with trailing slash (or backslash).
//!  Texture projected by spotlights is loaded from pathToShaders + "../maps/spot0.png".
//! \param layerNumber
//!  Start rendering existing static buffers from this layer, use them for all static lighting operations.
//!  To start in realtime GI rendering mode, pass -1-layerNumber.
//! \param honourExpensiveLightingShadowingFlags
//!  False=all objects are lit and cast shadows, fast. True=honours lighting or shadowing disabled by rr::RRMesh::getTriangleMaterial(), slower.
void RR_GL_API sceneViewer(rr::RRDynamicSolver* solver, bool createWindow, const char* pathToShaders, int layerNumber, bool honourExpensiveLightingShadowingFlags);

enum UpdateResult
{
	UR_UPDATED, // update of lightmaps finished
	UR_ABORTED, // update was aborted and dialog closed
	UR_CUSTOM   // update was aborted and custom button pressed
};
//! RRDynamicSolver::updateLightmaps() with dialog.
//
//! This function can be used to replace RRDynamicSolver::updateLightmaps().
//! It has benefits
//! - shows log
//! - has abort button
//! - when aborted, it lets user change quality setting and rerun, or run sceneViewer
//! - has custom button for custom action
//!
//! and drawbacks
//! - depends on LightsprintGL library (because of sceneViewer)
//! - it's forbidden to run this function from multiple threads at the same time
UpdateResult RR_GL_API updateLightmapsWithDialog(rr::RRDynamicSolver* solver,
					int layerNumberLighting, int layerNumberDirectionalLighting, int layerNumberBentNormals, rr::RRDynamicSolver::UpdateParameters* paramsDirect, rr::RRDynamicSolver::UpdateParameters* paramsIndirect, const rr::RRDynamicSolver::FilteringParameters* filtering,
					bool createWindowForSceneViewer, const char* pathToShaders,
					const char* customButtonCaption);

}; // namespace

#endif
