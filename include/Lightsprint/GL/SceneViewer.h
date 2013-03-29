//----------------------------------------------------------------------------
//! \file SceneViewer.h
//! \brief LightsprintGL | viewer of scene
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

// enforces comctl32.dll version 6 in all application that use sceneViewer() (such application must include this header or use following pragma)
// default version 5 creates errors we described in http://trac.wxwidgets.org/ticket/12709, 12710, 12711, 12112
#if !defined(RR_GL_STATIC)
	#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "Lightsprint/GL/RealtimeLight.h"
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
	LD_BAKED,                ///< Direct illumination is taken from lightmaps in layerBakedLightmap.
	LD_REALTIME,             ///< Direct illumination is realtime computed.
};

enum LightingIndirect
{
	LI_NONE,                 ///< No indirect illumination, Doom-3 look with shadows completely black.
	LI_CONSTANT,             ///< Constant ambient, widely used poor man's approach.
	LI_BAKED,                ///< Indirect illumination is taken from lightmaps in layerBakedLightmap or layerBakedAmbient.
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


//! Optional parameters of sceneViewer()
struct SceneViewerState
{
	rr::RRCamera     eye;                       //! Current camera.
	rr::RRTime       referenceTime;             //! Time when animation started/was in position 0.

	bool             envSimulateSky;            //! Should we simulate sky according to location and datetime?
	bool             envSimulateSun;            //! Should we simulate Sun according to location and datetime?
	float            envLongitudeDeg;           //! Longitude for sky/sun simulation, -180..180, east is positive, west is negative.
	float            envLatitudeDeg;            //! Latitude for sky/sun simulation, -90..90, north is positive, south is negative.
	float            envSpeed;                  //! How qualicky simulation runs, 0=stopped, 1=realtime, 3600=day in 24sec.

	unsigned         layerBakedLightmap;        //! Layer used for static global illumination.
	unsigned         layerBakedAmbient;         //! Layer used for static indirect illumination.
	unsigned         layerRealtimeAmbient;      //! Layer used for realtime lighting.
	unsigned         layerBakedLDM;             //! Layer used for light indirect maps, precomputed maps that modulate realtime indirect per-vertex.
	unsigned         layerBakedEnvironment;     //! Layer used for realtime environment, cubemaps.
	unsigned         layerRealtimeEnvironment;  //! Layer used for static environment, cubemaps.

