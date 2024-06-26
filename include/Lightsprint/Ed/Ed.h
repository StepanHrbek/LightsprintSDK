//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file Ed.h
//! \brief LightsprintGL | 3d scene editor with global illumination
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

// enforces comctl32.dll version 6 in all application that use sceneViewer() (such application must include this header or use following pragma)
// default version 5 creates errors we described in http://trac.wxwidgets.org/ticket/12709, 12710, 12711, 12112
#if !defined(RR_ED_STATIC)
	#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "Lightsprint/GL/RealtimeLight.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/RRSolver.h"
#include <ctime> // struct tm

// define RR_ED_API
#ifdef _MSC_VER
	#ifdef RR_ED_STATIC
		// build or use static library
		#define RR_ED_API
	#elif defined(RR_ED_BUILD)
		// build dll
		#define RR_ED_API __declspec(dllexport)
		//#pragma warning(disable:4251) // stop MSVC warnings
	#else
		// use dll
		#define RR_ED_API __declspec(dllimport)
		//#pragma warning(disable:4251) // stop MSVC warnings
	#endif
#else
	// build or use static library
	#define RR_ED_API
#endif

// autolink library when external project includes this header
#ifdef _MSC_VER
	#if !defined(RR_ED_MANUAL_LINK) && !defined(RR_ED_BUILD)
		#ifdef RR_ED_STATIC
			// use static library
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintEd." RR_LIB_COMPILER "_sr.lib")
			#else
				#pragma comment(lib,"LightsprintEd." RR_LIB_COMPILER "_sd.lib")
			#endif
		#else
			// use dll
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintEd." RR_LIB_COMPILER ".lib")
			#else
				#pragma comment(lib,"LightsprintEd." RR_LIB_COMPILER "_dd.lib")
			#endif
		#endif
	#endif
#endif

#if defined(RR_ED_BUILD) && !defined(wxMSVC_VERSION_AUTO)
	#define wxMSVC_VERSION_AUTO // used only when building this library, looks for wx libraries at different paths for different compilers
#endif

namespace rr_ed /// LightsprintEd - scene editor with global illumination
{

enum LightingIndirect
{
	LI_NONE,                 ///< No indirect illumination, Doom-3 look with shadows completely black.
	LI_CONSTANT,             ///< Constant ambient, widely used poor man's approach.
	LI_LIGHTMAPS,            ///< Indirect illumination is taken from baked lightmaps in layerBakedLightmap.
	LI_AMBIENTMAPS,          ///< Indirect illumination is taken from baked ambient maps in layerBakedAmbient.
	LI_REALTIME_ARCHITECT,   ///< Indirect illumination is realtime computed by Architect solver. No precalculations. If not sure, use Fireball.
	LI_REALTIME_FIREBALL,    ///< Indirect illumination is realtime computed by Fireball solver. Fast.
};

//! Transparency modes used by realtime renderer, to trade speed/quality. Offline GI solver always works as if the highest quality mode is selected.
enum Transparency
{
	//! No transparency, the fastest mode, no object sorting, light is completely blocked by surface.
	T_OPAQUE,
	//! 1-bit transparency, very fast mode, no object sorting, light either stops or goes through, no semi-translucency. Good for fences, plants etc.
	T_ALPHA_KEY,
	//! 8-bit transparency, fast mode, objects must be sorted, fraction of light goes through without changing color, creates semi-translucency effect. Good for non-colored glass. Unlike alpha keying, blending may generate artifacts when semi-translucent faces overlap.
	T_ALPHA_BLEND,
	//! 24-bit transparency, the highest quality mode, objects must be sorted, semi-translucency is evaluated separately in rgb channels. Good for colored glass (only blue light goes through blue glass). Unlike alpha keying, blending may generate artifacts when semi-translucent faces overlap.
	T_RGB_BLEND,
};


enum PivotPosition
{
	PP_TOP,
	PP_CENTER,
	PP_BOTTOM,
};

//! Optional parameters of sceneViewer()
struct SceneViewerState
{
	rr::RRCamera     camera;                    //! Current camera. If you wish to pass panorama settings to sceneViewer, set SceneViewerState::panoramaXxx.
	rr::RRTime       referenceTime;             //! Time when animation started/was in position 0.

