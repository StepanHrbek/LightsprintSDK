//----------------------------------------------------------------------------
//! \file RRDynamicSolverGL.h
//! \brief LightsprintGL | OpenGL based RRDynamicSolver
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef RRDYNAMICSOLVERGL_H
#define RRDYNAMICSOLVERGL_H

#include "../RRDynamicSolver.h"
#include "Texture.h"
#include "Program.h"
#include "Renderer.h"
#include "UberProgramSetup.h"

//! LightsprintGL - OpenGL 2.0 renderer that displays realtime global illumination.
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
		//! Changes are processed when RealtimeLight::dirtyXxx flag is set.
		//! While renderer reads most of light properties from original lights,
		//! 'camera' properties like position, direction, fov are taken from #realtimeLights.
		virtual void setLights(const rr::RRLights& lights);
		virtual void setStaticObjects(const rr::RRObjects& objects, const SmoothingParameters* smoothing, const char* cacheLocation=NULL, rr::RRCollider::IntersectTechnique intersectTechnique=rr::RRCollider::IT_BSP_FASTER, rr::RRDynamicSolver* copyFrom = NULL);
		//! Renders whole scene, called by solver when updating shadowmaps. To be implemented by application.
		//! renderingFromThisLight is set only when rendering light view into shadowmap, otherwise NULL.
		virtual void renderScene(UberProgramSetup uberProgramSetup, const rr::RRLight* renderingFromThisLight) = 0;
		//! Renders wireframe frustums or boxes of lights.
		virtual void renderLights();

		virtual unsigned updateEnvironmentMap(rr::RRObjectIllumination* illumination);


		virtual void reportDirectIlluminationChange(unsigned lightIndex, bool dirtyShadowmap, bool dirtyGI);

		//! Updates shadowmaps, detects direct illumination, calculates GI.
		//
		//! Updates only dirty lights, call reportDirectIlluminationChange() to mark light dirty.
		//! \n You can update only shadowmaps by setting params->qualityIndirectDynamic=0.
		virtual void calculate(CalculateParameters* params = NULL);

		//! Realtime lights, set by setLights(). You may modify them freely.
		rr::RRVector<RealtimeLight*> realtimeLights;
		//! Scene observer, inited to NULL. You may modify it freely. Shadow quality is optimized for observer.
		Camera* observer;
		//! Returns minimal superset of all material features in static scene in MATERIAL_*.
		UberProgramSetup getMaterialsInStaticScene() {return materialsInStaticScene;}
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
		//! Sets shader so that feeding vertices+normals to rendering pipeline renders irradiance, incoming light
		//! without material. Helper function called from detectDirectIllumination().
		virtual Program* setupShader(unsigned objectNumber);
	private:
		//! Updates shadowmaps for lights with RealtimeLight::dirtyShadowmap flag set.
		//
		//! It also copies position and direction from RealtimeLight-s to RRLight-s.
		//! dirtyShadowmap flag is set when you call reportDirectIlluminationChange(,true,).
		//! \n Called by calculate().
		virtual void updateShadowmaps();
		//! Helper function called from detectDirectIllumination().
		virtual unsigned detectDirectIlluminationTo(unsigned* results, unsigned space);

		// for internal rendering (shadowmaps, DDI)
		char pathToShaders[300];
		class CaptureUv* captureUv;
		const void* rendererObject;
		class RendererOfRRObject* rendererNonCaching;
		Renderer* rendererCaching;
		Texture* detectBigMap;
		Program* scaleDownProgram;
		DDIQuality detectionQuality;
		UberProgram* uberProgram1; // for updating shadowmaps and detecting direct illumination
		double lastDDITime;

		// for GI of multiple lights
		RealtimeLight* setupShaderLight;
		unsigned* detectedDirectSum;
		unsigned detectedNumTriangles;
		rr::RRVec3 oldObserverPos;

		// Minimal superset of all material features in static scene, recommended MATERIAL_* setting for UberProgramSetup, detected from scene materials in setStaticObjects().
		UberProgramSetup materialsInStaticScene;
	};

};

#endif
