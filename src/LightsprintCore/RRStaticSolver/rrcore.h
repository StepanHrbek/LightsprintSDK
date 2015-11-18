// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Radiosity solver - low level.
// --------------------------------------------------------------------------

#ifndef RRVISION_RRCORE_H
#define RRVISION_RRCORE_H

//#define SUPPORT_INTERPOL // support interpolation, +20% memory required
#define BESTS           400 // how many best shooters to precalculate in one pass. more=faster best() but less accurate

#define CHANNELS         3
#define HITCHANNELS      1 // 1 (CHANNELS only if we support specular reflection that changes light color (e.g. polished steel) or specular transmittance that changes light color (e.g. colored glass))
#define FACTORCHANNELS   1

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
#include "../RRPackedSolver/PackedSolverFile.h"
#include <boost/random/linear_congruential.hpp>
//#include <boost/random/mersenne_twister.hpp>

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
	// Extended form factor.
	// Fraction of emited energy that reaches destination's diffuse component.
	// It is not modulated by destination diffuse color,
	// but specular interreflections and interrefractions are factored in
	// (specular fraction bounces through scene until it nearly dissipates in hits to diffuse surfaces).
	// This way, factor may be above 1, up to 1/diffuseReflectance.
	FactorChannels power;
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

	// light acumulators
	//  exitingXxx includes emittance
	//  incidentXxx does not include triangle's emittance
	RRVec3  totalExitingFluxToDiffuse;
	RRVec3  totalExitingFlux;
	RRVec3  totalIncidentFlux;
	RRVec3  directIncidentFlux;  // backup of direct incident flux in time 0. Set only by setSurface(). Read only by getSourceXxx().

	// get direct light (entered by client, not calculated)
	RRVec3  getDirectIncidentFlux()   const {return directIncidentFlux;}
	//RRVec3  getDirectEmitingFlux()    const {return surface->diffuseEmittance.color*area;} // emissivity
	RRVec3  getDirectExitingFlux(RRReal emissiveMultiplier) const {return directIncidentFlux*surface->diffuseReflectance.colorLinear + surface->diffuseEmittance.colorLinear*(area*emissiveMultiplier);} // emissivity + reflected light
	RRVec3  getDirectIrradiance()     const {return directIncidentFlux/area;}
	//RRVec3  getDirectEmittance()      const {return surface->diffuseEmittance.color;}
	RRVec3  getDirectExitance(RRReal emissiveMultiplier) const {return directIncidentFlux/area*surface->diffuseReflectance.colorLinear + surface->diffuseEmittance.colorLinear*emissiveMultiplier;}

	// get total light
	//RRVec3  getTotalIncidentFlux()    const {return totalIncidentFlux;}
	//RRVec3  getTotalEmitingFlux()     const {return surface->diffuseEmittance.color*area;}
	//RRVec3  getTotalExitingFlux()     const {return totalExitingFlux;}
	RRVec3  getTotalIrradiance()      const {return totalIncidentFlux/area;}
	//RRVec3  getTotalEmittance()       const {return surface->diffuseEmittance.color;}
	RRVec3  getTotalExitance()        const {return totalExitingFlux/area;}

	// get indirect light (computed from direct light)
	//RRVec3  getIndirectIncidentFlux() const {return totalIncidentFlux-directIncidentFlux;}
	//RRVec3  getIndirectEmitingFlux()  const {return RRVec3(0);}
	//RRVec3  getIndirectExitingFlux()  const {return totalExitingFlux-getDirectExitingFlux();}
	RRVec3  getIndirectIrradiance()   const {return (totalIncidentFlux-directIncidentFlux)/area;}
	//RRVec3  getIndirectEmittance()    const {return RRVec3(0);}
	RRVec3  getIndirectExitance(RRReal emissiveMultiplier) const {return totalExitingFlux/area-getDirectExitance(emissiveMultiplier);}

	// get any combination of direc/indirect/exiting light
	RRVec3  getMeasure(RRRadiometricMeasure measure, RRReal emissiveMultiplier) const;
	RRVec3  getPointMeasure(RRRadiometricMeasure measure, const RRVec2& uv) const; // supports only RM_IRRADIANCE_LINEAR, RM_IRRADIANCE_LINEAR_DIRECT, RM_IRRADIANCE_LINEAR_INDIRECT

	// precalc for best()
	real    precalcDistributing;
	real    precalcRefreshing;
	void    updatePrecalc(unsigned numTrianglesTotal);

	// geometry
	real    area;
	S8      setGeometry(const RRMesh::TriangleBody& body,float ignoreSmallerAngle,float ignoreSmallerArea);

	// material
	Channels setSurface(const RRMaterial *s,const RRVec3& sourceIrradiance, bool resetPropagation, RRReal emissiveMultiplier); // sets direct(source) lighting. emittance comes with material. irradiance comes from detectDirectIllumination [realtime] or from first gather [offline]
	const RRMaterial* surface;     // material at outer and inner side of Triangle

	// smoothing
	IVertex* topivertex[3]; // 3x ivertex

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
	unsigned size() {return triangles;}
	Triangle* get();

	private:
		unsigned trianglesAllocated;
		unsigned triangles;
		Triangle** triangle;
};

