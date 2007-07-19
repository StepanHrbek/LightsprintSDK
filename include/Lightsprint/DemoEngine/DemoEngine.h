//----------------------------------------------------------------------------
//! \file DemoEngine.h
//! \brief LightsprintGL | library settings and macros
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2005-2007
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef DEMOENGINE_H
#define DEMOENGINE_H

#include "Lightsprint/RRMemory.h"

#ifdef _MSC_VER
#	ifdef RR_GL_STATIC
		// use static library
		#define RR_GL_API
#	else // use dll
#define RR_GL_API __declspec(dllimport)
#pragma warning(disable:4251) // stop MSVC warnings
#	endif
#else
	// use static library
	#define RR_GL_API
#endif

#ifndef RR_GL_MANUAL_LINK
#	ifdef _MSC_VER
#	ifdef RR_GL_STATIC
		// use static library
#		ifdef NDEBUG
#			pragma comment(lib,"LightsprintGL_sr.lib")
#		else
#			pragma comment(lib,"LightsprintGL_sd.lib")
#		endif
#	else // !RR_GL_STATIC
	#ifdef RR_GL_DLL_BUILD
		// build dll
		#undef RR_GL_API
		#define RR_GL_API __declspec(dllexport)
	#else
		// use dll
		#if _MSC_VER<1400
#			ifdef NDEBUG
				#ifdef RR_GL_DEBUG
					#pragma comment(lib,"LightsprintGL.vs2003_dd.lib")
				#else
					#pragma comment(lib,"LightsprintGL.vs2003.lib")
				#endif
#			else
				#pragma comment(lib,"LightsprintGL.vs2003_dd.lib")
#			endif
		#else
#			ifdef NDEBUG
				#ifdef RR_GL_DEBUG
					#pragma comment(lib,"LightsprintGL_dd.lib")
				#else
					#pragma comment(lib,"LightsprintGL.lib")
				#endif
#			else
				#pragma comment(lib,"LightsprintGL_dd.lib")
#			endif
		#endif
	#endif
#	endif // !RR_GL_STATIC
	#pragma comment(lib,"opengl32.lib")
	#pragma comment(lib,"glu32.lib")
	#pragma comment(lib,"glew32.lib")
#	endif // _MSC_VER
#endif // !RR_GL_MANUAL_LINK


// helper macros
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))
#define CLAMP(a,min,max) (a)=(((a)<(min))?min:(((a)>(max)?(max):(a))))
#define LIMITED_TIMES(times_max,action) {static unsigned times_done=0; if(times_done<times_max) {times_done++;action;}}
#ifndef SAFE_DELETE
	#define SAFE_DELETE(a)       {delete a;a=NULL;}
#endif
#ifndef SAFE_DELETE_ARRAY
	#define SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}
#endif

#endif