	rr::RRString     skyboxFilename;            //! Current skybox filename, e.g. skybox.hdr or skybox_ft.tga.
	float            skyboxRotationRad;
	bool             envSimulateSky;            //! Should we simulate sky according to location and datetime?
	bool             envSimulateSun;            //! Should we simulate Sun according to location and datetime?
	float            envLongitudeDeg;           //! Longitude for sky/sun simulation, -180..180, east is positive, west is negative.
	float            envLatitudeDeg;            //! Latitude for sky/sun simulation, -90..90, north is positive, south is negative.
	float            envSpeed;                  //! How qualicky simulation runs, 0=stopped, 1=realtime, 3600=day in 24sec.
	tm               envDateTime;               //! Date and time for sky/sun simulation.

	unsigned         layerBakedLightmap;        //! Layer used for static global illumination.
	unsigned         layerBakedAmbient;         //! Layer used for static indirect illumination.
	unsigned         layerRealtimeAmbient;      //! Layer used for realtime lighting.
	unsigned         layerBakedLDM;             //! Layer used for light indirect maps, precomputed maps that modulate realtime indirect per-vertex.
	unsigned         layerBakedEnvironment;     //! Layer used for realtime environment, cubemaps.
	unsigned         layerRealtimeEnvironment;  //! Layer used for static environment, cubemaps.

	unsigned         selectedLightIndex;        //! Index into lights array, light controlled by mouse/arrows.
	unsigned         selectedObjectIndex;       //! Index into static objects array.
	unsigned         selectedLayer;             //! When renderLightmaps2d, this is used for selecting what layer to show. 0 selects based on current illumination mode.
	bool             fullscreen;                //! Ignored. Fullscreen/windowed bit is saved to and read from user preferences file. Quit sceneViewer() in fullscreen and it will start in fullscreen next time.
	bool             renderStereo;              //! Enables stereo rendering.
	bool             renderPanorama;            //! Enables 360 degree panorama rendering.
	rr::RRCamera::PanoramaMode panoramaMode;    //! Overrides the same variable in camera.
	rr::RRCamera::PanoramaCoverage panoramaCoverage;//! Overrides the same variable in camera.
	float            panoramaScale;             //! Overrides the same variable in camera.
	float            panoramaFovDeg;            //! Overrides the same variable in camera.
	bool             renderDof;                 //! Render depth of field effect.
	bool             dofAccumulated;            //! For depth of field effect only: set dof near/far automatically.
	rr::RRString     dofApertureShapeFilename;  //! For depth of field effect only: filename of bokeh image.
	bool             dofAutomaticFocusDistance; //! For depth of field effect only: set dof near/far automatically.
	bool             renderLightDirect;         //! False disables realtime direct lighting based on glsl+shadowmaps.
	LightingIndirect renderLightIndirect;       //! Indirect illumination mode.
	bool             renderLightDirectRelevant() {return renderLightIndirect!=LI_LIGHTMAPS;}
	bool             renderLightDirectActive() {return renderLightDirect && renderLightDirectRelevant();}

	bool             multipliersEnabled;
	rr::RRSolver::Multipliers multipliers;
	rr::RRSolver::Multipliers multipliersDirect;
	rr::RRSolver::Multipliers multipliersIndirect;
	rr::RRSolver::Multipliers getMultipliersDirect()   {return multipliersEnabled ? multipliers*multipliersDirect : rr::RRSolver::Multipliers();}
	rr::RRSolver::Multipliers getMultipliersIndirect() {return multipliersEnabled ? multipliers*multipliersIndirect : rr::RRSolver::Multipliers();}