//////////////////////////////////////////////////////////////////////////////
//
// set of reflectors (light sources and things that reflect some light, no dark things)

struct BestInfo
{
	Triangle* node;
	unsigned shotsForNewFactors; // 0 = selected for distrib, not for shooting

	bool needsRefresh() {return shotsForNewFactors>0;}
};

class Reflectors
{
public:
	unsigned nodes;

	Reflectors();
	~Reflectors();
	void    reset(); // remove all reflectors
	void    resetBest(); // reset acceleration structures for best(), call after big update of primary energies
	bool    insert(Triangle* anode); // returns true when node was inserted (=appended)
	void    insertObject(class Object *o);

	BestInfo best(real allEnergyInScene);

	private:
		unsigned nodesAllocated;
		Triangle** node;
		// pack of best reflectors (internal cache for best())
		unsigned bests;
		BestInfo bestNode[BESTS];
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
	bool     buildTopIVertices(const RRSolver::SmoothingParameters* smoothing, bool& aborting); // false when out of memory or aborting
	unsigned getTriangleIndex(Triangle* t); // return index of triangle in object, UINT_MAX for invalid input

	// energies
	Channels objSourceExitingFlux; // primary source exiting radiant flux in Watts
	void    resetStaticIllumination(bool resetFactors, bool resetPropagation, RRReal emissiveMultiplier, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB);

private:
	Object();
	IVertex* topivertexArray; // array of all ivertices
};


//////////////////////////////////////////////////////////////////////////////
//
// Russian roulette

class RussianRoulette
{
public:
	void reset() {accumulator = 0;}
	bool survived(float survivalProbability);
private:
	float accumulator;
};


//////////////////////////////////////////////////////////////////////////////
//
// ShootingKernel

// used in radiosity form factor calculation, one kernel per thread
// when all rays are shot, all hitTriangles from all kernels are processed
// triangle is never inserted into multiple hitTriangles containers

class ShootingKernel
{
public:
	ShootingKernel();
	~ShootingKernel();

	RRRay*  sceneRay;
	class RRCollisionHandlerLod0* collisionHandlerLod0;
	boost::rand48 rand;
	//boost::mt11213b rand;
	RussianRoulette russianRoulette;
	Triangles hitTriangles;
	unsigned recursionDepth;
};

class ShootingKernels
{
public:
	ShootingKernels();
	void setGeometry(Triangle* sceneGeometry);
	void reset(unsigned maxQueries);
	~ShootingKernels();

	unsigned numKernels;
	ShootingKernel* shootingKernel;
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

	HitChannels rayTracePhoton(ShootingKernel* shootingKernel, const RRVec3& eye, const RRVec3& direction, const Triangle *skip, HitChannels power=HitChannels(1));

	void    objInsertStatic(Object *aobject);

	RRStaticSolver::Improvement resetStaticIllumination(bool resetFactors, bool resetPropagation, RRReal emissiveMultiplier, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB);
	RRStaticSolver::Improvement improveStatic(RRStaticSolver::EndFunc& endfunc);
	void    abortStaticImprovement();
	bool    shortenStaticImprovementIfBetterThan(real minimalImprovement);
	bool    finishStaticImprovement();
	bool    distribute(int minSteps, real maxError, real maxSeconds);//skonci kdyz nejvetsi mnozstvi nerozdistribuovane energie na jednom facu nepresahuje takovou cast energie lamp (0.001 is ok)

	real    avgAccuracy();

	//! \param raysFromTriangle
	//!  Average number of rays per triangle, increases calculation time and quality.
	//! \param importanceOfDetails
	//!  0=optimize for big picture, features are calculated at quality proportional to their size, micropolygons are noisy
	//!  1=optimize for details, all features are calculated at the same quality, even micropolygons
	//! \param aborting
	//!  May be set asynchronously, aborts update.
	PackedSolverFile* packSolver(unsigned raysFromTriangle, float importanceOfDetails, bool& aborting);

	private:
		int     phase;
		BestInfo improvingStatic;
		RRMesh::TriangleBody improvingBody;
		RRMesh::TangentBasis improvingBasisOrthonormal;
		void    shotFromToHalfspace(ShootingKernel* shootingKernel,Triangle* sourceNode);
		void    refreshFormFactorsFromUntil(BestInfo source,RRStaticSolver::EndFunc& endfunc);
		bool    energyFromDistributedUntil(BestInfo source,RRStaticSolver::EndFunc& endfunc);

		Channels staticSourceExitingFlux; // primary source exiting radiant flux in Watts, sum of absolute values
		unsigned shotsForNewFactors;
		unsigned shotsAccumulated;
		unsigned shotsForFactorsTotal;
		unsigned shotsTotal;
		Reflectors staticReflectors; // top nodes in static Triangle trees

		// array of kernels, one per core
		ShootingKernels shootingKernels;

		// all factors allocated by this scene
		// deallocated only in scene destructor or when factors are reset
		ChunkList<Factor>::Allocator factorAllocator;

		// used only during fireball build to gather sky hits
		PackedSkyTriangleFactor::UnpackedFactor* skyPatchHitsForAllTriangles;
		PackedSkyTriangleFactor::UnpackedFactor* skyPatchHitsForCurrentTriangle;
};

} // namespace

#endif
