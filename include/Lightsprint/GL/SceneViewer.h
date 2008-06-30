//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2008
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/RRDynamicSolver.h"

namespace rr_gl
{

// Optional parameters of sceneViewer()
struct SceneViewerState
{
	// viewer state
	Camera           eye;
	unsigned         selectedLightIndex;  //! Index into lights, light controlled by mouse/arrows.
	unsigned         selectedObjectIndex; //! Index into static objects.
	bool             fullscreen;
	bool             renderRealtime;      //! realtime vs static lighting
	bool             render2d;            //! when not rendering realtime, show static lightmaps in 2D
	bool             renderAmbient;
	bool             renderDiffuse;
	bool             renderSpecular;
	bool             renderEmission;
	bool             renderTransparent;
	bool             renderTextures;
	bool             renderTonemapping;
	bool             renderWireframe;
	bool             renderHelpers;
	bool             honourExpensiveLightingShadowingFlags; //! False=all objects are lit and cast shadows, fast. True=honours lighting or shadowing disabled by rr::RRMesh::getTriangleMaterial(), slower.
	float            speedGlobal;         //! Speed of movement controlled by user, in m/s.
	rr::RRVec4       brightness;
	float            gamma;
	bool             bilinear;
	unsigned         staticLayerNumber;   //! Layer used for all static lighting operations. Set it to precomputed layer you want to display.
	unsigned         realtimeLayerNumber; //! Layer used for all realtime lighting operations.
	// viewer initialization
	bool             autodetectCamera;    //! Ignore what's set in eye and generate camera (and speedGlobal) from scene.
	// sets default state with realtime GI and random camera
	SceneViewerState()
		: eye(-1.856f,1.440f,2.097f, 2.404f,0,-0.3f, 1.3f, 90, 0.1f,1000)
	{
		selectedLightIndex = 0;
		selectedObjectIndex = 0;
		fullscreen = 0;
		renderRealtime = 1;
		render2d = 0;
		renderAmbient = 0;
		renderDiffuse = 1;
		renderSpecular = 0;
		renderEmission = 1;
		renderTransparent = 1;
		renderTextures = 1;
		renderTonemapping = 1;
		renderWireframe = 0;
		renderHelpers = 0;
		speedGlobal = 2;
		brightness = rr::RRVec4(1);
		gamma = 1;
		bilinear = 1;
		staticLayerNumber = 192837464;
		realtimeLayerNumber = 192837465;
		autodetectCamera = 1;
	}
};

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
//! \param svs
//!  Initial state of viewer. Use NULL for default state with realtime GI and random camera.
void RR_GL_API sceneViewer(rr::RRDynamicSolver* solver, bool createWindow, const char* pathToShaders, SceneViewerState* svs);

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