	bool             renderLDM;                 //! Modulate indirect illumination by LDM.
	bool             renderLDMRelevant() {return renderLightIndirect!=LI_LIGHTMAPS && renderLightIndirect!=LI_AMBIENTMAPS && renderLightIndirect!=LI_NONE;}
	bool             renderLDMEnabled() {return renderLDM && renderLDMRelevant();}
	bool             renderLightmaps2d;         //! When not rendering realtime, show static lightmaps in 2D.
	bool             renderLightmapsBilinear;   //! Render lightmaps with bilinear interpolation rather than without it.
	bool             renderDDI;                 //! Render triangles illuminated with DDI. Diagnostic use only.
	bool             renderMaterialDiffuse;     //! Render diffuse reflections.
	bool             renderMaterialDiffuseColor;//! Render diffuse color.
	bool             renderMaterialSpecular;    //! Render specular reflections.
	bool             renderMaterialEmission;    //! Render emissivity.
	Transparency     renderMaterialTransparency;//! Render transparency. Allows realtime renderer to use modes up to this one. Offline GI always uses the highest one.
	bool             renderMaterialTransparencyNoise; //! When rendering 1bit transparency, use noise to simulate different levels of transparency.
	bool             renderMaterialTransparencyFresnel; //! When rendering transparency, modulate result by fresnel term.
	bool             renderMaterialTransparencyRefraction; //! When rendering transparency, approximate refraction with cubemaps.
	bool             renderMaterialBumpMaps;  //! Render normal maps.
	bool             renderMaterialTextures;    //! Render textures (diffuse, emissive) rather than constant colors.
	bool             renderMaterialSidedness;   //! Render 1-sided materials as 1-sided, rather than everything 2-sided.
	bool             renderWireframe;           //! Render all in wireframe.
	bool             renderFPS;                 //! Render FPS counter.
	bool             renderHelpers;             //! Show helper wireframe objects and text outputs.
	bool             renderBloom;               //! Render bloom effect.
	float            bloomThreshold;            //! Only pixels with intensity above this threshold receive bloom.
	bool             renderLensFlare;           //! Render lens flare effect.
	float            lensFlareSize;             //! Relative lens flare size, 1 for typical size.
	unsigned         lensFlareId;               //! Other lens flare parameters are generated from this number.
	bool             renderVignette;            //! Render vignette overlay from sv_vignette.png.
	bool             renderHelp;                //! Render help overlay from sv_help.png.
	bool             renderLogo;                //! Render logo overlay from sv_logo.png.
	bool             renderTonemapping;         //! Render with tonemapping.
	rr_gl::ToneParameters tonemapping;          //! If(renderTonemapping) color correction applied at render time.
	bool             tonemappingAutomatic;      //! Automatically adjust tonemappingBrightness.
	float            tonemappingAutomaticTarget;//! Target average screen intensity for tonemappingAutomatic.
	float            tonemappingAutomaticSpeed; //! Speed of automatic tonemapping change.
	bool             pathEnabled;               //! Enables pathtracer after 0.2s of inactivity.
	bool             pathShortcut;              //! Lets pathtracer reuse indirect from Fireball/Arch solvers, if available.
	bool             ssgiEnabled;               //! Apply SSGI postprocess.
	float            ssgiIntensity;             //! Intensity of SSGI effect, with 1=default, 0=no effect.
	float            ssgiRadius;                //! Distance of occluders (in meters) taken into account when calculating SSGI.
	float            ssgiAngleBias;             //! SSGI is based on face normals, not per-pixel normals, so at 0 even smooth edges are visible; higher angle bias makes more edges invisible.
	bool             contoursEnabled;
	rr::RRVec3       contoursSilhouetteColor;
	rr::RRVec3       contoursCreaseColor;
	bool             playVideos;                //! Play videos, false = videos are paused.
	rr_gl::RealtimeLight::ShadowTransparency shadowTransparency; //! Type of transparency in shadows, we copy it to all lights.
	bool             videoEmittanceAffectsGI;   //! Makes video in emissive material slot affect GI in realtime, light emitted from video is recalculated in every frame.
	unsigned         videoEmittanceGIQuality;   //! Quality if videoEmittanceAffectsGI is true.
	bool             videoTransmittanceAffectsGI;//! Makes video in transparency material slot affect GI in realtime, light going through transparent regions is recalculated in every frame.
	bool             videoTransmittanceAffectsGIFull;//! Updates GI rather than shadows only.
	bool             videoEnvironmentAffectsGI; //! Makes video in environment affect GI in realtime, light emitted from video is recalculated in every frame.
	unsigned         videoEnvironmentGIQuality; //! Quality if videoEnvironmentAffectsGI is true.
	unsigned         fireballQuality;           //! Quality used each time Fireball needs rebuild.
	unsigned         fireballWorkPerFrame;
	unsigned         fireballWorkTotal;
	bool             raytracedCubesEnabled;     //! Enables realtime raytraced diffuse and specular cubemap reflections.
	unsigned         raytracedCubesRes;         //! Resolution of reflection env maps.
	unsigned         raytracedCubesMaxObjects;  //! But only if there is less than this number of objects in scene.
	float            raytracedCubesSpecularThreshold;
	float            raytracedCubesDepthThreshold;
	bool             mirrorsEnabled;            //! Enables rendering of flat reflections using mirror maps (rather than less accurate raytraced cubes).
	bool             mirrorsDiffuse;            //! Enables using mirrors when rendering diffuse reflection.
	bool             mirrorsSpecular;           //! Enables using mirrors when rendering specular reflection.
	bool             mirrorsMipmaps;            //! Enables higher quality mirroring mode using mipmaps.
	bool             mirrorsOcclusion;          //! Enables occlusion query optimization, increases fps in some scenes, reduces in others.
	bool             srgbCorrect;               //! Add realtime lights sRGB correctly, if OpenGL 3.0+ or sufficient extensions are found.
	bool             lightmapFloats;
	//rr::RRSolver::CalculateParameters calculateParameters;        //! Realtime GI settings.
	rr::RRSolver::UpdateParameters    lightmapDirectParameters;   //! Lightmap baking settings.
	//rr::RRSolver::UpdateParameters    lightmapIndirectParameters; //! Lightmap baking settings.
	rr::RRSolver::FilteringParameters lightmapFilteringParameters;  //! Lightmap baking settings.
	bool             cameraDynamicNear;         //! Camera sets near dynamically to prevent near clipping.
	unsigned         cameraDynamicNearNumRays;  //! How many rays it shoots per frame to autodetect camera near/far.
	float            cameraMetersPerSecond;     //! Speed of movement controlled by user, in m/s.
	bool             renderGrid;                //! Show grid.
	unsigned         gridNumSegments;           //! Number of grid segments per line, e.g. 10 makes grid with 10x10 squares.
	float            gridSegmentSize;           //! Distance of grid lines in meters (scene units).
	int              precision;                 //! Max number of digits after decimal point in float properties. -1 for full precision.
	bool             openLogWindows;            //! True = opens new log window for each task. False = does not create any new log window (works with preexisting windows).
	bool             autodetectCamera;          //! At initialization, ignore what's set in eye and generate camera (and cameraMetersPerSecond) from scene.

