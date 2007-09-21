//#define PARTIAL_SORT // best vybira pomoci partial_sort(), sponzu zpomali ze 103 na 83, z 65 na 49
//#define SHOW_CONVERGENCE
//#define SUPPORT_INCREMENTAL_RESET
//#define END_BY_QUALITY
#define BESTS 200 // sponza bests->speed 100->65 200->103 300->120 400->126 800->117   vetsi BESTS=horsi kvalita vysledku
#define MIN(a,b) (((a)<(b))?(a):(b))

#include "RRPackedSolver.h"
#include "PackedSolverFile.h"
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#if defined(USE_SSE) || defined(USE_SSEU)
	#include <xmmintrin.h>
#endif


namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// PackedTriangle

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
#ifdef USE_SSEU
	RRVec3p incidentFluxToDiffuse; // reset to direct illum, modified by improve
#else
	RRVec3 incidentFluxToDiffuse; // reset to direct illum, modified by improve
#endif
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





//////////////////////////////////////////////////////////////////////////////
//
// PackedBests
//
// struktura zodpovedna za vyber nejsilnejsich shooteru pro scatter


class PackedBests
{
public:

	void init(const PackedTriangle* _triangles, unsigned _indexBegin, unsigned _indexEnd, unsigned _indexStep)
	{
		triangle = _triangles;
		indexBegin = _indexBegin;
		indexEnd = _indexEnd;
		indexStep = _indexStep;
		reset();
	}

	void reset()
	{
		bests = 0;
		bestNodeIterator = 0;
	}

	// m-threaded interface, to be run from 1 thread when other threads don't call getSelectedBest()
	//  returns number of triangles in selected group
	unsigned selectBests()
	{
		//RRReportInterval report(INF2,"Finding bests200...\n");//!!!
		reset();


		// search 200 shooters with most energy to diffuse
		RRReal bestQ[BESTS];
		bestQ[BESTS-1] = -1; // last is always valid
		for(unsigned i=indexBegin;i<indexEnd;i++)
		{
			// calculate quality of distributor
			RRReal q = triangle[i].incidentFluxToDiffuse.sum();

			if(q>bestQ[BESTS-1])
			{
				// sort [q,node] into best cache, bestQ[0] is highest
				if(bests<BESTS) bests++;
				unsigned pos = bests-1;
				for(; pos>0 && q>bestQ[pos-1]; pos--)
				{
					// probublavam mezi spravne velkymi besty smerem k lepsim
					bestNode[pos] = bestNode[pos-1];
					bestQ[pos] = bestQ[pos-1];
				}
				bestNode[pos] = i;
				bestQ[pos] = q;
			}
		}
	//if(bests) RRReporter::report(INF3,"bestQ[0]=%f bestQ[%d]=%f\n",bestQ[0],bests-1,bestQ[bests-1]);//!!!


		return bests;
	}

	// m-threaded interface, to be run from many threads
	//  returns i-th triangle from group of selectBests() triangles
	unsigned getSelectedBest(unsigned i) const
	{
		RR_ASSERT(i<bests);
		return bestNode[i];
	}

	// 1-threaded interface
	//  returns best triangle
	unsigned getBest()
	{
		// if cache empty, fill cache
		if(bestNodeIterator==bests)
		{
			selectBests();
		}
		// get best from cache
		if(!bests) return UINT_MAX;
		return bestNode[bestNodeIterator++];
	}
protected:
	unsigned bests; ///< Number of elements in bestNode[] array.
	unsigned bestNode[BESTS]; ///< Runtime selected triangle indices in range <indexBegin..indexEnd-1>.
	unsigned bestNodeIterator; ///< Iterator used by 1threaded getBest(), index into bestNode[] array.
	const PackedTriangle* triangle; ///< Pointer to our data: triangle[indexBegin]..triangle[indexEnd-1].
	unsigned indexBegin; ///< Index into triangle array, first of our triangles.
	unsigned indexEnd; ///< Index into triangle array, last+1 of our triangles.
	unsigned indexStep; ///< Step used in triangle array, we access only triangles: indexBegin, indexBegin+indexStep, indexBegin+2*indexStep, ...
};




//////////////////////////////////////////////////////////////////////////////
//
// RRPackedSolver

