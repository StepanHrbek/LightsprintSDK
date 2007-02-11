#ifndef RRGPUOPENGL_H
#define RRGPUOPENGL_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRGPUOpenGL.h
//! \brief RRGPUOpenGL - access to GPU via OpenGL
//! \version 2007.2.7
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRRealtimeRadiosity.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#ifdef NDEBUG
			#pragma comment(lib,"RRGPUOpenGL_sr.lib")
		#else
			#pragma comment(lib,"RRGPUOpenGL_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_GPUOPENGL
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#ifdef NDEBUG
	#pragma comment(lib,"RRGPUOpenGL.lib")
#else
	#pragma comment(lib,"RRGPUOpenGL_dd.lib")
#endif
#	endif
#	endif
#endif

#include "DemoEngine/Texture.h"
#include "DemoEngine/Program.h"
#include "DemoEngine/Renderer.h"

//! LightsprintGL - OpenGL 2.0 part of realtime global illumination solver.
namespace rr_gl
{

	//! Implementation of rr::RRRealtimeRadiosity generic GPU operations using OpenGL 2.0.
	//
	//! This is not complete implementation of rr::RRRealtimeRadiosity,
	//! it contains generic GPU access operations, but not operations specific to your renderer.
	//! You need to subclass RRRealtimeRadiosityGL and implement remaining operations specific to your renderer.
	//! See HelloRealtimeRadiosity for an example of such implementation.
	class RR_API RRRealtimeRadiosityGL : public rr::RRRealtimeRadiosity
	{
	public:
		//! Initializes generic GPU access implemented in RRRealtimeRadiosityGL.
		//! \param pathToShaders
		//!   Path to directory with lightmap_filter.* and scaledown_filter.* shaders.
		//!   Must be terminated with slash (or be empty for current dir).
		RRRealtimeRadiosityGL(char* pathToShaders);
		virtual ~RRRealtimeRadiosityGL();

		//! Creates 2d texture for indirect illumination storage.
		//! Used for precomputed global illumination of static objects.
		//! \param width Width of texture.
		//! \param width Height of texture.
		//! \param swapChannels Set to true only on buggy AMD drivers that swap R and B channels.
		rr::RRIlluminationPixelBuffer* createIlluminationPixelBuffer(unsigned width, unsigned height, bool swapChannels = false);
		//! Loads RRIlluminationPixelBuffer stored on disk.
		rr::RRIlluminationPixelBuffer* loadIlluminationPixelBuffer(const char* filename);

		//! Creates cube texture for indirect illumination storage.
		//! Used for realtime or precomputed global illumination of dynamic objects.
		static rr::RRIlluminationEnvironmentMap* createIlluminationEnvironmentMap();
		//! Loads RRIlluminationEnvironmentMap stored on disk.
		//! \param filenameMask
		//!   Name of image file. Must be in supported format.
		//!   For cube textures, filename must contain %s wildcard, that will be replaced by cubeSideName.
		//!   Example: "/maps/cube_%s.png".
		//! \param cubeSideName
		//!   Array of six unique names of cube sides in following order:
		//!   x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!   Examples: {"0","1","2","3","4","5"}, {"rt","lf","up","dn","ft","bk"}.
		static rr::RRIlluminationEnvironmentMap* loadIlluminationEnvironmentMap(const char* filenameMask, const char* cubeSideName[6]);

	protected:
		//! Detection of direct illumination implemented using OpenGL 2.0.
		virtual bool detectDirectIllumination();
		//! Sets shader so that feeding vertices+normals to rendering pipeline renders irradiance, incoming light
		//! without material. This is renderer specific operation and can't be implemented in this generic class.
		virtual void setupShader() = 0;

	private:
		char pathToShaders[300];
		// for internal rendering
		class CaptureUv* captureUv;
		class RendererOfRRObject* rendererNonCaching;
		de::Renderer* rendererCaching;
		de::Texture* detectBigMap;
		unsigned* detectSmallMap;
		unsigned smallMapSize;
		de::Program* scaleDownProgram;
		// backup of render states
		GLint viewport[4];
		GLboolean depthTest, depthMask;
		GLfloat clearcolor[4];
	};

};

#endif
