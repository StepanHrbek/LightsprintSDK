// --------------------------------------------------------------------------
// Final gather support.
// Copyright 2007-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef GATHERER_H
#define GATHERER_H

#include "../RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/RRIllumination.h" // toto je jedine misto kde kod z RRStaticSolver zavisi na RRIllumination
#include "../RRObject/RRCollisionHandler.h" // SkipTriangle

namespace rr
{
// Casts 1 ray with possible reflections/refractions, returns color visible in given direction.

// It is used only by RRDynamicSolver, but for higher speed,
// it reads data directly from RRStaticSolver internals, so it is here.

// For final gathering of many rays, use one gatherer per thread.
// May be used 1000x for 1 final gathered texel, 640x480x for 1 raytraced image...
class Gatherer
{
public:
	// Initializes helper structures for gather().
	//! \param gatherDirectEmitors
	//!  Gather direct exitance from emitors (stored in material).
	//! \param gatherIndirectLight
	//!  Gather indirect exitance (stored in static solver). May include indirect light computed from direct realtime lights, direct emitors, rrlights, env.
	Gatherer(RRRay* ray, const RRStaticSolver* staticSolver, const RRBuffer* environment, const RRScaler* scaler, bool gatherDirectEmitors, bool gatherIndirectLight);

	// Returns color visible in given direction, in physical scale.
	// May reflect/refract internally.
	// Individual calls to gather() are independent.
	RRVec3 gather(RRVec3 eye, RRVec3 direction, unsigned skipTriangleNumber, RRVec3 visibility);

protected:
	// helper structures
	RRRay* ray;
	SkipTriangle skipTriangle;
	RRObject* object;
	const RRCollider* collider;
	const RRBuffer* environment;
	const RRScaler* scaler;
	bool gatherDirectEmitors;
	bool gatherIndirectLight;
	class Triangle* triangle;
	unsigned triangles;
};

}; // namespace

#endif