	unsigned         selectedLightIndex;        //! Index into lights array, light controlled by mouse/arrows.
	unsigned         selectedObjectIndex;       //! Index into static objects array.
	bool             fullscreen;                //! Ignored. Fullscreen/windowed bit is saved to and read from user preferences file. Quit sceneViewer() in fullscreen and it will start in fullscreen next time.
	bool             renderStereo;              //! Enables interlaced stereo rendering.
	bool             renderDof;                 //! Render depth of field effect.
	bool             dofAutomatic;              //! For depth of field effect only: set dof near/far automatically.
	LightingDirect   renderLightDirect;         //! Direct illumination mode.
	LightingIndirect renderLightIndirect;       //! Indirect illumination mode.
	float            renderLightIndirectMultiplier; //! Makes indirect illumination this times brighter.
	bool             renderLDM;                 //! Modulate indirect illumination by LDM.
	bool             renderLDMEnabled() {return renderLDM && renderLightIndirect!=LI_BAKED && renderLightIndirect!=LI_NONE;}
	bool             renderLightmaps2d;         //! When not rendering realtime, show static lightmaps in 2D.
	bool             renderLightmapsBilinear;   //! Render lightmaps with bilinear interpolation rather than without it.
	bool             renderMaterialDiffuse;     //! Render diffuse color.
	bool             renderMaterialSpecular;    //! Render specular reflections.
	bool             renderMaterialEmission;    //! Render emissivity.
	Transparency     renderMaterialTransparency;//! Render transparency. Allows realtime renderer to use modes up to this one. Offline GI always uses the highest one.
	bool             renderMaterialTransparencyFresnel; //! When rendering transparency, modulate result by fresnel term.
	bool             renderMaterialTransparencyRefraction; //! When rendering transparency, approximate refraction with cubemaps.
	bool             renderMaterialBumpMaps;  //! Render normal maps.
	bool             renderMaterialTextures;    //! Render textures (diffuse, emissive) rather than constant colors.
	bool             renderMaterialSidedness;   //! Render 1-sided materials as 1-sided, rather than everything 2-sided.
	bool             renderWireframe;           //! Render all in wireframe.
	bool             renderFPS;                 //! Render FPS counter.
	bool             renderIcons;               //! Unused.
	bool             renderHelpers;             //! Show helper wireframe objects and text outputs.
	bool             renderBloom;               //! Render bloom effect.
	bool             renderLensFlare;           //! Render lens flare effect.
	float            lensFlareSize;             //! Relative lens flare size, 1 for typical size.
	unsigned         lensFlareId;               //! Other lens flare parameters are generated from this number.
	bool             renderVignette;            //! Render vignette overlay from sv_vignette.png.
	bool             renderHelp;                //! Render help overlay from sv_help.png.
	bool             renderLogo;                //! Render logo overlay from sv_logo.png.
	bool             renderTonemapping;         //! Render with tonemapping.
	rr::RRVec4       tonemappingBrightness;     //! If(renderTonemapping) Brightness applied at render time as simple multiplication, changed by tonemappingAutomatic.
	float            tonemappingGamma;          //! If(renderTonemapping) Gamma correction applied at render time, 1=no correction.
	bool             tonemappingAutomatic;      //! Automatically adjust tonemappingBrightness.
	float            tonemappingAutomaticTarget;//! Target average screen intensity for tonemappingAutomatic.
	float            tonemappingAutomaticSpeed; //! Speed of automatic tonemapping change.
	bool             playVideos;                //! Play videos, false = videos are paused.
	RealtimeLight::ShadowTransparency shadowTransparency; //! Type of transparency in shadows, we copy it to all lights.
	float            emissiveMultiplier;        //! Multiplies effect of emissive materials on scene, without affecting emissive materials.
	bool             videoEmittanceAffectsGI;   //! Makes video in emissive material slot affect GI in realtime, light emitted from video is recalculated in every frame.
	unsigned         videoEmittanceGIQuality;   //! Quality if videoEmittanceAffectsGI is true.
	bool             videoTransmittanceAffectsGI;//! Makes video in transparency material slot affect GI in realtime, light going through transparent regions is recalculated in every frame.
	bool             videoTransmittanceAffectsGIFull;//! Updates GI rather than shadows only.
	bool             videoEnvironmentAffectsGI; //! Makes video in environment affect GI in realtime, light emitted from video is recalculated in every frame.
	unsigned         videoEnvironmentGIQuality; //! Quality if videoEnvironmentAffectsGI is true.
	unsigned         fireballQuality;           //! Quality used each time Fireball needs rebuild.
	bool             raytracedCubesEnabled;     //! Enables realtime raytraced diffuse and specular cubemap reflections.
	unsigned         raytracedCubesRes;         //! Resolution of reflection env maps.
	unsigned         raytracedCubesMaxObjects;  //! But only if there is less than this number of objects in scene.
	float            raytracedCubesSpecularThreshold;
	float            raytracedCubesDepthThreshold;
	bool             mirrorsEnabled;            //! Enables rendering of flat reflections using mirror maps (rather than less accurate raytraced cubes).
	bool             mirrorsDiffuse;            //! Enables using mirrors when rendering diffuse reflection.
	bool             mirrorsSpecular;           //! Enables using mirrors when rendering specular reflection.
	bool             mirrorsMipmaps;            //! Enables higher quality mirroring mode using mipmaps.
	bool             srgbCorrect;               //! Add realtime lights sRGB correctly, if OpenGL 3.0+ or sufficient extensions are found.
	bool             lightmapFloats;
	//rr::RRDynamicSolver::CalculateParameters calculateParameters;        //! Realtime GI settings.
	rr::RRDynamicSolver::UpdateParameters    lightmapDirectParameters;   //! Lightmap baking settings.
	//rr::RRDynamicSolver::UpdateParameters    lightmapIndirectParameters; //! Lightmap baking settings.
	rr::RRDynamicSolver::FilteringParameters lightmapFilteringParameters;  //! Lightmap baking settings.
	bool             cameraDynamicNear;         //! Camera sets near dynamically to prevent near clipping.
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
		new(&eye) rr::RRCamera(rr::RRVec3(-1.856f,1.8f,2.097f), rr::RRVec3(2.404f,-0.3f,0), 1.3f, 90, 0.1f,1000);
		envSimulateSky = false;
		envSimulateSun = false;
		envLongitudeDeg = 14+26/60.f; // Prague
		envLatitudeDeg = 50+5/60.f;
		envSpeed = 0;

