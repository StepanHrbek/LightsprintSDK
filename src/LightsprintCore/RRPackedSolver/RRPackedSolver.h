#ifndef RRPACKEDSOLVER_H
#define RRPACKEDSOLVER_H

#include "Lightsprint/RRObject.h"

//#define THREADED_BEST // sponza: pocet shooteru zvedne ze 100 na 132

namespace rr
{

class RRPackedSolver
{
public:
	static RRPackedSolver* create(const RRObject* object, const class PackedSolverFile* adopt_packedSolverFile);

	void illuminationReset(unsigned* customDirectIrradiance, RRReal* customToPhysical);
	void illuminationImprove(bool endfunc(void*), void* context);

	// Triangle exitance, physical, flat. For dynamic objects.
	RRVec3 getTriangleExitance(unsigned triangle) const;

	// Triangle indirect irradiance, physical, gouraud. For static objects.
	// Pointer is guaranteed to stay constant, you can reuse it in next frames.
	// It may return NULL (for degenated and needle triangles).
	// Pointers are valid even without calling update, however data behind pointers
	// are valid only after calling update.
	void getTriangleIrradianceIndirectUpdate();
	const RRVec3* getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const;

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

	// mostly constant realtime acquired data
	RRReal customToPhysical[256];

	// varying data
	class PackedBests* packedBests;
	RRVec3* ivertexIndirectIrradiance;
	bool triangleIrradianceIndirectDirty;
};

} // namespace

#endif
