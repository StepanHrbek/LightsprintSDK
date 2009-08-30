//----------------------------------------------------------------------------
//! \file DemoEngine.h
//! \brief LightsprintGL | library settings and macros
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef DEMOENGINE_H
#define DEMOENGINE_H

#include "Lightsprint/RRMemory.h"

// define RR_GL_API
#ifdef _MSC_VER
	#ifdef RR_GL_STATIC
		// build or use static library
		#define RR_GL_API
	#elif defined(RR_GL_BUILD)
		// build dll
		#define RR_GL_API __declspec(dllexport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#else
		// use dll
		#define RR_GL_API __declspec(dllimport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#endif
#else
	// build or use static library
	#define RR_GL_API
#endif

// autolink library when external project includes this header
#ifdef _MSC_VER
	// for opengl
	#pragma comment(lib,"opengl32.lib")
	#pragma comment(lib,"glu32.lib")
	#pragma comment(lib,"glew32.lib")

	// for wxWidgets
	#pragma comment(lib,"rpcrt4.lib")
	#pragma comment(lib,"comctl32.lib")
	#pragma comment(lib,"winmm.lib")

	#if !defined(RR_GL_MANUAL_LINK) && !defined(RR_GL_BUILD)
		#ifdef RR_GL_STATIC
			// use static library
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2003_sr.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2003_sd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2005_sr.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2005_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2008_sr.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2008_sd.lib")
				#endif
			#endif
		#else
			// use dll
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2003.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2003_dd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2005.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2005_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintGL.vs2008.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2008_dd.lib")
				#endif
			#endif
		#endif
	#endif
#endif

#endif