	//! Sets default state with realtime GI and random camera.
	SceneViewerState()
	{
		clearSvs();
	}

	//! Clears all but state that should survive 'project' load.
	void clearSvs()
	{
		new(&camera) rr::RRCamera(rr::RRVec3(-1.856f,1.8f,2.097f), rr::RRVec3(2.404f,-0.3f,0), 1.3f, 90, 0.1f,1000);
		skyboxRotationRad = 0;
		envSimulateSky = false;
		envSimulateSun = false;
		envLongitudeDeg = 14+26/60.f; // Prague
		envLatitudeDeg = 50+5/60.f;
		envSpeed = 0;
		time_t now = time(nullptr);
		envDateTime = *localtime(&now);

		layerBakedLightmap = 192837463; // any numbers unlikely to collide with user's layer numbers, better than 0 that nearly always collides
		layerBakedAmbient = 192837464;
		layerRealtimeAmbient = 192837465;
		layerBakedLDM = 192837466;
		layerBakedEnvironment = 192837467;
		layerRealtimeEnvironment = 192837468;
		selectedLightIndex = 0;
		selectedObjectIndex = 0;
		selectedLayer = 0;
		fullscreen = 0;
		renderStereo = false;
		renderPanorama = false;
		panoramaMode = rr::RRCamera::PM_EQUIRECTANGULAR;
		panoramaCoverage = rr::RRCamera::PC_FULL;
		panoramaScale = 1;
		panoramaFovDeg = 180;
		renderDof = false;
		dofAccumulated = true;
		dofApertureShapeFilename.clear();
		dofAutomaticFocusDistance = false;
		renderLightDirect = true;
		renderLightIndirect = LI_REALTIME_FIREBALL;
		multipliersEnabled = false;
		multipliers = rr::RRSolver::Multipliers();
		multipliersDirect = rr::RRSolver::Multipliers();
		multipliersIndirect = rr::RRSolver::Multipliers();
		renderLDM = true;
		renderLightmaps2d = 0;
		renderMaterialDiffuse = 1;
		renderMaterialDiffuseColor = 1;
		renderMaterialSpecular = 1;
		renderMaterialEmission = 1;
		renderMaterialTransparency = T_RGB_BLEND;
		renderMaterialTransparencyNoise = true;
		renderMaterialTransparencyFresnel = true;
		renderMaterialTransparencyRefraction = 0;
		renderMaterialBumpMaps = 1;
		renderMaterialTextures = 1;
		renderMaterialSidedness = true;
		renderWireframe = 0;
		renderFPS = 0;
		renderHelpers = 0;
		renderLightmapsBilinear = 1;
		renderDDI = false;
		renderBloom = 0;
		bloomThreshold = 1;
		renderLensFlare = 1;
		lensFlareSize = 8;
		lensFlareId = 204;
		renderVignette = 0;
		renderHelp = 0;
		renderLogo = 0;
		renderTonemapping = 0;
		tonemappingAutomatic = 1;
		tonemappingAutomaticTarget = 0.5f;
		tonemappingAutomaticSpeed = 1;
		tonemapping = rr_gl::ToneParameters();
		pathEnabled = false;
		pathShortcut = true;
		ssgiEnabled = true;
		ssgiIntensity = 1.0f;
		ssgiRadius = 0.3f;
		ssgiAngleBias = 0.1f;
		contoursEnabled = false;
		contoursSilhouetteColor = rr::RRVec3(0);
		contoursCreaseColor = rr::RRVec3(0.7f);
		playVideos = 1;
		shadowTransparency = rr_gl::RealtimeLight::FRESNEL_SHADOWS;
		videoEmittanceAffectsGI = true;
		videoEmittanceGIQuality = 5;
		videoTransmittanceAffectsGI = true;
		videoTransmittanceAffectsGIFull = true;
		videoEnvironmentAffectsGI = true;
		videoEnvironmentGIQuality = 300;
		fireballQuality = 350;
		fireballWorkPerFrame = 3;
		fireballWorkTotal = 10000;
		raytracedCubesEnabled = true;
		raytracedCubesRes = 16;
		raytracedCubesMaxObjects = 1000;
		raytracedCubesSpecularThreshold = 0.2f;
		raytracedCubesDepthThreshold = 0.1f;
		mirrorsEnabled = true;
		mirrorsDiffuse = true;
		mirrorsSpecular = true;
		mirrorsMipmaps = true;
		mirrorsOcclusion = true;
		srgbCorrect = true;
		lightmapFloats = false;
		lightmapDirectParameters.aoSize = 1;
		cameraDynamicNear = true;
		cameraDynamicNearNumRays = 100;
		cameraMetersPerSecond = 2;
		renderGrid = 0;
		gridNumSegments = 100;
		gridSegmentSize = 1;
		openLogWindows = true;
		autodetectCamera = 1;
		precision = -1; // display all significant digits
	}
	//! Used to check whether important state was modified, less important state is commented out. 
	bool operator ==(const SceneViewerState& a) const
	{
		return 1
			&& a.camera==camera

			&& a.skyboxFilename==skyboxFilename
			&& a.skyboxRotationRad==skyboxRotationRad
			&& a.envSimulateSky==envSimulateSky
			&& a.envSimulateSun==envSimulateSun
			&& a.envLongitudeDeg==envLongitudeDeg
			&& a.envLatitudeDeg==envLatitudeDeg
			&& a.envSpeed==envSpeed
			&& a.envDateTime.tm_year==envDateTime.tm_year
			&& a.envDateTime.tm_mon==envDateTime.tm_mon
			&& a.envDateTime.tm_mday==envDateTime.tm_mday
			&& a.envDateTime.tm_hour==envDateTime.tm_hour
			&& a.envDateTime.tm_min==envDateTime.tm_min
			&& a.envDateTime.tm_sec==envDateTime.tm_sec

			&& a.layerBakedLightmap==layerBakedLightmap
			&& a.layerBakedAmbient==layerBakedAmbient
			&& a.layerRealtimeAmbient==layerRealtimeAmbient
			&& a.layerBakedLDM==layerBakedLDM
			&& a.layerBakedEnvironment==layerBakedEnvironment
			&& a.layerRealtimeEnvironment==layerRealtimeEnvironment
			//&& a.selectedLightIndex==selectedLightIndex // differs after click to scene tree
			//&& a.selectedObjectIndex==selectedObjectIndex
			&& a.fullscreen==fullscreen
			&& a.renderStereo==renderStereo
			&& a.renderPanorama==renderPanorama
			&& a.panoramaMode==panoramaMode
			&& a.panoramaCoverage==panoramaCoverage
			&& a.panoramaScale==panoramaScale
			&& a.panoramaFovDeg==panoramaFovDeg
			&& a.renderDof==renderDof
			&& a.dofAccumulated==dofAccumulated
			&& a.dofApertureShapeFilename==dofApertureShapeFilename
			&& a.dofAutomaticFocusDistance==dofAutomaticFocusDistance
			&& a.renderLightDirect==renderLightDirect
			&& a.renderLightIndirect==renderLightIndirect
			&& a.multipliersEnabled==multipliersEnabled
			&& a.multipliers==multipliers
			&& a.multipliersDirect==multipliersDirect
			&& a.multipliersIndirect==multipliersIndirect
			&& a.renderLDM==renderLDM
			&& a.renderLightmaps2d==renderLightmaps2d
			&& a.renderLightmapsBilinear==renderLightmapsBilinear
			&& a.renderDDI==renderDDI
			&& a.renderMaterialDiffuse==renderMaterialDiffuse
			&& a.renderMaterialDiffuseColor==renderMaterialDiffuseColor
			&& a.renderMaterialSpecular==renderMaterialSpecular
			&& a.renderMaterialEmission==renderMaterialEmission
			&& a.renderMaterialTransparency==renderMaterialTransparency
			&& a.renderMaterialTransparencyNoise==renderMaterialTransparencyNoise
			&& a.renderMaterialTransparencyFresnel==renderMaterialTransparencyFresnel
			&& a.renderMaterialTransparencyRefraction==renderMaterialTransparencyRefraction
			&& a.renderMaterialBumpMaps==renderMaterialBumpMaps
			&& a.renderMaterialTextures==renderMaterialTextures
			&& a.renderMaterialSidedness==renderMaterialSidedness
			&& a.renderWireframe==renderWireframe
			&& a.renderFPS==renderFPS
			&& a.renderHelpers==renderHelpers
			&& a.renderBloom==renderBloom
			&& a.bloomThreshold==bloomThreshold
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
			&& (a.tonemapping.color==tonemapping.color || (renderTonemapping && tonemappingAutomatic)) // brightness may differ if automatic tonemapping is enabled
			&& a.tonemapping.gamma==tonemapping.gamma
			&& a.tonemapping.hsv==tonemapping.hsv
			&& a.tonemapping.steps==tonemapping.steps
			&& a.pathEnabled==pathEnabled
			&& a.pathShortcut==pathShortcut
			&& a.ssgiEnabled==ssgiEnabled
			&& a.ssgiIntensity==ssgiIntensity
			&& a.ssgiRadius==ssgiRadius
			&& a.ssgiAngleBias==ssgiAngleBias
			&& a.contoursEnabled==contoursEnabled
			&& a.contoursSilhouetteColor==contoursSilhouetteColor
			&& a.contoursCreaseColor==contoursCreaseColor
			&& a.playVideos==playVideos
			&& a.shadowTransparency==shadowTransparency
			&& a.videoEmittanceAffectsGI==videoEmittanceAffectsGI
			&& a.videoEmittanceGIQuality==videoEmittanceGIQuality
			&& a.videoTransmittanceAffectsGI==videoTransmittanceAffectsGI
			&& a.videoTransmittanceAffectsGIFull==videoTransmittanceAffectsGIFull
			&& a.videoEnvironmentAffectsGI==videoEnvironmentAffectsGI
			&& a.videoEnvironmentGIQuality==videoEnvironmentGIQuality
			&& a.fireballQuality==fireballQuality
			&& a.fireballWorkPerFrame==fireballWorkPerFrame
			&& a.fireballWorkTotal==fireballWorkTotal
			&& a.raytracedCubesEnabled==raytracedCubesEnabled
			&& a.raytracedCubesRes==raytracedCubesRes
			&& a.raytracedCubesMaxObjects==raytracedCubesMaxObjects
			&& a.raytracedCubesSpecularThreshold==raytracedCubesSpecularThreshold
			&& a.raytracedCubesDepthThreshold==raytracedCubesDepthThreshold
			&& a.mirrorsEnabled==mirrorsEnabled
			&& a.mirrorsDiffuse==mirrorsDiffuse
			&& a.mirrorsSpecular==mirrorsSpecular
			&& a.mirrorsMipmaps==mirrorsMipmaps
			&& a.mirrorsOcclusion==mirrorsOcclusion
			&& a.srgbCorrect==srgbCorrect
			&& a.lightmapFloats==lightmapFloats
			&& a.lightmapDirectParameters==lightmapDirectParameters
			&& a.lightmapFilteringParameters==lightmapFilteringParameters
			&& a.cameraDynamicNear==cameraDynamicNear
			&& a.cameraDynamicNearNumRays==cameraDynamicNearNumRays
			&& a.cameraMetersPerSecond==cameraMetersPerSecond
			&& a.renderGrid==renderGrid
			&& a.gridNumSegments==gridNumSegments
			&& a.gridSegmentSize==gridSegmentSize
			&& a.precision==precision
			&& a.openLogWindows==openLogWindows
			&& a.autodetectCamera==autodetectCamera
			;
	}
	bool operator !=(const SceneViewerState& a) const
	{
		return !(a==*this);
	}
};

//! Runs interactive scene viewer/editor.
//
//! \image html SceneViewer_2.jpg
//! Scene viewer includes debugging and editing features.
//! All lights and materials can be freely edited and skybox changed.
//! All lighting techniques for both realtime and precomputed GI are available via menu.
//! All effects can be enabled and configured via scene properties.
//!
//! Scene viewer can serve as a base for development of custom products.
//! If you need new feature added, we offer to do it for you or license you full scene viewer source code.
//!
//! \param inputSolver
//!  Solver to be displayed. This is handy for debugging scene already present in solver. May be nullptr.
//! \param inputFilename
//!  If inputSolver is nullptr, we attempt to open and display this file. This is handy for scene viewer applications.
//! \param skyboxFilename
//!  If RRBuffer::loadCube() loads this skybox successfully, it is used, overriding eventual environment from inputSolver.
//! \param pathToData
//!  Path to directory with data, where subdirectories maps, shaders are expected.
//!  Must be terminated with slash (or be empty for current dir).
//! \param svs
//!  Initial state of viewer. Use nullptr for default state with realtime GI and random camera.
//! \param releaseResources
//!  Resources allocated by scene viewer will be released on exit.
//!  It could take some time in huge scenes, so there's option to not release them, let them leak.
//!  Not releasing resources is good idea e.g. if you plan to exit application soon.
void RR_ED_API sceneViewer(rr::RRSolver* inputSolver, const rr::RRString& inputFilename, const rr::RRString& skyboxFilename, const rr::RRString& pathToData, SceneViewerState* svs, bool releaseResources);

}; // namespace

#endif