		layerBakedLightmap = 192837463; // any numbers unlikely to collide with user's layer numbers, better than 0 that nearly always collides
		layerBakedAmbient = 192837464;
		layerRealtimeAmbient = 192837465;
		layerBakedLDM = 192837466;
		layerBakedEnvironment = 192837467;
		layerRealtimeEnvironment = 192837468;
		selectedLightIndex = 0;
		selectedObjectIndex = 0;
		fullscreen = 0;
		renderStereo = false;
		renderDof = false;
		dofAutomatic = false;
		renderLightDirect = LD_REALTIME;
		renderLightIndirect = LI_REALTIME_FIREBALL;
		renderLightIndirectMultiplier = 1;
		renderLDM = true;
		renderLightmaps2d = 0;
		renderMaterialDiffuse = 1;
		renderMaterialSpecular = 1;
		renderMaterialEmission = 1;
		renderMaterialTransparency = T_RGB_BLEND;
		renderMaterialTransparencyFresnel = true;
		renderMaterialTransparencyRefraction = 0;
		renderMaterialBumpMaps = 1;
		renderMaterialTextures = 1;
		renderMaterialSidedness = true;
		renderWireframe = 0;
		renderFPS = 0;
		renderIcons = 1;
		renderHelpers = 0;
		renderLightmapsBilinear = 1;
		renderBloom = 0;
		renderLensFlare = 1;
		lensFlareSize = 8;
		lensFlareId = 204;
		renderVignette = 0;
		renderHelp = 0;
		renderLogo = 0;
		renderTonemapping = 1;
		tonemappingAutomatic = 1;
		tonemappingAutomaticTarget = 0.5f;
		tonemappingAutomaticSpeed = 1;
		tonemappingBrightness = rr::RRVec4(1);
		tonemappingGamma = 1;
		playVideos = 1;
		shadowTransparency = RealtimeLight::FRESNEL_SHADOWS;
		emissiveMultiplier = 1;
		videoEmittanceAffectsGI = true;
		videoEmittanceGIQuality = 5;
		videoTransmittanceAffectsGI = true;
		videoTransmittanceAffectsGIFull = true;
		videoEnvironmentAffectsGI = true;
		videoEnvironmentGIQuality = 300;
		fireballQuality = 350;
		raytracedCubesEnabled = true;
		raytracedCubesRes = 16;
		raytracedCubesMaxObjects = 1000;
		raytracedCubesSpecularThreshold = 0.2f;
		raytracedCubesDepthThreshold = 0.1f;
		mirrorsEnabled = true;
		mirrorsDiffuse = true;
		mirrorsSpecular = true;
		mirrorsMipmaps = true;
		srgbCorrect = true;
		lightmapFloats = false;
		lightmapDirectParameters.aoSize = 1;
		cameraDynamicNear = 1;
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
			&& a.eye==eye

			&& a.envSimulateSky==envSimulateSky
			&& a.envSimulateSun==envSimulateSun
			&& a.envLongitudeDeg==envLongitudeDeg
			&& a.envLatitudeDeg==envLatitudeDeg
			&& a.envSpeed==envSpeed

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
			&& a.renderDof==renderDof
			&& a.dofAutomatic==dofAutomatic
			&& a.renderLightDirect==renderLightDirect
			&& a.renderLightIndirect==renderLightIndirect
			&& a.renderLightIndirectMultiplier==renderLightIndirectMultiplier
			&& a.renderLDM==renderLDM
			&& a.renderLightmaps2d==renderLightmaps2d
			&& a.renderLightmapsBilinear==renderLightmapsBilinear
			&& a.renderMaterialDiffuse==renderMaterialDiffuse
			&& a.renderMaterialSpecular==renderMaterialSpecular
			&& a.renderMaterialEmission==renderMaterialEmission
			&& a.renderMaterialTransparency==renderMaterialTransparency
			&& a.renderMaterialTransparencyFresnel==renderMaterialTransparencyFresnel
			&& a.renderMaterialTransparencyRefraction==renderMaterialTransparencyRefraction
			&& a.renderMaterialBumpMaps==renderMaterialBumpMaps
			&& a.renderMaterialTextures==renderMaterialTextures
			&& a.renderMaterialSidedness==renderMaterialSidedness
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
			&& a.shadowTransparency==shadowTransparency
			&& a.emissiveMultiplier==emissiveMultiplier
			&& a.videoEmittanceAffectsGI==videoEmittanceAffectsGI
			&& a.videoEmittanceGIQuality==videoEmittanceGIQuality
			&& a.videoTransmittanceAffectsGI==videoTransmittanceAffectsGI
			&& a.videoTransmittanceAffectsGIFull==videoTransmittanceAffectsGIFull
			&& a.videoEnvironmentAffectsGI==videoEnvironmentAffectsGI
			&& a.videoEnvironmentGIQuality==videoEnvironmentGIQuality
			&& a.fireballQuality==fireballQuality
			&& a.raytracedCubesEnabled==raytracedCubesEnabled
			&& a.raytracedCubesRes==raytracedCubesRes
			&& a.raytracedCubesMaxObjects==raytracedCubesMaxObjects
			&& a.raytracedCubesSpecularThreshold==raytracedCubesSpecularThreshold
			&& a.raytracedCubesDepthThreshold==raytracedCubesDepthThreshold
			&& a.mirrorsEnabled==mirrorsEnabled
			&& a.mirrorsDiffuse==mirrorsDiffuse
			&& a.mirrorsSpecular==mirrorsSpecular
			&& a.mirrorsMipmaps==mirrorsMipmaps
			&& a.srgbCorrect==srgbCorrect
			&& a.lightmapFloats==lightmapFloats
			&& a.lightmapDirectParameters==lightmapDirectParameters
			&& a.lightmapFilteringParameters==lightmapFilteringParameters
			&& a.cameraDynamicNear==cameraDynamicNear
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
