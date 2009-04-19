// --------------------------------------------------------------------------
// Fireball, lightning fast realtime GI solver.
// Copyright (c) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifndef RRPACKEDSOLVER_H
#define RRPACKEDSOLVER_H

#include "Lightsprint/RRObject.h"

//#define THREADED_BEST // sponza: pocet shooteru zvedne ze 100 na 132

namespace rr
{

class RRPackedSolver: public RRUniformlyAllocatedNonCopyable
{
public:
	static RRPackedSolver* create(const RRObject* object, const class PackedSolverFile* adopt_packedSolverFile);

	void setEnvironment(const RRBuffer* environment, const RRScaler* scaler);

	void illuminationReset(const unsigned* customDirectIrradiance, const RRReal* customToPhysical);
	void illuminationImprove(unsigned qualityDynamic, unsigned qualityStatic);

	// Triangle exitance, physical, flat. For dynamic objects/per-triangle materials.
	RRVec3 getTriangleExitance(unsigned triangle) const;

	// Triangle exitance, physical, flat. For dynamic objects/point materials.
	RRVec3 getTriangleIrradiance(unsigned triangle) const;

	// Triangle indirect irradiance, physical, gouraud, includes skylight. For static objects.
	// Pointer is guaranteed to stay constant, you can reuse it in next frames.
	// It may return NULL (for degenated and needle triangles).
	// Pointers are valid even without calling update, however data behind pointers
	// are valid only after calling update.
	void getTriangleIrradianceIndirectUpdate();
	const RRVec3* getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const;

	bool getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRVec3& out) const;

	//! Returns version of global illumination solution.
	//
	//! You may use this number to avoid unnecessary updates of illumination buffers.
	//! Store version together with your illumination buffers and don't update them
	//! (updateLightmaps()) until this number changes.
	unsigned getSolutionVersion() const {return currentVersionInTriangles;}

	~RRPackedSolver();

protected:
	RRPackedSolver(const RRObject* object, const class PackedSolverFile* adopt_packedSolverFile);

	// all objects exist, pointers are never NULL

	// constant precomputed data
	const class PackedSolverFile* packedSolverFile;

	// constant realtime acquired data
	const RRObject* object;
	class PackedTriangle* triangles;
	unsigned numTriangles;

	// varying data
	class PackedBests* packedBests;
	RRVec3* ivertexIndirectIrradiance;
	unsigned currentVersionInTriangles; // version of results available per triangle. reset, improve and setEnvironment may increment it
	unsigned currentVersionInVertices; // version of results available per vertex. getTriangleIrradianceIndirectUpdate() updates it to triangle version
	unsigned currentQuality; // number of best200 groups processed since reset
	RRReal terminalFluxToDistribute; // set during illuminationImprove(), when the best node has fluxToDistribute lower, improvement terminates
};

} // namespace

#endif
