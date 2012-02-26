// --------------------------------------------------------------------------
// Compile-time configuration of ray-mesh intersections.
// Copyright (c) 2000-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef COLLIDER_CONFIG_H
#define COLLIDER_CONFIG_H

#include "Lightsprint/RRCollider.h"

//#define BUNNY_BENCHMARK_OPTIMIZATIONS // optimizations only for Bunny Benchmark, turns off unneeded features

#if defined(_MSC_VER) && defined(_WIN32)
	#define USE_SSE // by default, code is pure ANSI C++. gcc compiles but crashes with sse, probably alignment issue
#endif
#define USE_FAST_BOX // fast box can't handle 2 special cases -> rare errors

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

//#define COLLIDER_INPUT_INVDIR
#ifdef COLLIDER_INPUT_INVDIR
	// must be identical to collider input, box test is always first operation
	#define BOX_INPUT_INVDIR // by default, box.intersect input is only DIR
	// should be identical to allow use of already present invdir
	#define TRAVERSAL_INPUT_DIR_INVDIR // by default, all traversals use only DIR
	#define rayDir         hitPadding1
#else
	// must be identical to collider input, box test is always first operation
	//#define BOX_INPUT_INVDIR
	// full traversal uses both
	#define TRAVERSAL_INPUT_DIR_INVDIR
	#define rayDirInv      hitPadding1
#endif

#ifdef USE_SSE
	//#define FASTEST_USES_SSE // use SSE in FASTEST technique (more memory, slowdown)
#endif

#ifndef PRIVATE
	#define PRIVATE
#endif

#define hitDistanceMin hitPadding2[0]
#define hitDistanceMax hitPadding2[1]

#endif
