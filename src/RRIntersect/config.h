#include "RRIntersect.h"

#define USE_SSE // by default, code is pure ANSI C++
//#define USE_SPHERE // everywhere -> slowdown
//#define USE_LONGJMP // bunny+msvc -> tiny slowdown

#ifdef BUNNY_BENCHMARK_OPTIMIZATIONS
	// limited featureset
	#define FILL_HITDISTANCE
	#define FILL_HITTRIANGLE
	// no statistics
	#define FILL_STATISTIC(a)
#else
	// full featureset
	#define FILL_HITDISTANCE
	#define FILL_HITTRIANGLE
	#define FILL_HITPOINT3D
	#define FILL_HITPOINT2D
	#define FILL_HITPLANE
	#define FILL_HITSIDE
	#define SURFACE_CALLBACK
	// statistics
	#define FILL_STATISTIC(a) a
#endif

#define COLLIDER_INPUT_INVDIR // partially hardcoded, must stay defined
#ifdef COLLIDER_INPUT_INVDIR
	// must be identical, box test is always first operation
	#define BOX_INPUT_INVDIR // by default, box.intersect input is only DIR
	// should be identical to allow use of already present invdir
	#define TRAVERSAL_INPUT_DIR_INVDIR // by default, all traversals use only DIR
#endif

#ifdef USE_SSE
	// helps only with intel compiler
	//#define FASTEST_USES_SSE // use SSE in FASTEST technique (more memory, hardly noticeably faster)
#endif

#ifndef PRIVATE
	#define PRIVATE
#endif

#define rayDir         hitPadding
#define hitDistanceMin hitPadding[4]
#define hitDistanceMax hitPadding[5]
