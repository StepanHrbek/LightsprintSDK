//----------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file RRSolverGL.h
//! \brief LightsprintGL | OpenGL based RRSolver
//----------------------------------------------------------------------------

#ifndef RRSOLVERGL_H
#define RRSOLVERGL_H

#include "../RRSolver.h"
#include "Texture.h"
#include "Program.h"
#include "Renderer.h"
#include "UberProgramSetup.h"

//! LightsprintGL - OpenGL based renderer that displays realtime global illumination.
namespace rr_gl
{

	//! Implementation of rr::RRSolver generic GPU operations using OpenGL 2.0.
	//
	//! Before creating solver, make sure OpenGL context already exists,
	//! and OpenGL version is at least 2.0.
	class RR_GL_API RRSolverGL : public rr::RRSolver
	{
	public:
		enum DDIQuality
		{
			DDI_AUTO, // default, uses 8x8 for Geforce6000+ and Radeon1300+, 4x4 for the rest
			DDI_4X4, // lower quality, runs safely everywhere
			DDI_8X8, // higher quality, warning: buggy Radeon driver is known to crash on X300
		};

		//! Initializes generic GPU access implemented in RRSolverGL.
		//! \param pathToShaders
		//!  Path to directory with shaders.
		//!  Must be terminated with slash (or be empty for current dir).
		//!  It is also passed to renderer and its plugins.
		//! \param pathToMaps
		//!  Path to directory with maps.
		//!  Must be terminated with slash (or be empty for current dir).
		//!  It is passed to renderer and its plugins, solver itself doesn't use any maps.
		//! \param detectionQuality
		//!  Sets quality of our detectDirectIllumination() routine.
		RRSolverGL(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps, DDIQuality detectionQuality = DDI_AUTO);
		virtual ~RRSolverGL();

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
		//virtual void renderScene(const RenderParameters& renderParameters);

		//! Renders wireframe frustums or boxes of lights.
		virtual void renderLights(const rr::RRCamera& _camera);


		//! Sets dirty flags in given light, or in all lights if lightIndex is -1.
		virtual void reportDirectIlluminationChange(int lightIndex, bool dirtyShadowmap, bool dirtyGI, bool dirtyRange) override;

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
		//! Note that RRSolverGL::calculate() calls setDirectIllumination() automatically.
		//! If you want to provide your own direct illumination data, switch from RRSolverGL to
		//! rr::RRSolver and call setDirectIllumination() manually before calculate().
		virtual void calculate(CalculateParameters* params = NULL) override;

		//! Calculates and updates object's environment map, stored in given layer of given illumination.
		//
		//! Function renders map with LightsprintGL's renderer, honouring all light and material properties,
		//! with indirect illumination taken from buffers in layerXxx.
		//! If your envmap has low resolution (less than approximately 32x32x6), consider calling RRSolver::updateEnvironmentMap() instead, which is faster.
		virtual unsigned updateEnvironmentMap(rr::RRObjectIllumination* illumination, unsigned layerEnvironment, unsigned layerLightmap, unsigned layerAmbientMap) override;

		//! Realtime lights, set by setLights(). You may modify them freely.
		rr::RRVector<RealtimeLight*> realtimeLights;
		//! Scene observer, inited to NULL. You may modify it freely. Shadow quality is optimized for observer.
		rr::RRCamera* observer;
		//! Users can reuse our uberprogram for their own rendering.
		UberProgram* getUberProgram() {return uberProgram1;}
		//! Users can reuse our renderer for their own rendering.
		Renderer* getRenderer() {return renderer;}

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
		Renderer* renderer;
		UberProgram* uberProgram1; // for updating shadowmaps and detecting direct illumination

		// for updating envmaps
		unsigned updatingEnvironmentMap;
		Texture* depthTexture;
	};

};

#endif
