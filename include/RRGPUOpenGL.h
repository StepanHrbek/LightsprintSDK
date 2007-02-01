#ifndef RRGPUOPENGL_H
#define RRGPUOPENGL_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRGPUOpenGL.h
//! \brief RRGPUOpenGL - access to GPU via OpenGL
//! \version 2007.1.16
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

//! LightsprintGL - OpenGL 2.0 part of realtime global illumination solver.
namespace rr_gl
{

	//! Creates vertex buffer for indirect illumination storage.
	//! Used for realtime global illumination of static objects.
	RR_API rr::RRIlluminationVertexBuffer* createIlluminationVertexBuffer(unsigned numVertices);

	//! Creates 2d texture for indirect illumination storage.
	//! Used for precomputed global illumination of static objects.
	RR_API rr::RRIlluminationPixelBuffer* createIlluminationPixelBuffer(unsigned width, unsigned height);

	//! Creates cube texture for indirect illumination storage.
	//! Used for realtime or precomputed global illumination of dynamic objects.
	RR_API rr::RRIlluminationEnvironmentMap* createIlluminationEnvironmentMap();

	//! Detects direct illumination on every static face in scene and stores it into solver.
	RR_API void detectDirectIllumination(class rr::RRRealtimeRadiosity* solver);

};

#endif
