// --------------------------------------------------------------------------
// Final gathering.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "Lightsprint/RRDynamicSolver.h"
#include "LightmapFilter.h"

#define MAX_LIGHTMAP_DIRECTIONS 4


namespace rr
{

struct TexelContext
{
	RRDynamicSolver* solver;
	LightmapFilter* pixelBuffers[MAX_LIGHTMAP_DIRECTIONS];
	const RRDynamicSolver::UpdateParameters* params; // measure_internal.direct zapina gather z emitoru. measure_internal.indirect zapina gather indirectu ze static solveru. oboje zapina gather direct+indirect ze static solveru
	LightmapFilter* bentNormalsPerPixel;
	RRObject* singleObjectReceiver;
	bool gatherDirectEmitors; // true only in final (not first) gather when scene contains emitors. might result in full hemisphere gather
	bool gatherAllDirections; // MAX_LIGHTMAP_DIRECTIONS irradiances are gathered rather than 1
};

// subtexel is triangular intersection of triangle and texel
struct SubTexel
{
	unsigned multiObjPostImportTriIndex;
	RRVec2 uvInTriangleSpace[3]; // subtexel vertex coords in triangle space, range 0,0..1,0..0,1
	RRReal areaInMapSpace; // SubTexel area in map space in unknown units
};

// texel knows its intersection with all triangles
struct TexelSubTexels : public std::vector<SubTexel>
{
	TexelSubTexels()
	{
		areaInMapSpace = 0;
	}
	void push_back(const SubTexel& subTexel)
	{
		std::vector<SubTexel>::push_back(subTexel);
		RR_ASSERT(_finite(subTexel.areaInMapSpace));
		areaInMapSpace += subTexel.areaInMapSpace;
	}
	RRReal getAreaInMapSpace()
	{
		return areaInMapSpace;
	}
private:
	RRReal areaInMapSpace; // sum of subtexel areas in map space, may be greater than texel area when triangles (and consequently subtexels) overlap
};

struct ProcessTexelParams
{
	ProcessTexelParams(const TexelContext& _context) : context(_context) 
	{
		resetFiller = 0;
		rays = NULL;
	}
	const TexelContext& context;
	TexelSubTexels* subTexels;
	unsigned uv[2]; // texel coord in lightmap in 0..width-1,0..height-1
	unsigned resetFiller;
	RRRay* rays; // pointer to TWO rays. rayLengthMin should be initialized
};

struct ProcessTexelResult
{
	RRVec4 irradiance[MAX_LIGHTMAP_DIRECTIONS]; // alpha = 0|1
	RRVec4 bentNormal; // alpha = 0|1
	ProcessTexelResult()
	{
		for(unsigned i=0;i<MAX_LIGHTMAP_DIRECTIONS;i++) irradiance[i] = RRVec4(0);
		bentNormal = RRVec4(0);
	}
};

ProcessTexelResult processTexel(const ProcessTexelParams& pti);

} // namespace

