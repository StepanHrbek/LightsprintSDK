//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2010
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/RRDynamicSolver.h"

#if _MSC_VER==1600
//#define CUSTOMIZED_FOR_3DRENDER
#endif

#ifdef RR_GL_BUILD
	#define wxMSVC_VERSION_AUTO // used only when building this library
#endif

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
	bool             renderIcons;               //! Unused.
	bool             renderHelpers;             //! Show helper wireframe objects and text outputs.
	bool             renderBloom;               //! Render bloom effect.
	bool             renderLensFlare;           //! Render lens flare effect.
	float            lensFlareSize;             //! Relative lens flare size, 1 for typical size.
	unsigned         lensFlareId;               //! Other lens flare parameters are generated from this number.
	bool             renderVignette;            //! Render vignette overlay from vignette.png.
	bool             renderHelp;                //! Render help overlay from sv_help.png.
	bool             renderLogo;                //! Render logo overlay from sv_logo.png.
	bool             renderTonemapping;         //! Render with tonemapping.
	rr::RRVec4       tonemappingBrightness;     //! If(renderTonemapping) Brightness applied at render time as simple multiplication, changed by tonemappingAutomatic.
	float            tonemappingGamma;          //! If(renderTonemapping) Gamma correction applied at render time, 1=no correction.
	bool             tonemappingAutomatic;      //! Automatically adjust tonemappingBrightness.
	float            tonemappingAutomaticTarget;//! Target average screen intensity for tonemappingAutomatic.
	float            tonemappingAutomaticSpeed; //! Speed of automatic tonemapping change.
	bool             playVideos;                //! Play videos, false = videos are paused.
	float            emissiveMultiplier;        //! Multiplies effect of emissive materials on scene, without affecting emissive materials.
	bool             videoEmittanceAffectsGI;   //! 
	bool             videoTransmittanceAffectsGI;
	bool             cameraDynamicNear;         //! Camera sets near dynamically to prevent near clipping.
	float            cameraMetersPerSecond;     //! Speed of movement controlled by user, in m/s.
	float            waterLevel;                //! Water level in meters(scene units). Has effect only if renderWater.
	rr::RRVec3       waterColor;                //! Water color in sRGB. Has effect only if renderWater.
	bool             renderGrid;                //! Show grid.
	unsigned         gridNumSegments;           //! Number of grid segments per line, i.e. 10 makes grid with 10x10 squares.
	float            gridSegmentSize;           //! Distance of grid lines in meters (scene units).
	int              precision;                 //! Max number of digits after decimal point in float properties. -1 for full precision.
	bool             autodetectCamera;          //! At initialization, ignore what's set in eye and generate camera (and cameraMetersPerSecond) from scene.

	// sets default state with realtime GI and random camera
	SceneViewerState()
		: eye(-1.856f,1.8f,2.097f, 2.404f,0,-0.3f, 1.3f, 90, 0.1f,1000)
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
		renderIcons = 1;
		renderHelpers = 0;
		renderLightmapsBilinear = 1;
		renderBloom = 0;
		renderLensFlare = 0;
		lensFlareSize = 1;
		lensFlareId = 0;
		renderVignette = 0;
		renderHelp = 0;
		renderLogo = 0;
		renderTonemapping = 1;
		tonemappingAutomatic = 1;
		tonemappingAutomaticTarget = 0.5;
		tonemappingAutomaticSpeed = 1;
		tonemappingBrightness = rr::RRVec4(1);
		tonemappingGamma = 1;
		playVideos = 1;
		emissiveMultiplier = 1;
		videoEmittanceAffectsGI = true;
		videoTransmittanceAffectsGI = true;
		cameraDynamicNear = 1;
		cameraMetersPerSecond = 2;
		waterColor = rr::RRVec3(0.1f,0.25f,0.35f);
		waterLevel = -0.05f; // scenes often contain surfaces at y=0, place water slightly below to avoid/reduce artifacts
		renderGrid = 0;
		gridNumSegments = 100;
		gridSegmentSize = 1;
		autodetectCamera = 1;
		precision = -1; // display all significant digits
	}
	bool operator ==(const SceneViewerState& a) const
	{
		return 1
			&& a.eye==eye
			&& a.staticLayerNumber==staticLayerNumber
			&& a.realtimeLayerNumber==realtimeLayerNumber
			&& a.ldmLayerNumber==ldmLayerNumber
			&& a.selectedLightIndex==selectedLightIndex
			&& a.selectedObjectIndex==selectedObjectIndex
			&& a.fullscreen==fullscreen
			&& a.renderLightDirect==renderLightDirect
			&& a.renderLightIndirect==renderLightIndirect
			&& a.renderLightmaps2d==renderLightmaps2d
			&& a.renderLightmapsBilinear==renderLightmapsBilinear
			&& a.renderMaterialDiffuse==renderMaterialDiffuse
			&& a.renderMaterialSpecular==renderMaterialSpecular
			&& a.renderMaterialEmission==renderMaterialEmission
			&& a.renderMaterialTransparency==renderMaterialTransparency
			&& a.renderMaterialTextures==renderMaterialTextures
			&& a.renderWater==renderWater
			&& a.renderWireframe==renderWireframe
			&& a.renderFPS==renderFPS
			&& a.renderIcons==renderIcons
			&& a.renderHelpers==renderHelpers
			&& a.renderBloom==renderBloom
			&& a.renderLensFlare==renderLensFlare
			&& a.lensFlareSize==lensFlareSize
			&& a.lensFlareId==lensFlareId
			&& a.renderVignette==renderVignette
			&& a.renderHelp==renderHelp
			&& a.renderLogo==renderLogo
			&& a.renderTonemapping==renderTonemapping
			&& a.tonemappingAutomatic==tonemappingAutomatic
			&& a.tonemappingAutomaticTarget==tonemappingAutomaticTarget
			&& a.tonemappingAutomaticSpeed==tonemappingAutomaticSpeed
			&& (a.tonemappingBrightness==tonemappingBrightness || (renderTonemapping && tonemappingAutomatic)) // brightness may differ if automatic tonemapping is enabled
			&& a.tonemappingGamma==tonemappingGamma
			&& a.playVideos==playVideos
			&& a.emissiveMultiplier==emissiveMultiplier
			&& a.videoEmittanceAffectsGI==videoEmittanceAffectsGI
			&& a.videoTransmittanceAffectsGI==videoTransmittanceAffectsGI
			&& a.cameraDynamicNear==cameraDynamicNear
			&& a.cameraMetersPerSecond==cameraMetersPerSecond
			&& a.waterLevel==waterLevel
			&& a.waterColor==waterColor
			&& a.renderGrid==renderGrid
			&& a.gridNumSegments==gridNumSegments
			&& a.gridSegmentSize==gridSegmentSize
			&& a.precision==precision
			&& a.autodetectCamera==autodetectCamera
			;
	}
	bool operator !=(const SceneViewerState& a) const
	{
		return !(a==*this);
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
//!  Path to directory with shaders.
//!  Must be terminated with slash (or be empty for current dir).
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
