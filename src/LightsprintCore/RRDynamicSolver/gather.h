// --------------------------------------------------------------------------
// Final gathering.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


//error : inserted by sunifdef: "#define USE_BOOST_POOL // simply undef it if you don't have boost headers. lightmap building will be slightly slower, using slightly less memory" contradicts -U at R:\work2\.git-rewrite\t\src\LightsprintCore\RRDynamicSolver\gather.h~(8)

#include "Lightsprint/RRDynamicSolver.h"
#include "LightmapFilter.h"


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
typedef std::vector<SubTexel> TexelSubTexels;

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
	RRLight** relevantLights; // pointer to sufficiently big array of RRLight*
	unsigned numRelevantLights; // number of valid relevant lights in array
	bool relevantLightsFilled; // true when relevantLights are already filled, false when array is uninitialized
};

struct ProcessTexelResult
{
	RRVec4 irradiance[NUM_LIGHTMAPS]; // alpha = 0|1
	RRVec4 bentNormal; // alpha = 0|1
	ProcessTexelResult()
	{
		for(unsigned i=0;i<NUM_LIGHTMAPS;i++) irradiance[i] = RRVec4(0);
		bentNormal = RRVec4(0);
	}
};

ProcessTexelResult processTexel(const ProcessTexelParams& pti);

//! Data gathered by gatherPerTriangle()
class GatheredPerTriangleData
{
public:
	RRVec3* data[NUM_BUFFERS]; 

	static GatheredPerTriangleData* create(unsigned numTriangles, bool gatherLightmap, bool gatherDirections, bool gatherBentNormals)
	{
		GatheredPerTriangleData* a = NULL;
		try
		{
			a = new GatheredPerTriangleData;
			a->data[LS_LIGHTMAP] = gatherLightmap ? new RRVec3[numTriangles] : NULL;
			a->data[LS_DIRECTION1] = gatherDirections ? new RRVec3[numTriangles] : NULL;
			a->data[LS_DIRECTION2] = gatherDirections ? new RRVec3[numTriangles] : NULL;
			a->data[LS_DIRECTION3] = gatherDirections ? new RRVec3[numTriangles] : NULL;
			a->data[LS_BENT_NORMALS] = gatherBentNormals ? new RRVec3[numTriangles] : NULL;
		}
		catch(std::bad_alloc)
		{
			SAFE_DELETE(a);
		}
		return a;
	}
	~GatheredPerTriangleData()
	{
		for(unsigned i=0;i<NUM_BUFFERS;i++)
			delete[] data[i];
	}
	void store(unsigned triangleNumber, const ProcessTexelResult& a) const
	{
		for(unsigned i=0;i<NUM_BUFFERS;i++)
			if(data[i])
				data[i][triangleNumber] = a.irradiance[i];
	}
protected:
	GatheredPerTriangleData()
	{
		for(unsigned i=0;i<NUM_BUFFERS;i++)
			data[i] = NULL;
	}
};

} // namespace

