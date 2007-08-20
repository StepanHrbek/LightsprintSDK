#ifndef GATHERER_H
#define GATHERER_H

#include "Lightsprint/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRObject/RRCollisionHandler.h" // SkipTriangle

namespace rr
{

// This should be part of RRDynamicSolver (it depends on RRStaticSolver and RRIllumination),
// but for higher speed, it reads data directly from RRStaticSolver internals. So it's here.

// For final gathering of many rays, one gatherer per thread.
// May be used 1000x for 1 final gathered texel, 640x480x for 1 raytraced image...
class Gatherer
{
public:
	Gatherer(RRRay* ray, const RRStaticSolver* staticSolver, const RRIlluminationEnvironmentMap* environment, const RRScaler* scaler);
	RRColor gather(RRVec3 eye, RRVec3 direction, unsigned skipTriangleNumber, RRColor visibility);
protected:
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
