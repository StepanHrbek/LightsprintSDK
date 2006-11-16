// --------------------------------------------------------------------------
// DemoEngine
// Settings, to be included in all DemoEngine public headers.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DEMOENGINE_H
#define DEMOENGINE_H

#ifdef _MSC_VER
	#ifdef DE_STATIC
		// use static library
		#define DE_API
		#ifdef NDEBUG
			#pragma comment(lib,"DemoEngine_sr.lib")
		#else
			#pragma comment(lib,"DemoEngine_sd.lib")
		#endif
	#else // use dll
		#pragma warning(disable:4251) // stop false MS warnings
		#ifdef DE_DLL_BUILD_DEMOENGINE
			// build dll
			#define DE_API __declspec(dllexport)
		#else
			// use dll
			#define DE_API __declspec(dllimport)
			//#ifdef NDEBUG
				#pragma comment(lib,"DemoEngine.lib")
			//#else
			//	#pragma comment(lib,"DemoEngine_dd.lib")
			//#endif
		#endif
	#endif
#else
	// use static library
	#define DE_API
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
