// --------------------------------------------------------------------------
// Radiosity solver - low level.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef RRVISION_RRCORE_H
#define RRVISION_RRCORE_H

#define SUPPORT_MIN_FEATURE_SIZE // support merging of near ivertices (to fight needles, hide features smaller than limit)
//#define SUPPORT_INTERPOL // support interpolation, +20% memory required
#define BESTS           200 // how many best shooters to precalculate in one pass. more=faster best() but less accurate

#define CHANNELS         3
#define HITCHANNELS      1 // 1 (CHANNELS only if we support specular reflection that changes light color (e.g. polished steel) or specular transmittance that changes light color (e.g. colored glass))
#define FACTORCHANNELS   1 // if CLEAN_FACTORS then 1 else CHANNELS

#if CHANNELS==1
#define Channels         RRReal
#elif CHANNELS==3
#define Channels         RRVec3
#else		    
#error unsupported CHANNELS
#endif

#if HITCHANNELS==1
#define HitChannels      RRReal
#elif HITCHANNELS==3
#define HitChannels      RRVec3
#else
#error unsupported HITCHANNELS
#endif

#if FACTORCHANNELS==1
#define FactorChannels   RRReal
#elif FACTORCHANNELS==3
#define FactorChannels   RRVec3
#else
#error unsupported FACTORCHANNELS
#endif

#include <stdarg.h>

#include "geometry_v.h"
#include "../RRStaticSolver/RRStaticSolver.h"
#include "interpol.h"
#include "ChunkList.h"

#define STATISTIC(a)
#define STATISTIC_INC(a) STATISTIC(RRStaticSolver::getSceneStatistics()->a++)

namespace rr
{

#define SMALL_ENERGY 0.000001f // energy amount small enough to have no impact on scene

#define ARRAY_ELEMENT_TO_INDEX(array,element) ((unsigned)((element)-(array))) // ARRAY_ELEMENT_TO_INDEX(a,&a[i]) = i

// konvence:
//  reset() uvadi objekt do stavu po konstruktoru
//  resetStaticIllumination() uvadi objekt do stavu po nacteni sceny



//////////////////////////////////////////////////////////////////////////////
//
// memory

void* realloc(void* p,size_t oldsize,size_t newsize);

//////////////////////////////////////////////////////////////////////////////
//
// globals

extern unsigned __frameNumber; // frame number increased after each draw

//////////////////////////////////////////////////////////////////////////////
//
// form factor from implicit source to explicit destination

class Factor
{
public:
	class Triangle* destination;
	FactorChannels power; // this fraction of emited energy reaches destination
	               // Q: is modulated by destination's material?
	               // A: CLEAN_FACTORS->NO, !CLEAN_FACTORS->YES
};

//////////////////////////////////////////////////////////////////////////////
//
// triangle
//

class Triangle
{
public:
	Triangle();

	void    reset(bool resetFactors);

	// form factors
	ChunkList<Factor> factors;

	// shooting
	real    hits; // accumulates hits from current shooter
	unsigned shotsForFactors:30; // number of shots used for current ff
	unsigned isLod0:1; // triangle is in lod0. constant for whole triangle lifetime
	unsigned isReflector:1; // triangle (is in lod0 and) has some energy accumulated to reflect
	real    accuracy(); // shots done per energy unit

	// light acumulators
	RRVec3  totalExitingFluxToDiffuse;
	RRVec3  totalExitingFlux;
	RRVec3  totalIncidentFlux;
	RRVec3  directIncidentFlux;  // backup of direct incident flux in time 0. Set only by setSurface(). Read only by getSourceXxx().
	// get direct light (entered by client, not calculated)
	RRVec3  getDirectIncidentFlux()   const {return directIncidentFlux;}
	RRVec3  getDirectEmitingFlux()    const {return surface->diffuseEmittance.color*area;} // emissivity
	RRVec3  getDirectExitingFlux()    const {return directIncidentFlux*surface->diffuseReflectance.color + surface->diffuseEmittance.color*area;} // emissivity + reflected light
	RRVec3  getDirectIrradiance()     const {return directIncidentFlux/area;}
	RRVec3  getDirectEmittance()      const {return surface->diffuseEmittance.color;}
	RRVec3  getDirectExitance()       const {return directIncidentFlux/area*surface->diffuseReflectance.color + surface->diffuseEmittance.color;}
	// get total light
	RRVec3  getTotalIncidentFlux()    const {return totalIncidentFlux;}
	RRVec3  getTotalEmitingFlux()     const {return surface->diffuseEmittance.color*area;}
	RRVec3  getTotalExitingFlux()     const {return totalExitingFlux;}
	RRVec3  getTotalIrradiance()      const {return totalIncidentFlux/area;}
	RRVec3  getTotalEmittance()       const {return surface->diffuseEmittance.color;}
	RRVec3  getTotalExitance()        const {return totalExitingFlux/area;}
	// get indirect light (computed from direct light)
	RRVec3  getIndirectIncidentFlux() const {return totalIncidentFlux-directIncidentFlux;}
	RRVec3  getIndirectEmitingFlux()  const {return RRVec3(0);}
	RRVec3  getIndirectExitingFlux()  const {return totalExitingFlux-getDirectExitingFlux();}
	RRVec3  getIndirectIrradiance()   const {return (totalIncidentFlux-directIncidentFlux)/area;}
	RRVec3  getIndirectEmittance()    const {return RRVec3(0);}
	RRVec3  getIndirectExitance()     const {return totalExitingFlux/area-getDirectExitance();}
	// get any combination of direc/indirect/exiting light
	RRVec3  getMeasure(RRRadiometricMeasure measure) const;

