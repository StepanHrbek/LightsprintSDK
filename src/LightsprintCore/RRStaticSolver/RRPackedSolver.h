#ifndef RRPACKEDSOLVER_H
#define RRPACKEDSOLVER_H

#include "Lightsprint/RRStaticSolver.h"

//#define THREADED_BEST

namespace rr
{

class PackedTriangle
{
public:
	RRVec3 diffuseReflectance;
	RRReal area;
	RRVec3 incidentFluxDirect;    // reset to direct illum
	RRVec3 incidentFluxToDiffuse; // reset to direct illum, modified by improve
	RRVec3 incidentFluxDiffused;  // reset to 0, modified by improve

	RRVec3 getExitance() const
	{
		return (incidentFluxDiffused+incidentFluxToDiffuse)*diffuseReflectance/area;
	}
	RRVec3 getIrradianceIndirect() const
	{
		return (incidentFluxDiffused+incidentFluxToDiffuse-incidentFluxDirect)/area;
	}
};

class RRPackedSolver
{
public:
	RRPackedSolver(const RRObject* object);
	void loadFile(class PackedSolverFile* adopt_packedSolverFile);
	void illuminationReset();
	void illuminationImprove(bool endfunc(void*), void* context);

	// physical, flat
	RRVec3 getTriangleExitance(unsigned triangle) const;

	// physical, gouraud. update must be called first
	// pointer is guaranteed to stay constant, you can reuse it in next frames
	void getTriangleIrradianceIndirectUpdate();
	const RRVec3* getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const;

	~RRPackedSolver();

protected:
	// constant precomputed data
	class PackedSolverFile* packedSolverFile; ///< Solver file loaded from disk.

	// constant realtime acquired data
	const RRObject* object;
	std::vector<PackedTriangle> triangles;

	// varying data
	class PackedBests* packedBests;
	RRVec3* ivertexIndirectIrradiance;
	bool triangleIrradianceIndirectDirty;
};

} // namespace

#endif
