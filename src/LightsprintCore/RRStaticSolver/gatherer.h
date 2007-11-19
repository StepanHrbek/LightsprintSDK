#ifndef GATHERER_H
#define GATHERER_H

#include "../RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRObject/RRCollisionHandler.h" // SkipTriangle

namespace rr
{
// Casts 1 ray with possible reflections/refractions, returns color visible in given direction.

// This should be part of RRDynamicSolver (it depends on RRStaticSolver and RRIllumination),
// but for higher speed, it reads data directly from RRStaticSolver internals. So it's here.

// For final gathering of many rays, use one gatherer per thread.
// May be used 1000x for 1 final gathered texel, 640x480x for 1 raytraced image...
class Gatherer
{
public:
	// Initializes helper structures for gather().
	Gatherer(RRRay* ray, const RRStaticSolver* staticSolver, const RRIlluminationEnvironmentMap* environment, const RRScaler* scaler);

	// Returns color visible in given direction, in physical scale.
	// May reflect/refract internally.
	// Individual calls to gather() are independent.
	RRColor gather(RRVec3 eye, RRVec3 direction, unsigned skipTriangleNumber, RRColor visibility);

protected:
	// helper structures
	RRRay* ray;
	SkipTriangle skipTriangle;
	RRObject* object;
	const RRCollider* collider;
	const RRIlluminationEnvironmentMap* environment;
	const RRScaler* scaler;
	class Triangle* triangle;
	unsigned triangles;
};

}; // namespace

#endif
