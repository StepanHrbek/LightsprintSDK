#ifndef RRPACKEDSOLVER_H
#define RRPACKEDSOLVER_H

#include "Lightsprint/RRStaticSolver.h"
#include <xmmintrin.h>

//#define THREADED_BEST
//#define USE_SSE // kod vyrazne zkrati ale nezrychli

namespace rr
{

#ifdef USE_SSE
class PackedTriangle : public RRAligned
{
public:
	RRVec3 diffuseReflectance;
	RRReal areaInv;
	RRVec3p incidentFluxToDiffuse; // reset to direct illum, modified by improve
	RRVec3p incidentFluxDiffused;  // reset to 0, modified by improve
	RRVec3p incidentFluxDirect;    // reset to direct illum
#else
class PackedTriangle
{
public:
	RRVec3 diffuseReflectance;
	RRReal areaInv;
	RRVec3 incidentFluxToDiffuse; // reset to direct illum, modified by improve
	RRVec3 incidentFluxDiffused;  // reset to 0, modified by improve
	RRVec3 incidentFluxDirect;    // reset to direct illum
#endif

	// for dynamic objects
	RRVec3 getExitance() const
	{
		return (incidentFluxDiffused+incidentFluxToDiffuse)*diffuseReflectance*areaInv;
	}
	// for static objects
	RRVec3 getIrradianceIndirect() const
	{
		return (incidentFluxDiffused+incidentFluxToDiffuse-incidentFluxDirect)*areaInv;
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
	// pointer is always valid, pointer to pink is returned when irradiance is not available for any reason
	void getTriangleIrradianceIndirectUpdate();
	const RRVec3* getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const;

	~RRPackedSolver();

protected:
	// constant precomputed data
	class PackedSolverFile* packedSolverFile; ///< Solver file loaded from disk.

	// constant realtime acquired data
	const RRObject* object;
	PackedTriangle* triangles;
	unsigned numTriangles;

	// varying data
	class PackedBests* packedBests;
	RRVec3* ivertexIndirectIrradiance;
	bool triangleIrradianceIndirectDirty;
};

} // namespace

#endif
