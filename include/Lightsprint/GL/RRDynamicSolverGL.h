//----------------------------------------------------------------------------
//! \file RRDynamicSolverGL.h
//! \brief LightsprintGL | OpenGL based RRDynamicSolver
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RRDYNAMICSOLVERGL_H
#define RRDYNAMICSOLVERGL_H

#include "../RRDynamicSolver.h"
#include "Texture.h"
#include "Program.h"
#include "Renderer.h"
#include "UberProgramSetup.h"

//! LightsprintGL - OpenGL 2.0 part of realtime global illumination solver.
namespace rr_gl
{

	//! Implementation of rr::RRDynamicSolver generic GPU operations using OpenGL 2.0.
	//
	//! This is not complete implementation of rr::RRDynamicSolver,
	//! it contains generic GPU access operations, but not operations specific to your renderer.
	//! You need to subclass RRDynamicSolverGL and implement remaining operations specific to your renderer.
	//! See RealtimeRadiosity sample for an example of such implementation.
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
		//!   Path to directory with lightmap_build.*, lightmap_filter.* and scaledown_filter.* shaders.
		//!   Must be terminated with slash (or be empty for current dir).
		//! \param detectionQuality
		//!   Sets quality of our detectDirectIllumination() routine.
		RRDynamicSolverGL(const char* pathToShaders, DDIQuality detectionQuality = DDI_AUTO);
		virtual ~RRDynamicSolverGL();

		//! Sets lights used by both realtime and offline renderer.
		//
		//! For realtime rendering, #realtimeLights are created based on lights given here.
		//!
		//! It is legal to modify light properties after set, but not number of lights.
		//! Changes are processed when RealtimeLight::dirty flag is set.
		//! While renderer reads most of light properties from original lights,
		//! 'camera' properties like position, direction, fov are taken from #realtimeLights.
		virtual void setLights(const rr::RRLights& lights);
		//! Updates shadowmaps and GI from lights with RealtimeLight::dirty set. Called by solver in response to reportDirectIlluminationChange().
		virtual void updateDirtyLights();
		//! Renders whole scene, called by solver when updating shadowmaps. To be implemented by application. renderingFromThisLight is set only when rendering light view into shadowmap, otherwise NULL.
		virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight) = 0;
		//! Renders lights (wireframe shadow envelopes).
		virtual void renderLights();

		virtual unsigned updateEnvironmentMap(rr::RRObjectIllumination* illumination);


		virtual void calculate(CalculateParameters* params = NULL);
		//! Sets shader so that feeding vertices+normals to rendering pipeline renders irradiance, incoming light
		//! without material. Helper function called from detectDirectIllumination().
		virtual void setupShader(unsigned objectNumber);
		//! Helper function called from detectDirectIllumination().
		virtual unsigned detectDirectIlluminationTo(unsigned* results, unsigned space);
		//! Detection of direct illumination from lights (see setLights()) implemented using OpenGL 2.0.
		virtual unsigned* detectDirectIllumination();

		//! Realtime lights, set by setLights(). You may modify them freely.
		rr::RRVector<RealtimeLight*> realtimeLights;
		//! Scene observer, inited to NULL. You may modify it freely. Shadow quality is optimized for observer.
		Camera* observer;
		//! Whether update of shadowmaps and detection of direct illum honours expensive lighting&shadowing flags.
		//! Inited to false, you may freely change it.
		bool honourExpensiveLightingShadowingFlags;
	private:
		// for internal rendering
		char pathToShaders[300];
		class CaptureUv* captureUv;
		const void* rendererObject;
		class RendererOfRRObject* rendererNonCaching;
		Renderer* rendererCaching;
		Texture* detectBigMap;
		Program* scaleDownProgram;
		DDIQuality detectionQuality;
		UberProgram* uberProgram1; // for updating shadowmaps and detecting direct illumination
		// for GI of multiple lights
		RealtimeLight* setupShaderLight;
		unsigned* detectedDirectSum;
		unsigned detectedNumTriangles;
		rr::RRVec3 oldObserverPos;
	};

};

#endif