	// geometry
	real    area;
	S8      setGeometry(const RRMesh::TriangleBody& body,float ignoreSmallerAngle,float ignoreSmallerArea);

	// material
	Channels setSurface(const RRMaterial *s,const RRVec3& sourceIrradiance, bool resetPropagation); // sets direct(source) lighting. emittance comes with material. irradiance comes from detectDirectIllumination [realtime] or from first gather [offline]
	const RRMaterial *surface;     // material at outer and inner side of Triangle

	// smoothing
	IVertex *topivertex[3]; // 3x ivertex
	IVertex *ivertex(int i);

	friend class IVertex;
	friend class Scene;
	friend class PackedBests;
};

//////////////////////////////////////////////////////////////////////////////
//
// set of triangles

class Triangles
{
public:
	Triangles();
	~Triangles();
	void    reset();

	void    insert(Triangle *key);
	Triangle *get();

	private:
		unsigned trianglesAllocated;
		unsigned triangles;
		Triangle **triangle;
};

//////////////////////////////////////////////////////////////////////////////
//
// set of reflectors (light sources and things that reflect some light, no dark things)

class Reflectors
{
public:
	unsigned nodes;

	Reflectors();
	~Reflectors();
	void    reset(); // remove all reflectors
	void    resetBest(); // reset acceleration structures for best(), call after big update of primary energies

	Triangle* best(real allEnergyInScene);
	bool    lastBestWantsRefresh() {return refreshing;}
	bool    insert(Triangle* anode); // returns true when node was inserted (=appended)
	void    insertObject(class Object *o);

	private:
		unsigned nodesAllocated;
	protected:
		Triangle** node;
		// pack of best reflectors (internal cache for best())
		unsigned bests;
		Triangle* bestNode[BESTS];
		bool refreshing; // false = all nodes were selected for distrib, true = for refresh
};

//////////////////////////////////////////////////////////////////////////////
//
// object, part of scene

class Object
{
public:
	static Object* create(int avertices,int atriangles);
	~Object();

	// object data
	RRObject* importer;
	unsigned vertices;
	unsigned triangles;
	Triangle*triangle;
	bool     buildTopIVertices(float minFeatureSize, float maxSmoothAngle, bool& aborting); // false when out of memory or aborting
		private:
		unsigned mergeCloseIVertices(IVertex* ivertex, float minFeatureSize, bool& aborting);
		public:
	unsigned getTriangleIndex(Triangle* t); // return index of triangle in object, UINT_MAX for invalid input
	// IVertex pool
	IVertex *newIVertex();
	void     deleteIVertices();
	IVertex *IVertexPool;
	unsigned IVertexPoolItems;
	unsigned IVertexPoolItemsUsed;

	// energies
	Channels objSourceExitingFlux; // primary source exiting radiant flux in Watts
	void    resetStaticIllumination(bool resetFactors, bool resetPropagation, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB);

private:
	Object();
};


//////////////////////////////////////////////////////////////////////////////
//
// homogenous filler

class HomogenousFiller
{
public:
	void Reset();
	real GetCirclePoint(real *a,real *b);
private:
	void GetTrianglePoint(real *a,real *b);
	unsigned num;
};


//////////////////////////////////////////////////////////////////////////////
//
// scene

class Scene
{
public:
	Scene();
	~Scene();

	Object* object;        // the only object that contains whole static scene

	HitChannels rayTracePhoton(Point3 eye,RRVec3 direction,Triangle *skip,HitChannels power=HitChannels(1));

	void    objInsertStatic(Object *aobject);

	RRStaticSolver::Improvement resetStaticIllumination(bool resetFactors, bool resetPropagation, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB);
	RRStaticSolver::Improvement improveStatic(bool endfunc(void*), void* context);
	void    abortStaticImprovement();
	bool    shortenStaticImprovementIfBetterThan(real minimalImprovement);
	bool    finishStaticImprovement();
	bool    distribute(real maxError);//skonci kdyz nejvetsi mnozstvi nerozdistribuovane energie na jednom facu nepresahuje takovou cast energie lamp (0.001 is ok)

	real    avgAccuracy();

	// night edition
	void    updateFactors(unsigned raysFromTriangle);
	class PackedSolverFile* packSolver() const;

	private:
		int     phase;
		Triangle* improvingStatic;
		Triangles hitTriangles;
		RRMesh::TriangleBody improvingBody;
		RRMesh::TangentBasis improvingBasisOrthonormal;
		void    shotFromToHalfspace(Triangle* sourceNode);
		void    refreshFormFactorsFromUntil(Triangle* source,unsigned forcedShotsForNewFactors,bool endfunc(void *),void *context);
		bool    energyFromDistributedUntil(Triangle* source,bool endfunc(void *),void *context);

		Channels staticSourceExitingFlux; // primary source exiting radiant flux in Watts, sum of absolute values
		unsigned shotsForNewFactors;
		unsigned shotsAccumulated;
		unsigned shotsForFactorsTotal;
		unsigned shotsTotal;
		Reflectors staticReflectors; // top nodes in static Triangle trees

		// previously global ray+levels, now allocated per scene
		// -> multiple independent scenes are legal
		RRRay*  sceneRay;
		class RRCollisionHandlerLod0* collisionHandlerLod0;

		// all factors allocated by this scene
		// deallocated only in scene destructor
		ChunkList<Factor>::Allocator factorAllocator;

		// previously global filler, now allocated per scene
		// -> multiple independent scenes are legal
		HomogenousFiller filler;
		bool getRandomExitDir(const RRMesh::TangentBasis& basis, const RRSideBits* sideBits, RRVec3& exitDir);
	public:
		Triangle* getRandomExitRay(Triangle* sourceNode, RRVec3* src, RRVec3* dir);
};

} // namespace

#endif