RRPackedSolver::RRPackedSolver(const RRObject* _object, const PackedSolverFile* _adopt_packedSolverFile)
{
	object = _object;
	const RRMesh* mesh = object->getCollider()->getMesh();
	numTriangles = mesh->getNumTriangles();
	triangles = new PackedTriangle[numTriangles];
	for(unsigned t=0;t<numTriangles;t++)
	{
		const RRMaterial* material = object->getTriangleMaterial(t);
		triangles[t].diffuseReflectance = material ? material->diffuseReflectance : RRVec3(0.5f);
		triangles[t].areaInv = 1/mesh->getTriangleArea(t);
		// reset at least once, all future resets may be incremental
		triangles[t].incidentFluxDirect = RRVec3(0);
		triangles[t].incidentFluxToDiffuse = RRVec3(0);
		triangles[t].incidentFluxDiffused = RRVec3(0);
	}
	packedBests = new PackedBests; packedBests->init(triangles,0,numTriangles,1);
	packedSolverFile = _adopt_packedSolverFile;
	ivertexIndirectIrradiance = new RRVec3[packedSolverFile->packedIvertices->getNumC1()];
	triangleIrradianceIndirectDirty = true;
}

RRPackedSolver* RRPackedSolver::create(const RRObject* object, const PackedSolverFile* adopt_packedSolverFile)
{
	if(object && adopt_packedSolverFile && adopt_packedSolverFile->isCompatible(object))
		return new RRPackedSolver(object,adopt_packedSolverFile);
	return NULL;
}

void RRPackedSolver::illuminationReset()
{
	triangleIrradianceIndirectDirty = true;
	packedBests->reset();
#ifdef SUPPORT_INCREMENTAL_RESET
	if(!strong)
	{
		#pragma omp parallel for
		for(int t=0;(unsigned)t<numTriangles;t++)
		{
			RRVec3 incidentFluxDirect;
			object->getTriangleIllumination(t,RRRadiometricMeasure(0,0,1,1,1),incidentFluxDirect);
			triangles[t].incidentFluxToDiffuse += incidentFluxDirect-triangles[t].incidentFluxDirect;
			triangles[t].incidentFluxDirect = incidentFluxDirect;
		}
	}
	else
#endif
	{
		#pragma omp parallel for
		for(int t=0;(unsigned)t<numTriangles;t++)
		{
			object->getTriangleIllumination(t,RRRadiometricMeasure(0,0,1,1,1),triangles[t].incidentFluxDirect);
			triangles[t].incidentFluxToDiffuse = triangles[t].incidentFluxDirect;
			triangles[t].incidentFluxDiffused = RRVec3(0);
		}
	}
}

