#include "Lightsprint/RRDynamicSolver.h"

#define POINT_LINE_DISTANCE_2D(point,line) ((line)[0]*(point)[0]+(line)[1]*(point)[1]+(line)[2])


namespace rr
{

struct ProcessTriangleInfo
{
	unsigned triangleIndex;
	RRVec3 normal;
	RRVec3 pos3d; //!!! to be removed (pouzito pro umisteni shooteru u svetel, odpadne az bude korektni antialias s nahodnym umistenim)
	RRMesh::TriangleBody triangleBody;
	RRVec3 line1InMap; // line equation in 0..1,0..1 map space
	RRVec3 line2InMap;
};

struct TexelContext
{
	RRDynamicSolver* solver;
	RRIlluminationPixelBuffer* pixelBuffer;
	const RRDynamicSolver::UpdateParameters* params;
	RRIlluminationPixelBuffer* bentNormalsPerPixel;
};

struct ProcessTexelParams
{
	ProcessTexelParams(const TexelContext& _context) : context(_context) 
	{
		resetFiller = 0;
		ray = NULL;
	}
	const TexelContext& context;
	unsigned uv[2]; // texel coord in lightmap in 0..width-1,0..height-1
	//std::vector<ProcessTriangleInfo> tri; // triangles intersecting texel
	ProcessTriangleInfo tri; // triangles intersecting texel
	unsigned resetFiller;
	RRRay* ray; // rayLengthMin should be initialized
};

struct ProcessTexelResult
{
	RRColorRGBAF irradiance; // alpha = 0|1
	RRColorRGBAF bentNormal; // alpha = 0|1
	ProcessTexelResult() : irradiance(0), bentNormal(0) {}
};

ProcessTexelResult processTexel(const ProcessTexelParams& pti);

} // namespace
