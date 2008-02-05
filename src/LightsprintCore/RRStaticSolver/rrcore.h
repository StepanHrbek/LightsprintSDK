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
#include "../RRObject/RRCollisionHandler.h" // SkipTriangle

#define STATISTIC(a)
#define STATISTIC_INC(a) STATISTIC(RRStaticSolver::getSceneStatistics()->a++)

namespace rr
{

#define SMALL_ENERGY 0.000001f // energy amount small enough to have no impact on scene

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

	Factor(Triangle* destination,FactorChannels apower);
};

//////////////////////////////////////////////////////////////////////////////
//
// all form factors from implicit source

extern unsigned __factorsAllocated;

class Factors
{
public:
	Factors();
	~Factors();
	void    reset();

	unsigned factors();
	void    insert(Factor afactor);
	void    insert(Factors *afactors);
	const Factor* get(unsigned i) {return &factor[i];} // returns i-th factor
	Factor  get(); // returns one factor and and removes it from container
	void    forEach(void (*func)(Factor *factor,va_list ap),...);

	private:
		unsigned factors24:24;
		unsigned allocatedLn2:8;//ln2(factors allocated), 0=nothing allocated
		unsigned factorsAllocated();
		Factor *factor;
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
	Factors factors;

	// shooting
	real    hits; // accumulates hits from current shooter
	unsigned shotsForFactors:31; // number of shots used for current ff
	unsigned isReflector:1;
	real    accuracy(); // shots done per energy unit

	// light acumulators
	RRVec3  totalExitingFluxToDiffuse;
	RRVec3  totalExitingFlux;
	RRVec3  totalIncidentFlux;
	RRVec3  directIncidentFlux;  // backup of direct incident flux in time 0. Set only by setSurface(). Read only by getSourceXxx().
	// get direct light (entered by client, not calculated)
	RRVec3  getDirectIncidentFlux()   const {return directIncidentFlux;}
	RRVec3  getDirectEmitingFlux()    const {return surface->diffuseEmittance*area;} // emissivity
	RRVec3  getDirectExitingFlux()    const {return directIncidentFlux*surface->diffuseReflectance + surface->diffuseEmittance*area;} // emissivity + reflected light
	RRVec3  getDirectIrradiance()     const {return directIncidentFlux/area;}
	RRVec3  getDirectEmittance()      const {return surface->diffuseEmittance;}
	RRVec3  getDirectExitance()       const {return directIncidentFlux/area*surface->diffuseReflectance + surface->diffuseEmittance;}
	// get total light
	RRVec3  getTotalIncidentFlux()    const {return totalIncidentFlux;}
	RRVec3  getTotalEmitingFlux()     const {return surface->diffuseEmittance*area;}
	RRVec3  getTotalExitingFlux()     const {return totalExitingFlux;}
	RRVec3  getTotalIrradiance()      const {return totalIncidentFlux/area;}
	RRVec3  getTotalEmittance()       const {return surface->diffuseEmittance;}
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
	S8      setGeometry(RRVec3* a,RRVec3* b,RRVec3* c,const RRMatrix3x4 *obj2world,Normal *n,float ignoreSmallerAngle,float ignoreSmallerArea);
	const RRVec3* getVertex(unsigned i) const {return qvertex[i];}
	RRVec3  getS3() const {return *getVertex(0);} // absolute position of start of base (transformed when dynamic)
	RRVec3  getR3() const {return *getVertex(1)-*getVertex(0);} // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0] (all transformed when dynamic)
	RRVec3  getL3() const {return *getVertex(2)-*getVertex(0);}
	Normal  getN3() const {return qn3;}
		private:
		const RRVec3* qvertex[3]; // 3x vertex
		Normal qn3; // normalized normal vector
		public:

	// material
	Channels setSurface(const RRMaterial *s,const RRVec3& sourceIrradiance, bool resetPropagation); // sets direct(source) lighting. emittance comes with material. irradiance comes from detectDirectIllumination [realtime] or from first gather [offline]
	const RRMaterial *surface;     // material at outer and inner side of Triangle

	// smoothing
	IVertex *topivertex[3]; // 3x ivertex
	IVertex *ivertex(int i);

	friend IVertex;
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

	Triangle* best(real allEnergyInScene,real subdivisionSpeed);
	bool    lastBestWantsRefresh() {return refreshing;}
	bool    insert(Triangle* anode); // returns true when node was inserted (=appended)
	void    insertObject(class Object *o);
	void    removeSubtriangles();

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
	Object(int avertices,int atriangles);
	~Object();

	// object data
	RRObject* importer;
	unsigned vertices;
	unsigned triangles;
	RRVec3    *vertex;
	Triangle*triangle;
	void    buildTopIVertices(unsigned smoothMode, float minFeatureSize, float maxSmoothAngle);
		private:
		unsigned mergeCloseIVertices(IVertex* ivertex, float minFeatureSize);
		public:
	unsigned getTriangleIndex(Triangle* t); // return index of triangle in object, UINT_MAX for invalid input
	// IVertex pool
	IVertex *newIVertex();
	void     deleteIVertices();
	IVertex *IVertexPool;
	unsigned IVertexPoolItems;
	unsigned IVertexPoolItemsUsed;

	float   subdivisionSpeed;

	// energies
	Channels objSourceExitingFlux; // primary source exiting radiant flux in Watts
	void    resetStaticIllumination(bool resetFactors, bool resetPropagation);
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

	RRStaticSolver::Improvement resetStaticIllumination(bool resetFactors, bool resetPropagation);
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
		Factors improvingFactors;
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

		// previously global filler, now allocated per scene
		// -> multiple independent scenes are legal
		HomogenousFiller filler;
		bool getRandomExitDir(const RRVec3& norm, const RRVec3& u3, const RRVec3& v3, const RRSideBits* sideBits, RRVec3& exitDir);
	public:
		Triangle* getRandomExitRay(Triangle* sourceNode, RRVec3* src, RRVec3* dir);
	private:

		// previously global skipTriangle, now allocated per scene
		//  -> multiple independent scenes are legal
		// SkipTriangle is not thread safe
		//  -> one SkipTriangle per thread must be used if improveStatic() gets parallelized
		SkipTriangle skipTriangle;
};

} // namespace

#endif