void RRPackedSolver::illuminationImprove(bool endfunc(void *), void *context)
{
	unsigned numShooters = 0;
	triangleIrradianceIndirectDirty = true;
	PackedFactorsThread* thread0 = packedSolverFile->packedFactors;
	do
	{
		{
			// 1-threaded propagation, s okamzitym zapojenim prijate energe do dalsiho strileni

#ifdef END_BY_QUALITY

			unsigned state = 0; // 0=first group, 1=middle groups, 2=last group
			RRReal metricTreshold;
			do 
			{
				// select group of bests
				unsigned bests = packedBests->selectBests();

				// get group metric
				RRReal metric = 0;
				for(unsigned i=MIN(bests,10);i--;)
				{
					metric += triangles[packedBests->getSelectedBest(i)].incidentFluxToDiffuse.sum();
				}

				// decide what to do, first group? last group?
				if(state==0)
				{
					metricTreshold = metric*0.2f; // remember first group
					state++;
					if(!bests || !metric) break; // no work to do, early exit (avoid another selectBests())
				}
				else
				if(metric<=metricTreshold)
				{
					bests /= 4; // process only 25% of last group
					state++;
				}

				for(unsigned i=0;i<bests;i++)
				{
					unsigned sourceTriangleIndex = packedBests->getSelectedBest(i);
					RR_ASSERT(sourceTriangleIndex!=UINT_MAX);
					PackedTriangle* source = &triangles[sourceTriangleIndex];
					// incidentFluxDiffused = co uz vystrilel
					// incidentFluxToDiffuse = co ma jeste vystrilet
					RRVec3 exitingFluxToDiffuse = source->incidentFluxToDiffuse * source->diffuseReflectance;
					source->incidentFluxDiffused += source->incidentFluxToDiffuse;
					source->incidentFluxToDiffuse = RRVec3(0);
					const PackedFactor* start = thread0->getC2(sourceTriangleIndex);
					const PackedFactor* stop  = thread0->getC2(sourceTriangleIndex+1);
					for(;start<stop;start++)
					{
						RR_ASSERT(start->getDestinationTriangle()<numTriangles);
						triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
							exitingFluxToDiffuse*start->getVisibility();
					}
				}
			} while(state<2);
			break;

#else // !END_BY_QUALITY

			unsigned sourceTriangleIndex = packedBests->getBest();
			if(sourceTriangleIndex==UINT_MAX) break;
			RR_ASSERT(sourceTriangleIndex!=UINT_MAX);
			PackedTriangle* source = &triangles[sourceTriangleIndex];
			// incidentFluxDiffused = co uz vystrilel
			// incidentFluxToDiffuse = co ma jeste vystrilet
#ifdef USE_SSE
			__m128 incidentFluxToDiffuse = _mm_load_ps((const float * const)&source->incidentFluxToDiffuse.x);
			__m128 exitingFluxToDiffuse = _mm_mul_ps(incidentFluxToDiffuse,_mm_load_ps((const float * const)&source->diffuseReflectance.x));
			float* incidentFluxDiffusedAdr = &source->incidentFluxDiffused.x;
			_mm_store_ps(incidentFluxDiffusedAdr,_mm_add_ps(_mm_load_ps(incidentFluxDiffusedAdr),incidentFluxToDiffuse));
#else
	#ifdef USE_SSEU
			__m128 exitingFluxToDiffuse = _mm_mul_ps(_mm_loadu_ps((const float * const)&source->incidentFluxToDiffuse.x),_mm_loadu_ps((const float * const)&source->diffuseReflectance.x));
	#else
			RRVec3 exitingFluxToDiffuse = source->incidentFluxToDiffuse * source->diffuseReflectance;
	#endif
			source->incidentFluxDiffused += source->incidentFluxToDiffuse;
#endif
			source->incidentFluxToDiffuse = RRVec3(0);
			const PackedFactor* start = thread0->getC2(sourceTriangleIndex);
			const PackedFactor* stop  = thread0->getC2(sourceTriangleIndex+1);
			for(;start<stop;start++)
			{
				RR_ASSERT(start->getDestinationTriangle()<numTriangles);
#ifdef USE_SSE
				float* incidentFluxToDiffuseAdr = &triangles[start->getDestinationTriangle()].incidentFluxToDiffuse.x;
				_mm_store_ps(incidentFluxToDiffuseAdr,_mm_add_ps(_mm_load_ps(incidentFluxToDiffuseAdr),_mm_mul_ps(exitingFluxToDiffuse,_mm_load1_ps(start->getVisibilityPtr()))));
#else
	#ifdef USE_SSEU
				float* incidentFluxToDiffuseAdr = &triangles[start->getDestinationTriangle()].incidentFluxToDiffuse.x;
				_mm_storeu_ps(incidentFluxToDiffuseAdr,_mm_add_ps(_mm_loadu_ps(incidentFluxToDiffuseAdr),_mm_mul_ps(exitingFluxToDiffuse,_mm_load1_ps(start->getVisibilityPtr()))));
	#else
				triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
					exitingFluxToDiffuse*start->getVisibility();
	#endif
#endif
			}
			numShooters++;

#endif // !END_BY_QUALITY

		}
	}
	while(!endfunc(context));

	//RRReporter::report(INF2,"numShooters=%d\n",numShooters);
}

RRVec3 RRPackedSolver::getTriangleExitance(unsigned triangle) const
{
	return triangles[triangle].getExitance();
}

void RRPackedSolver::getTriangleIrradianceIndirectUpdate()
{
	if(!triangleIrradianceIndirectDirty) return;
	triangleIrradianceIndirectDirty = false;
	PackedIvertices* packedIvertices = packedSolverFile->packedIvertices;
	int numIvertices = (int)packedIvertices->getNumC1();
#pragma omp parallel for schedule(static)
	for(int i=0;i<numIvertices;i++)
	{
		RRVec3 irrad = RRVec3(0);
		const PackedSmoothTriangleWeight* begin = packedIvertices->getC2(i);
		const PackedSmoothTriangleWeight* end = packedIvertices->getC2(i+1);
		for(;begin<end;begin++)
		{
			irrad += triangles[begin->triangleIndex].getIrradianceIndirect() * begin->weight;
		}
		ivertexIndirectIrradiance[i] = irrad;
	}	
}

const RRVec3* RRPackedSolver::getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const
{
	if(triangle>=0x3fffffff || vertex>=3 // wrong inputs, shouldn't happen (btw, it's not ffff, UNDEFINED is clamped to 30 + 2 bits)
		|| packedSolverFile->packedSmoothTriangles[triangle].ivertexIndex[vertex]>=packedSolverFile->packedIvertices->getNumC1()) // ffff in packed file -> triangle is degen or needle
	{
		return NULL;
	}
	return &ivertexIndirectIrradiance[packedSolverFile->packedSmoothTriangles[triangle].ivertexIndex[vertex]];
}

RRPackedSolver::~RRPackedSolver()
{
	delete[] triangles;
	delete[] ivertexIndirectIrradiance;
	delete packedBests;
	delete packedSolverFile;
}

} // namespace
