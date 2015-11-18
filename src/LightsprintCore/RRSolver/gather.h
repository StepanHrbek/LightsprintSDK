// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// First and final gathering.
// --------------------------------------------------------------------------



//#define USE_BOOST_POOL // use boost. big, fast
// comment out both = use stl. small, slow

#include "Lightsprint/RRSolver.h"
#include "../RRStaticSolver/pathtracer.h" // PathtracerJob
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

//////////////////////////////////////////////////////////////////////////////
//
// LightmapperJob
//
// constant data shared by all workers/threads

struct LightmapperJob : public PathtracerJob
{
	RRBuffer* pixelBuffers[NUM_BUFFERS]; // classical lmap, 3 directional lmaps, bent normal map
	const RRSolver::UpdateParameters* params;
	RRObject* singleObjectReceiver;
	bool gatherAllDirections; // LS_DIRECTIONn irradiances are gathered too
	bool staticSceneContainsLods; // scene contains LODs, additional work

	LightmapperJob(RRSolver* _solver)
		: PathtracerJob(_solver,false)
	{
		for (unsigned i=0;i<NUM_BUFFERS;i++)
			pixelBuffers[i] = NULL;
		params = NULL;
		singleObjectReceiver = NULL;
		gatherAllDirections = true;
		staticSceneContainsLods = false;
	};
};

// subtexel is triangular intersection of triangle and texel
struct SubTexel
{
	unsigned multiObjPostImportTriIndex;
	RRVec2 uvInTriangleSpace[3]; // subtexel vertex coords in triangle space, range 0,0..1,0..0,1
	RRReal areaInMapSpace; // SubTexel area in map space in unknown units
};

// extra information necessary for smoothing and grow when gutter is too small
enum TexelFlags
{
	QUADRANT_X0Y0=1,
	QUADRANT_X1Y0=2,
	QUADRANT_X0Y1=4,
	QUADRANT_X1Y1=8,
	EDGE_X0=16,
	EDGE_X1=32,
	EDGE_Y0=64,
	EDGE_Y1=128,
};
#define FLOAT_TO_TEXELFLAGS(flo) ((unsigned)((flo)*255+0.5f))
#define TEXELFLAGS_TO_FLOAT(fla) ((fla)/255.f)

// texel knows its intersection with all triangles
class TexelSubTexels : public
	ChunkList<SubTexel> // chunklist is small, fast
{
public:
	unsigned char texelFlags; // TexelFlags
	TexelSubTexels()
	{
		texelFlags = 0;
	}
};

struct ProcessTexelParams
{
	ProcessTexelParams(const LightmapperJob& _context) : context(_context) 
	{
		// [#15] rand() randomizes HOMOGENOUS_FILL, so that every texel shoots into different directions (but still homogenously)
		// &0xffff reduces randomness on systems with huge RAND_MAX, it saves calculation time (filler speed depends on number of bits in this number)
		// warning: for very high quality, we could end up with neighbouring texels shooting nearly the same rays, e.g. 0..100000 and 1000..101000
		//          ideally we should set higher random numbers when higher quality is set
		resetFiller = rand()&0xffff;
		rayLengthMin = 0;
		relevantLights = NULL;
	}
	const LightmapperJob& context;
	TexelSubTexels* subTexels;
	unsigned uv[2]; // texel coord in lightmap in 0..width-1,0..height-1
	unsigned resetFiller;
	float rayLengthMin; // will be copied to rays
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

