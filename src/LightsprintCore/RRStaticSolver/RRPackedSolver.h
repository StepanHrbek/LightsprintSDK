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
	void loadFactors(class PackedFactorsProcess* packedFactorsProcess);
	void illuminationReset();
	void illuminationImprove(bool endfunc(void*), void* context);
	void updateIndirect(); // must be called before sequence of getTriangleIncidentIndirectIrradiance()s
	// physical, flat
	RRVec3 getTriangleExitance(unsigned triangle) const;
	// physical, gouraud
	RRVec3 getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex, RRStaticSolver* staticSolver) const;
	~RRPackedSolver();

protected:
	const RRObject* object;
	std::vector<PackedTriangle> triangles;
	//class PackedFactors* packedFactors;
	class PackedFactorsProcess* packedFactorsProcess;
	class PackedBests* packedBests;
};

} // namespace

#endif
