// helps gcc to produce faster code (speedup in bunnytest=1%..probably not worth much longer compile time)
// you can enable this speedup for gcc by uncommenting line "OBJS = big.o" in makefile

#define PRIVATE static
        
#include "bsp.cpp"
#include "cache.cpp"
#include "dll.cpp"
#include "geometry.cpp"
#include "IntersectBspCompact.cpp"
#include "IntersectBspFast.cpp"
#include "IntersectLinear.cpp"
#include "RRIntersect.cpp"
#include "sha1.cpp"

#if PRIVATE!=static
	#error 1
#endif
