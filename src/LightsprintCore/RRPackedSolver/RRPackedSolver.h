// --------------------------------------------------------------------------
// Fireball, lightning fast realtime GI solver.
// Copyright (c) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
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
	//! Creates packed solver (fireball).
	//
	//! Initial state is: no light sources.
	//! To enable GI from emissive materials, call setEmittance().
	//! To enable GI from environment, call setEnvironment().
	static RRPackedSolver* create(const RRObject* object, const class PackedSolverFile* adopt_packedSolverFile);

	void setEnvironment(const RRBuffer* environment, const RRScaler* scaler);

	//! See RRDynamicSolver::setEmittance().
	//
	//! This function updates emittances in solver,
	//! but they are not used until you call illuminationReset().
	//!
	//! Scaler is used to convert emissive texture values to physical scale.
	//! Used only if quality && !usePointMaterials.
	void setEmittance(float emissiveMultiplier, unsigned quality, bool usePointMaterials, const RRScaler* scaler);
	//! Returns triangle emittance, physical, flat.
	//RRVec3 getTriangleEmittance(unsigned t);
	//! Sets triangle emittance, physical.
	//void setTriangleEmittance(unsigned t, const RRVec3& diffuseEmittance);

	void illuminationReset(const unsigned* customDirectIrradiance, const RRReal* customToPhysical);
	void illuminationImprove(unsigned qualityDynamic, unsigned qualityStatic);

	//! Returns triangle exitance, physical, flat. For dynamic objects/per-triangle materials.
	RRVec3 getTriangleExitance(unsigned triangle) const;

	//! Returns triangle exitance, physical, flat. For dynamic objects/point materials.
	RRVec3 getTriangleIrradiance(unsigned triangle) const;

	//! Updates values behind pointers returned by getTriangleIrradianceIndirect().
	void getTriangleIrradianceIndirectUpdate();
	//! Returns triangle indirect irradiance, physical, gouraud, includes skylight. For static objects.
	//
	//! Pointer is guaranteed to stay constant, you can reuse it in next frames.
	//! It may return NULL (for degenated and needle triangles).
	//! Pointers are valid even without calling update, however data behind pointers
	//! are valid only after calling update.
	const RRVec3* getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const;

	//! Returns any specified measure, slower but universal replacement for other getTriangleXxx() functions.
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
	RRMaterial defaultMaterial;

	// varying data
	class PackedBests* packedBests;
	RRVec3* ivertexIndirectIrradiance; // per-vertex results filled by getTriangleIrradianceIndirectUpdate()
	unsigned currentVersionInTriangles; // version of results available per triangle. reset, improve and setEnvironment may increment it
	unsigned currentVersionInVertices; // version of results available per vertex. getTriangleIrradianceIndirectUpdate() updates it to triangle version
	unsigned currentQuality; // number of best200 groups processed since reset
	RRReal terminalFluxToDistribute; // set during illuminationImprove(), when the best node has fluxToDistribute lower, improvement terminates
};

} // namespace

#endif
