//----------------------------------------------------------------------------
//! \file RRDynamicSolverGL.h
//! \brief LightsprintGL | OpenGL based RRDynamicSolver
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2013
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RRDYNAMICSOLVERGL_H
#define RRDYNAMICSOLVERGL_H

#include "../RRDynamicSolver.h"
#include "Texture.h"
#include "Program.h"
#include "RendererOfScene.h"
#include "UberProgramSetup.h"

//! LightsprintGL - multiplatform OpenGL based renderer that displays realtime global illumination.
namespace rr_gl
{

	//! Implementation of rr::RRDynamicSolver generic GPU operations using OpenGL 2.0.
	//
	//! Before creating solver, make sure OpenGL context already exists,
	//! and OpenGL version is at least 2.0.
	class RR_GL_API RRDynamicSolverGL : public rr::RRDynamicSolver
	{
	public:
		enum DDIQuality
		{
			DDI_AUTO, // default, uses 8x8 for Geforce6000+ and Radeon1300+, 4x4 for the rest
			DDI_4X4, // lower quality, runs safely everywhere
			DDI_8X8, // higher quality, warning: buggy Radeon driver is known to crash on X300
		};

		//! Initializes generic GPU access implemented in RRDynamicSolverGL.
		//! \param pathToShaders
		//!  Path to directory with shaders.
		//!  Must be terminated with slash (or be empty for current dir).
		//! \param detectionQuality
		//!  Sets quality of our detectDirectIllumination() routine.
		RRDynamicSolverGL(const char* pathToShaders, DDIQuality detectionQuality = DDI_AUTO);
		virtual ~RRDynamicSolverGL();

		//! Sets lights used by both realtime and offline renderer.
		//
		//! For realtime rendering, #realtimeLights are created based on lights given here.
		//!
		//! It is legal to modify light properties after set, but not number of lights.
		//! Changes are processed when RealtimeLight::dirtyXxx flag is set.
		//! While renderer reads most of light properties from original lights,
		//! 'camera' properties like position, direction, fov are taken from #realtimeLights.
		//!
		//! By default, light's near/far range is autodetected from distance to static objects in solver (to avoid clipping),
		//! but if you wish to set range manually, clear RealtimeLight::dirtyRange after setLights()
		//! to prevent autodetection and set your own range.
		virtual void setLights(const rr::RRLights& lights);

		//! Renders scene in solver, with all static and dynamic objects, lights, environment.
		virtual void renderScene(const RenderParameters& renderParameters);

		//! Renders wireframe frustums or boxes of lights.
		virtual void renderLights(const rr::RRCamera& _camera);


		//! Sets dirty flags in given light, or in all lights if lightIndex is -1.
		virtual void reportDirectIlluminationChange(int lightIndex, bool dirtyShadowmap, bool dirtyGI, bool dirtyRange);

		//! Updates shadowmaps, detects direct illumination, calculates GI.
		//
		//! Call this function once per frame while rendering realtime shadows or realtime GI. You can call it even when
		//! not rendering, to improve GI solution inside solver (so that you have it ready when you render scene later).
		//! On the other hand, don't call it when you don't need realtime shadows and realtime GI,
		//! for example when rendering offline baked lightmaps, it would still spend time calculating realtime GI even if you don't need it.
		//!
		//! It updates illumination from dirty lights only, call reportDirectIlluminationChange() to mark light dirty.
		//! \n You can update only shadowmaps by setting params->qualityIndirectDynamic=0.
		//!
		//! Note that RRDynamicSolverGL::calculate() calls setDirectIllumination() automatically.
		//! If you want to provide your own direct illumination data, switch from RRDynamicSolverGL to
		//! rr::RRDynamicSolver and call setDirectIllumination() manually before calculate().
		virtual void calculate(CalculateParameters* params = NULL);

		enum { ENVMAP_RES_RASTERIZED = 32 };
		//! Calculates and updates object's environment map, stored in given layer of given illumination.
		//
		//! If map resolution is below ENVMAP_RES_RASTERIZED, function falls back to default implementation, which is realtime raytracer,
		//! using only diffuse color, no textures, with indirect illumination taken directly from solver.
		//! If resolution is ENVMAP_RES_RASTERIZED or higher, function renders map with LightsprintGL's renderer, honouring all light and material properties,
		//! with indirect illumination taken from buffers in layerLightmap.
		virtual unsigned updateEnvironmentMap(rr::RRObjectIllumination* illumination, unsigned layerEnvironment, unsigned layerLightmap, unsigned layerAmbientMap);

		//! Realtime lights, set by setLights(). You may modify them freely.
		rr::RRVector<RealtimeLight*> realtimeLights;
		//! Scene observer, inited to NULL. You may modify it freely. Shadow quality is optimized for observer.
		rr::RRCamera* observer;
		//! Users can reuse our uberprogram for their own rendering.
		UberProgram* getUberProgram() {return uberProgram1;}
		//! Users can reuse our renderer for their own rendering.
		RendererOfScene* getRendererOfScene() {return rendererOfScene;}

	protected:
		//! Detects direct illumination from lights (see setLights()) on all faces in scene and returns it in array of RGBA values.
		//! Result may be immediately passed to setDirectIllumination().
		//! \return Pointer to array of detected average per-triangle direct-lighting irradiances in custom scale
		//!  (= average triangle colors when material is not applied).
		//!  Values are stored in RGBA8 format.
		//!  Return NULL when direct illumination was not detected for any reason, this
		//!  function will be called again in next calculate().
		virtual const unsigned* detectDirectIllumination();
	private:
		//! Updates shadowmaps for lights with RealtimeLight::dirtyShadowmap flag set.
		//
		//! It also copies position and direction from RealtimeLight-s to RRLight-s.
		//! dirtyShadowmap flag is set when you call reportDirectIlluminationChange(,true,).
		//! \n Called by calculate().
		virtual void updateShadowmaps();
		//! Helper function called from detectDirectIllumination().
		unsigned detectDirectIlluminationTo(RealtimeLight* light, unsigned* results, unsigned space);

		// for DDI
		Texture* detectBigMap;
		Texture* detectSmallMap;
		Program* scaleDownProgram;
		DDIQuality detectionQuality;
		rr::RRTime lastDDITime;
		// for DDI of multiple lights
		unsigned* detectedDirectSum;
		unsigned detectedNumTriangles;
		unsigned lastDDINumLightsEnabled;

		// for internal rendering (shadowmaps)
		char pathToShaders[300];
		RendererOfScene* rendererOfScene;
		UberProgram* uberProgram1; // for updating shadowmaps and detecting direct illumination

		// for updating envmaps
		Texture* depthTexture;
	};

};

#endif
