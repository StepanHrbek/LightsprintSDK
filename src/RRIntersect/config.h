
#define USE_SSE // by default, code is pure ANSI C++
//#define USE_SPHERE

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

#ifdef COLLIDER_INPUT_INVDIR
	// must be identical, box test is always first operation
	#define BOX_INPUT_INVDIR // by default, box.intersect input is only DIR
	// should be identical to allow use of already present invdir
	#define TRAVERSAL_INPUT_DIR_INVDIR // by default, all traversals use only DIR
#endif

#ifndef PRIVATE
	#define PRIVATE
#endif
