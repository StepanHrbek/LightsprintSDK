// --------------------------------------------------------------------------
// Final gathering.
// Copyright (c) 2006-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------



//#define USE_BOOST_POOL // use boost. big, fast
// comment out both = use stl. small, slow

#include "Lightsprint/RRDynamicSolver.h"
#include "LightmapFilter.h"
	#include "../RRStaticSolver/ChunkList.h"

namespace rr
{

// defines order of lightmaps in array
enum LightmapSemantic
{
	LS_LIGHTMAP = 0,
	LS_DIRECTION1,
	LS_DIRECTION2,
	LS_DIRECTION3,
	LS_BENT_NORMALS,
	NUM_LIGHTMAPS = LS_DIRECTION3+1,
	NUM_BUFFERS = LS_BENT_NORMALS+1,
};

struct TexelContext
{
	RRDynamicSolver* solver;
	LightmapFilter* pixelBuffers[NUM_BUFFERS]; // classical lmap, 3 directional lmaps, bent normal map
	const RRDynamicSolver::UpdateParameters* params; // measure_internal.direct zapina gather z emitoru. measure_internal.indirect zapina gather indirectu ze static solveru. oboje zapina gather direct+indirect ze static solveru
	RRObject* singleObjectReceiver;
	bool gatherDirectEmitors; // true only in final (not first) gather when scene contains emitors. might result in full hemisphere gather
	bool gatherAllDirections; // LS_DIRECTIONn irradiances are gathered too
	bool staticSceneContainsLods; // scene contains LODs, additional work
};

// subtexel is triangular intersection of triangle and texel
struct SubTexel
{
	unsigned multiObjPostImportTriIndex;
	RRVec2 uvInTriangleSpace[3]; // subtexel vertex coords in triangle space, range 0,0..1,0..0,1
	RRReal areaInMapSpace; // SubTexel area in map space in unknown units
};

// texel knows its intersection with all triangles
typedef ChunkList<SubTexel> TexelSubTexels; // chunklist is small, fast

struct ProcessTexelParams
{
	ProcessTexelParams(const TexelContext& _context) : context(_context) 
	{
		resetFiller = 0;
		rays = NULL;
		relevantLights = NULL;
	}
	const TexelContext& context;
	TexelSubTexels* subTexels;
	unsigned uv[2]; // texel coord in lightmap in 0..width-1,0..height-1
	unsigned resetFiller;
	RRRay* rays; // pointer to TWO rays. rayLengthMin should be initialized
	const RRLight** relevantLights; // pointer to sufficiently big array of RRLight*
	unsigned numRelevantLights; // number of valid relevant lights in array
	bool relevantLightsFilled; // true when relevantLights are already filled, false when array is uninitialized
};

struct ProcessTexelResult
{
	RRVec4 irradiancePhysical[NUM_LIGHTMAPS]; // alpha = 0|1
	RRVec4 bentNormal; // alpha = 0|1
	ProcessTexelResult()
	{
		for (unsigned i=0;i<NUM_LIGHTMAPS;i++) irradiancePhysical[i] = RRVec4(0);
		bentNormal = RRVec4(0);
	}
};

ProcessTexelResult processTexel(const ProcessTexelParams& pti);

//! Data gathered by gatherPerTrianglePhysical()
class GatheredPerTriangleData
{
public:
	RRVec3* data[NUM_BUFFERS]; // physical scale

	static GatheredPerTriangleData* create(unsigned numTriangles, bool gatherLightmap, bool gatherDirections, bool gatherBentNormals)
	{
		GatheredPerTriangleData* a = new GatheredPerTriangleData;
		a->data[LS_LIGHTMAP] = gatherLightmap ? new (std::nothrow) RRVec3[numTriangles] : NULL;
		a->data[LS_DIRECTION1] = gatherDirections ? new (std::nothrow) RRVec3[numTriangles] : NULL;
		a->data[LS_DIRECTION2] = gatherDirections ? new (std::nothrow) RRVec3[numTriangles] : NULL;
		a->data[LS_DIRECTION3] = gatherDirections ? new (std::nothrow) RRVec3[numTriangles] : NULL;
		a->data[LS_BENT_NORMALS] = gatherBentNormals ? new (std::nothrow) RRVec3[numTriangles] : NULL;
		if ((gatherLightmap && !a->data[LS_LIGHTMAP])
			|| (gatherDirections && (!a->data[LS_DIRECTION1] || !a->data[LS_DIRECTION2] || !a->data[LS_DIRECTION3]))
			|| (gatherBentNormals && !a->data[LS_BENT_NORMALS]))
		{
			RR_SAFE_DELETE(a);
		}
		return a;
	}
	~GatheredPerTriangleData()
	{
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			delete[] data[i];
	}
	void store(unsigned triangleNumber, const ProcessTexelResult& a) const
	{
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			if (data[i])
				data[i][triangleNumber] = a.irradiancePhysical[i];
	}
protected:
	GatheredPerTriangleData()
	{
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			data[i] = NULL;
	}
};

} // namespace

