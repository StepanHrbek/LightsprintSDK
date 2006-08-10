// --------------------------------------------------------------------------
// DemoEngine
// Settings, to be included in all DemoEngine public headers.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
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

#endif
