//----------------------------------------------------------------------------
//! \file DemoEngine.h
//! \brief LightsprintGL | library settings and macros
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2013
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

	#if !defined(RR_GL_MANUAL_LINK) && !defined(RR_GL_BUILD)
		#ifdef RR_GL_STATIC
			// use static library
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_sr.lib")
			#else
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_sd.lib")
			#endif
		#else
			// use dll
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER ".lib")
			#else
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_dd.lib")
			#endif
		#endif
	#endif
#endif

#endif
