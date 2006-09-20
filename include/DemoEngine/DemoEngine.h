// --------------------------------------------------------------------------
// DemoEngine
// Settings, to be included in all DemoEngine public headers.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DEMOENGINE_H
#define DEMOENGINE_H

#include "RRMath.h" // defines RR_API

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
			#pragma comment(lib,"DemoEngine_sr.lib")
		#else
			#pragma comment(lib,"DemoEngine_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_DEMOENGINE
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"DemoEngine.lib")
#	endif
#	endif
#endif

// helper macros
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
#define CLAMP(a,min,max) (a)=(((a)<(min))?min:(((a)>(max)?(max):(a))))
#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}

#endif
