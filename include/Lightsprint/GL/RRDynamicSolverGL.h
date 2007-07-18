// --------------------------------------------------------------------------
// OpenGL based RRDynamicSolver.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef RRDYNAMICSOLVERGL_H
#define RRDYNAMICSOLVERGL_H

#include "../RRDynamicSolver.h"

#ifndef RR_MANUAL_LINK
#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#ifdef NDEBUG
			#pragma comment(lib,"LightsprintGL_sr.lib")
		#else
			#pragma comment(lib,"LightsprintGL_sd.lib")
		#endif
#	else
#ifdef RR_DLL_BUILD_GPUOPENGL
	// build dll
	#undef RR_API
	#define RR_API __declspec(dllexport)
#else
	// use dll
	#if _MSC_VER<1400
#		ifdef NDEBUG
			#ifdef RR_DEBUG
				#pragma comment(lib,"LightsprintGL.vs2003_dd.lib")
			#else
				#pragma comment(lib,"LightsprintGL.vs2003.lib")
			#endif
#		else
			#pragma comment(lib,"LightsprintGL.vs2003_dd.lib")
#		endif
	#else
#		ifdef NDEBUG
			#ifdef RR_DEBUG
				#pragma comment(lib,"LightsprintGL_dd.lib")
			#else
				#pragma comment(lib,"LightsprintGL.lib")
			#endif
#		else
			#pragma comment(lib,"LightsprintGL_dd.lib")
#		endif
	#endif
#endif
#	endif
#endif
#endif

#include "../DemoEngine/Texture.h"
#include "../DemoEngine/Program.h"
#include "../DemoEngine/Renderer.h"
#include "../DemoEngine/UberProgramSetup.h"

//! LightsprintGL - OpenGL 2.0 part of realtime global illumination solver.
namespace rr_gl
{

	//! Implementation of rr::RRDynamicSolver generic GPU operations using OpenGL 2.0.
	//
	//! This is not complete implementation of rr::RRDynamicSolver,
	//! it contains generic GPU access operations, but not operations specific to your renderer.
	//! You need to subclass RRDynamicSolverGL and implement remaining operations specific to your renderer.
	//! See RealtimeRadiosity sample for an example of such implementation.
	class RR_API RRDynamicSolverGL : public rr::RRDynamicSolver
	{
	public:
		//! Initializes generic GPU access implemented in RRDynamicSolverGL.
		//! \param pathToShaders
		//!   Path to directory with lightmap_build.*, lightmap_filter.* and scaledown_filter.* shaders.
		//!   Must be terminated with slash (or be empty for current dir).
		RRDynamicSolverGL(char* pathToShaders);
		virtual ~RRDynamicSolverGL();


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
		//!  Object numbers are defined by order in which you pass objects to setObjects().
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
		static rr::RRIlluminationEnvironmentMap* adaptIlluminationEnvironmentMap(de::Texture* cube);

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

	protected:
		//! Detection of direct illumination from custom light sources implemented using OpenGL 2.0.
		virtual bool detectDirectIllumination();


		//! Sets shader so that feeding vertices+normals to rendering pipeline renders irradiance, incoming light
		//! without material. This is renderer specific operation and can't be implemented here.
		virtual void setupShader(unsigned objectNumber) = 0;

		//! Multiplies direct illumination detected by detectDirectIllumination(). Default value is 1 (no change, physically correct).
		float boostDetectedDirectIllumination;
	private:
		char pathToShaders[300];
		// for internal rendering
		class CaptureUv* captureUv;
		const void* rendererObject;
		class RendererOfRRObject* rendererNonCaching;
		de::Renderer* rendererCaching;
		de::Texture* detectBigMap;
		unsigned* detectSmallMap;
		unsigned smallMapSize;
		de::Program* scaleDownProgram;
		// backup of render states
		int viewport[4];
		unsigned char depthTest, depthMask;
		float clearcolor[4];
	};

};

#endif
