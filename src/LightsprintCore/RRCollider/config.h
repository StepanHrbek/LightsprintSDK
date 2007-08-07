#ifndef COLLIDER_CONFIG_H
#define COLLIDER_CONFIG_H

#include "Lightsprint/RRCollider.h"

//#define BUNNY_BENCHMARK_OPTIMIZATIONS // optimizations only for Bunny Benchmark, turns off unneeded features

#if defined(_MSC_VER) && defined(WIN32)
	#define USE_SSE // by default, code is pure ANSI C++. gcc compiles but crashes with sse
#endif
#define USE_FAST_BOX // fast box can't handle 2 special cases -> rare errors
//#define USE_EXPECT_HIT // always slower in rr, but may be useful for someone else later

#ifdef BUNNY_BENCHMARK_OPTIMIZATIONS
	// limited outputs
	#define FILL_HITDISTANCE
	#define FILL_HITTRIANGLE
	// no statistics
	#define FILL_STATISTIC(a)
	// limited inputs (no rayLength)
	#define COLLIDER_INPUT_UNLIMITED_DISTANCE // by default, collider.intersect expects that hitDistanceMin/Max contains range of allowed hitDistance
#else
	// full featureset
	#define FILL_HITDISTANCE
	#define FILL_HITTRIANGLE
	#define FILL_HITPOINT3D
	#define FILL_HITPOINT2D
	#define FILL_HITPLANE
	#define FILL_HITSIDE
	#define COLLISION_HANDLER
	// statistics
		#define FILL_STATISTIC(a)
#endif

#define COLLIDER_INPUT_INVDIR // partially hardcoded, must stay defined
#ifdef COLLIDER_INPUT_INVDIR
	// must be identical, box test is always first operation
	#define BOX_INPUT_INVDIR // by default, box.intersect input is only DIR
	// should be identical to allow use of already present invdir
	#define TRAVERSAL_INPUT_DIR_INVDIR // by default, all traversals use only DIR
#endif

#ifdef USE_SSE
	//#define FASTEST_USES_SSE // use SSE in FASTEST technique (more memory, slowdown)
#endif

#ifndef PRIVATE
	#define PRIVATE
#endif

#define rayDir         hitPadding[0]
#define hitDistanceMin hitPadding[1][0]
#define hitDistanceMax hitPadding[1][1]

#endif
