// --------------------------------------------------------------------------
// DemoEngine
// Library settings, to be included in all DemoEngine public headers.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#ifndef DEMOENGINE_H
#define DEMOENGINE_H

#ifdef _MSC_VER
#ifdef DE_MANUAL_LINK
	#define DE_API
#else // !DE_MANUAL_LINK
#ifdef DE_STATIC
	// use static library
	#define DE_API
	#ifdef NDEBUG
		#pragma comment(lib,"DemoEngine_sr.lib")
	#else
		#pragma comment(lib,"DemoEngine_sd.lib")
	#endif
#else	// use dll
	#pragma warning(disable:4251) // stop false MS warnings
	#ifdef DE_DLL_BUILD_DEMOENGINE
		// build dll
		#define DE_API __declspec(dllexport)
	#else
		// use dll
		#define DE_API __declspec(dllimport)
		#if _MSC_VER<1400
#			ifdef NDEBUG
				#ifdef RR_DEBUG
					#pragma comment(lib,"DemoEngine.vs2003_dd.lib")
				#else
					#pragma comment(lib,"DemoEngine.vs2003.lib")
				#endif
#			else
				#pragma comment(lib,"DemoEngine.vs2003_dd.lib")
#			endif
		#else
#			ifdef NDEBUG
				#ifdef RR_DEBUG
					#pragma comment(lib,"DemoEngine_dd.lib")
				#else
					#pragma comment(lib,"DemoEngine.lib")
				#endif
#			else
				#pragma comment(lib,"DemoEngine_dd.lib")
#			endif
		#endif
	#endif
#endif
	#pragma comment(lib,"opengl32.lib")
	#pragma comment(lib,"glu32.lib")
	#pragma comment(lib,"glew32.lib")
#endif // !DE_MANUAL_LINK
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
#ifndef SAFE_DELETE
	#define SAFE_DELETE(a)       {delete a;a=NULL;}
#endif
#ifndef SAFE_DELETE_ARRAY
	#define SAFE_DELETE_ARRAY(a) {delete[] a;a=NULL;}
#endif

#include <new> // operators new/delete

namespace de
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRUniformlyAllocated
	//! Base class for objects allocated on our heap.
	//
	//! Allocating on our heap prevents corruption
	//! in environment with multiple heaps.
	//
	//////////////////////////////////////////////////////////////////////////////

	class DE_API RRUniformlyAllocated
	{
	public:
		//! Allocates aligned space for instance of any derived class.
		void* operator new(std::size_t n);
		//! Allocates aligned space for array of instances of any derived class.
		void* operator new[](std::size_t n);
		//! Frees aligned space allocated by new.
		void operator delete(void* p, std::size_t n);
		//! Frees aligned space allocated by new[].
		void operator delete[](void* p, std::size_t n);
	};

}; // namespace

#endif
