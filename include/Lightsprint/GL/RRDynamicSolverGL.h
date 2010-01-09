//----------------------------------------------------------------------------
//! \file RRDynamicSolverGL.h
//! \brief LightsprintGL | OpenGL based RRDynamicSolver
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2010
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RRDYNAMICSOLVERGL_H
#define RRDYNAMICSOLVERGL_H

#include "../RRDynamicSolver.h"
#include "Texture.h"
#include "Program.h"
#include "RendererOfScene.h"
#include "UberProgramSetup.h"

//! LightsprintGL - OpenGL 2.0 renderer that displays realtime global illumination.
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
		//! Initial near/far range for shadowmapping is set here (calculated from distance to static scene).
		//! If you don't change range later in your code, make sure that static scene is already set (setStaticObjects)
		//! and lights are in their typical positions when you call setLights().
		virtual void setLights(const rr::RRLights& lights);

		//! Renders scene in solver, with all static and dynamic objects, lights, environment.
		//! 
		//! Renderer uses camera actually set in OpenGL fixed pipeline, so you can set it by standard OpenGL commands
		//! or by our Camera::setupForRender().
		virtual void renderScene(
			const UberProgramSetup& _uberProgramSetup,
			const rr::RRLight* _renderingFromThisLight,
			bool _updateLightIndirect,
			unsigned _lightIndirectLayer,
			int _lightDetailMapLayer,
			float _clipPlaneY,
			const rr::RRVec4* _brightness,
			float _gamma);

		//! Renders wireframe frustums or boxes of lights.
		virtual void renderLights();


		virtual void reportDirectIlluminationChange(unsigned lightIndex, bool dirtyShadowmap, bool dirtyGI);

		//! Updates shadowmaps, detects direct illumination, calculates GI.
		//
		//! Updates only dirty lights, call reportDirectIlluminationChange() to mark light dirty.
		//! \n You can update only shadowmaps by setting params->qualityIndirectDynamic=0.
		//!
		//! Note that RRDynamicSolverGL::calculate() calls setDirectIllumination() automatically.
		//! If you want to provide your own direct illumination data, switch from RRDynamicSolverGL to
		//! rr::RRDynamicSolver and call setDirectIllumination() manually before calculate().
		virtual void calculate(CalculateParameters* params = NULL);

		//! Realtime lights, set by setLights(). You may modify them freely.
		rr::RRVector<RealtimeLight*> realtimeLights;
		//! Scene observer, inited to NULL. You may modify it freely. Shadow quality is optimized for observer.
		Camera* observer;
		//! Users can reuse our uberprogram for their own rendering.
		UberProgram* getUberProgram() {return uberProgram1;}

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
		double lastDDITime;
		// for DDI of multiple lights
		unsigned* detectedDirectSum;
		unsigned detectedNumTriangles;

		// for internal rendering (shadowmaps)
		char pathToShaders[300];
		RendererOfScene* rendererOfScene;
		UberProgram* uberProgram1; // for updating shadowmaps and detecting direct illumination
		rr::RRVec3 oldObserverPos;
	};

};

#endif
