#ifndef RRPACKEDSOLVER_H
#define RRPACKEDSOLVER_H

#include "Lightsprint/RRStaticSolver.h"

//error : inserted by sunifdef: "#define THREADED_BEST" contradicts -U at R:\work2\.git-rewrite\t\src\LightsprintCore\RRStaticSolver\RRPackedSolver.h~(6)

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
	void getTriangleIrradianceIndirectUpdate();
	RRVec3 getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex, RRStaticSolver* staticSolver) const;

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
