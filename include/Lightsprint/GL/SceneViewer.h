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

//! Optional parameters of sceneViewer()
struct SceneViewerState
{
	// viewer state
	Camera           eye;
	unsigned         staticLayerNumber;   //! Layer used for all static lighting operations. Set it to precomputed layer you want to display.
	unsigned         realtimeLayerNumber; //! Layer used for all realtime lighting operations.
	unsigned         ldmLayerNumber;      //! Layer used for light indirect maps, precomputed maps that modulate realtime indirect per-vertex.
	unsigned         selectedLightIndex;  //! Index into lights array, light controlled by mouse/arrows.
	unsigned         selectedObjectIndex; //! Index into static objects array.
	bool             fullscreen;          //! Fullscreen rather than window.
	bool             renderRealtime;      //! Realtime lighting (from realtimeLayerNumber) rather than static lighting (from staticLayerNumber).
	bool             render2d;            //! When not rendering realtime, show static lightmaps in 2D.
	bool             renderAmbient;       //! Constant ambient light.
	bool             renderLDM;           //! Render light detail map (from ldmLayerNumber).
	bool             renderDiffuse;       //! Render diffuse color.
	bool             renderSpecular;      //! Render specular reflections.
	bool             renderEmission;      //! Render emissivity.
	bool             renderTransparent;   //! Render transparency.
	bool             renderTextures;      //! Render textures (diffuse, emissive) rather than constant colors.
	bool             renderWater;         //! Render water surface at world y=0.
	bool             renderWireframe;     //! Render all in wireframe.
	bool             renderHelpers;       //! Show helper wireframe objects and text outputs.
	bool             renderBilinear;      //! Render lightmaps with bilinear interpolation rather than without it.
	bool             adjustTonemapping;   //! Automatically adjust tonemapping operator.
	bool             honourExpensiveLightingShadowingFlags; //! False=all objects are lit and cast shadows, fast. True=honours lighting or shadowing disabled by rr::RRMesh::getTriangleMaterial(), slower.
	bool             cameraGravity;       //! Camera responds to gravity.
	bool             cameraCollisions;    //! Camera collides with geometry.
	float            cameraMetersPerSecond;//! Speed of movement controlled by user, in m/s.
	rr::RRVec4       brightness;          //! Brightness applied at render time as simple multiplication, changed by adjustTonemapping.
	float            gamma;               //! Gamma correction applied at render time, 1=no correction.
	// viewer initialization
	bool             autodetectCamera;    //! Ignore what's set in eye and generate camera (and cameraMetersPerSecond) from scene.
	// sets default state with realtime GI and random camera
	SceneViewerState()
		: eye(-1.856f,1.440f,2.097f, 2.404f,0,-0.3f, 1.3f, 90, 0.1f,1000)
	{
		staticLayerNumber = 192837464;
		realtimeLayerNumber = 192837465;
		ldmLayerNumber = 192837466;
		selectedLightIndex = 0;
		selectedObjectIndex = 0;
		fullscreen = 0;
		renderRealtime = 1;
		render2d = 0;
		renderAmbient = 0;
		renderLDM = 0;
		renderDiffuse = 1;
		renderSpecular = 0;
		renderEmission = 1;
		renderTransparent = 1;
		renderTextures = 1;
		renderWater = 0;
		renderBilinear = 1;
		renderWireframe = 0;
		renderHelpers = 0;
		adjustTonemapping = 1;
		honourExpensiveLightingShadowingFlags = 0;
		cameraGravity = 0;
		cameraCollisions = 0;
		cameraMetersPerSecond = 2;
		brightness = rr::RRVec4(1);
		gamma = 1;
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
