//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/RRDynamicSolver.h"

#ifdef _MSC_VER // we need wxWidgets 2.9, not available in linux distros yet
#define SUPPORT_SCENEVIEWER
#endif
//#define CUSTOMIZED_FOR_3DRENDER

namespace rr_gl
{

enum LightingDirect
{
	LD_NONE,                 ///< No direct illumination, for testing only.
	LD_STATIC_LIGHTMAPS,     ///< Direct illumination is taken from lightmaps in staticLayerNumber.
	LD_REALTIME,             ///< Direct illumination is realtime computed.
};

enum LightingIndirect
{
	LI_NONE,                 ///< No indirect illumination, Doom-3 look with shadows completely black.
	LI_CONSTANT,             ///< Constant ambient, widely used poor man's approach.
	LI_STATIC_LIGHTMAPS,     ///< Indirect illumination is taken from lightmaps in staticLayerNumber.
	LI_REALTIME_ARCHITECT,   ///< Indirect illumination is realtime computed by Architect solver. No precalculations. If not sure, use Fireball.
	LI_REALTIME_FIREBALL,    ///< Indirect illumination is realtime computed by Fireball solver. Fast.
	LI_REALTIME_FIREBALL_LDM,///< Indirect illumination is realtime computed by Fireball solver, LDM from ldmLayerNumber adds details. Fast, but initial LDM build is slow.
};


//! Optional parameters of sceneViewer()
struct SceneViewerState
{
	// viewer state
	Camera           eye;                       //! Current camera.
	unsigned         staticLayerNumber;         //! Layer used for all static lighting operations. Set it to precomputed layer you want to display.
	unsigned         realtimeLayerNumber;       //! Layer used for all realtime lighting operations.
	unsigned         ldmLayerNumber;            //! Layer used for light indirect maps, precomputed maps that modulate realtime indirect per-vertex.
	unsigned         selectedLightIndex;        //! Index into lights array, light controlled by mouse/arrows.
	unsigned         selectedObjectIndex;       //! Index into static objects array.
	bool             fullscreen;                //! Fullscreen rather than window.
	LightingDirect   renderLightDirect;         //! Render direct illumination.
	LightingIndirect renderLightIndirect;       //! Render indirect illumination.
	bool             renderLightmaps2d;         //! When not rendering realtime, show static lightmaps in 2D.
	bool             renderLightmapsBilinear;   //! Render lightmaps with bilinear interpolation rather than without it.
	bool             renderMaterialDiffuse;     //! Render diffuse color.
	bool             renderMaterialSpecular;    //! Render specular reflections.
	bool             renderMaterialEmission;    //! Render emissivity.
	bool             renderMaterialTransparency;//! Render transparency.
	bool             renderMaterialTextures;    //! Render textures (diffuse, emissive) rather than constant colors.
	bool             renderWater;               //! Render water surface as a plane at y=waterLevel.
	bool             renderWireframe;           //! Render all in wireframe.
	bool             renderFPS;                 //! Render FPS counter.
	bool             renderHelpers;             //! Show helper wireframe objects and text outputs.
	bool             renderVignette;            //! Render vignette overlay from vignette.png.
	bool             renderHelp;                //! Render help overlay from sv_help.png.
	bool             renderLogo;                //! Render logo overlay from sv_logo.png.
	bool             adjustTonemapping;         //! Automatically adjust tonemapping operator.
	bool             cameraDynamicNear;         //! Camera sets near dynamically to prevent near clipping.
	float            cameraMetersPerSecond;     //! Speed of movement controlled by user, in m/s.
	rr::RRVec4       brightness;                //! Brightness applied at render time as simple multiplication, changed by adjustTonemapping.
	float            gamma;                     //! Gamma correction applied at render time, 1=no correction.
	float            waterLevel;                //! Water level in meters(scene units). Has effect only if renderWater.

	// viewer initialization
	bool             autodetectCamera;          //! Ignore what's set in eye and generate camera (and cameraMetersPerSecond) from scene.

	// sets default state with realtime GI and random camera
	SceneViewerState()
		: eye(-1.856f,1.440f,2.097f, 2.404f,0,-0.3f, 1.3f, 90, 0.1f,1000)
	{
		staticLayerNumber = 192837464; // any numbers unlikely to collide with user's layer numbers, better than 0 that nearly always collides
		realtimeLayerNumber = 192837465;
		ldmLayerNumber = 192837466;
		selectedLightIndex = 0;
		selectedObjectIndex = 0;
		fullscreen = 0;
		renderLightDirect = LD_REALTIME;
		renderLightIndirect = LI_REALTIME_FIREBALL;
		renderLightmaps2d = 0;
		renderMaterialDiffuse = 1;
		renderMaterialSpecular = 1;
		renderMaterialEmission = 1;
		renderMaterialTransparency = 1;
		renderMaterialTextures = 1;
		renderWater = 0;
		renderWireframe = 0;
		renderFPS = 0;
		renderHelpers = 0;
		renderLightmapsBilinear = 1;
		renderVignette = 0;
		renderHelp = 0;
		renderLogo = 0;
		adjustTonemapping = 1;
		cameraDynamicNear = 1;
		cameraMetersPerSecond = 2;
		brightness = rr::RRVec4(1);
		gamma = 1;
		waterLevel = -0.05f; // scenes often contain surfaces at y=0, place water slightly below to avoid/reduce artifacts
		autodetectCamera = 1;
	}
};

//! Runs interactive scene viewer.
//
//! Scene viewer includes debugging and relighting features. All lights and skybox are editable.
//! All lighting techniques for both realtime and precomputed GI are available via menu.
//!
//! \param inputSolver
//!  Solver to be displayed. This is handy for debugging scene already present in solver. May be NULL.
//! \param inputFilename
//!  If inputSolver is NULL, we attempt to open and display this file. This is handy for scene viewer applications.
//!  Your string is not free()d.
//! \param skyboxFilename
//!  If RRBuffer::loadCube() loads this skybox successfully, it is used, overriding eventual environment from inputSolver.
//!  Your string is not free()d.
//! \param pathToShaders
//!  Shaders are loaded from pathToShaders with trailing slash (or backslash).
//!  Your string is not free()d.
//! \param svs
//!  Initial state of viewer. Use NULL for default state with realtime GI and random camera.
//! \param releaseResources
//!  Resources allocated by scene viewer will be released on exit.
//!  It could take some time in huge scenes, so there's option to not release them, let them leak.
//!  Not releasing resources is good idea e.g. if you plan to exit application soon.
void RR_GL_API sceneViewer(rr::RRDynamicSolver* inputSolver, const char* inputFilename, const char* skyboxFilename, const char* pathToShaders, SceneViewerState* svs, bool releaseResources);

}; // namespace

#endif
