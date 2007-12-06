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

		//! Creates 2d texture for indirect illumination storage.
		//! Used for precomputed global illumination of static objects.
		//! \param width Width of texture.
		//! \param height Height of texture.
		rr::RRIlluminationPixelBuffer* createIlluminationPixelBuffer(unsigned width, unsigned height);

		//! Loads RRIlluminationPixelBuffer stored on disk.
		rr::RRIlluminationPixelBuffer* loadIlluminationPixelBuffer(const char* filename);

		//! Captures direct illumination on object's surface into lightmap, using GPU.
		//
		//! Lightmap uses uv coordinates provided by RRMesh::getTriangleMapping(),
		//! the same coordinates are used for ambient map.
		//!
		//! This function will be unified with RRDynamicSolver::updateLightmap() in future release.
		//!
		//! \param objectNumber
		//!  Number of object in this scene.
		//!  Object numbers are defined by order in which you pass objects to setStaticObjects().
		//! \param lightmap
		//!  Pixel buffer for storing calculated lightmap.
		//!  Lightmap holds direct irradiance in custom scale, which is light from
		//!  realtime light sources (point/spot/dir/area lights) coming to object's surface.
		bool updateLightmap_GPU(unsigned objectNumber, rr::RRIlluminationPixelBuffer* lightmap);


		//! Creates cube texture for indirect illumination storage.
		//! Used for realtime or precomputed global illumination of dynamic objects.
		static rr::RRIlluminationEnvironmentMap* createIlluminationEnvironmentMap();

		//! Adapts cube texture for RRIlluminationEnvironmentMap interface.
		//! Original cube must exist as long as or longer than returned adapter.
		static rr::RRIlluminationEnvironmentMap* adaptIlluminationEnvironmentMap(Texture* cube);

		//! Loads RRIlluminationEnvironmentMap stored on disk.
		//! \param filenameMask
		//!   Name of image file. Must be in supported format.
		//!   For cube textures, filename may contain \%s wildcard -> 6 images will be loaded with %s replaced by cubeSideName[side].
		//!   \n Example1: "path/cube.hdr" - cube in 1 image.
		//!   \n Example2: "path/cube_%s.png" - cube in 6 images.
		//! \param cubeSideName
		//!   Array of six unique names of cube sides in following order:
		//!   x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!   Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
		//! \param flipV
		//!   Flip each image vertically at load time.
		//! \param flipH
		//!   Flip each image horizontally at load time.
		static rr::RRIlluminationEnvironmentMap* loadIlluminationEnvironmentMap(const char* filenameMask, const char* cubeSideName[6], bool flipV = false, bool flipH = false);

		//! Loads illumination layer from disk.
		unsigned loadIllumination(const char* path, unsigned layerNumber, bool vertexColors, bool lightmaps);
		//! Saves illumination layer to disk.
		unsigned saveIllumination(const char* path, unsigned layerNumber, bool vertexColors, bool lightmaps);

		//! Deletes helper objects stored permanently to speed up some operations.
		//! Calling it makes sense if you detect memory leaks.
		static void cleanup();

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
