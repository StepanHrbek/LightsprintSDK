//#define PARTIAL_SORT // best vybira pomoci partial_sort(), pomalejsi

#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "rrcore.h"
#include "RRPackedSolver.h"

//#define SHOW_CONVERGENCE

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// ArrayWithArrays
//
// Array of C1 with each element owning variable length array of C2.
// In single memory block.
// Doesn't call constructors/destructors.
// unsigned C1::arrayOffset must exist.

template <class C1, class C2>
class ArrayWithArrays
{
public:
	//! Constructor used when complete array is acquired (e.g. loaded from disk).
	ArrayWithArrays(char* _data, unsigned _sizeInBytes)
	{
		data = _data;
		sizeInBytes = _sizeInBytes; // full size
		numC1 = data ? getC1(0)->arrayOffset/sizeof(C1)-1 : 0;
	}
	//! Constructor used when empty array is created and then filled with newC1, newC2.
	//
	//! Size must be known in advance.
	//! Filling scheme:
	//! for(index1=0;index1<numC1;index1++)
	//! {
	//!   newC1(index1);
	//!   for(index2=0;index2<...;index2++) newC2();
	//! }
	//! newC1(numC1);
	ArrayWithArrays(unsigned _numC1, unsigned _numC2)
	{
		data = new char[(_numC1+1)*sizeof(C1)+_numC2*sizeof(C2)];
		sizeInBytes = (_numC1+1)*sizeof(C1); // point to first C2, will be increased by newC2
		numC1 = _numC1;
	}
	//! Loads instance from disk.
	static ArrayWithArrays* load(const char* filename)
	{
		FILE* f = fopen(filename,"rb");
		if(!f) return NULL;
		fseek(f,0,SEEK_END);
		unsigned sizeInBytes = ftell(f);
		char* data = new char[sizeInBytes];
		fseek(f,0,SEEK_SET);
		fread(data,1,sizeInBytes,f);
		fclose(f);
		return new ArrayWithArrays(data,sizeInBytes);
	}
	//! Saves instance to disk, returns success.
	bool save(const char* filename) const
	{
		if(!data) return false;
		FILE* f = fopen(filename,"wb");
		if(!f) return false;
		fwrite(data,sizeInBytes,1,f);
		fclose(f);
		return true;
	}
	unsigned getNumC1()
	{
		return numC1;
	}
	//! Returns n-th C1 element.
	C1* getC1(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		return (C1*)data+index1;
	}
	//! Returns first C2 element in of n-th C1 element. Other C2 elements follow until getC2(n+1) is reached.
	C2* getC2(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		return (C2*)(data+getC1(index1)->arrayOffset);
	}
	//! Allocates C1 element. To be used only when array is dynamically created, see constructor for details.
	C1* newC1(unsigned index1)
	{
		RR_ASSERT(index1<=numC1);
		getC1(index1)->arrayOffset = sizeInBytes;
		return getC1(index1);
	}
	//! Allocates C2 element. To be used only when array is dynamically created, see constructor for details.
	C2* newC2()
	{
		sizeInBytes += sizeof(C2);
		return (C2*)(data+sizeInBytes-sizeof(C2));
	}
	//! Size of our single memory block. Don't use when array is not fully filled.
	unsigned getMemoryOccupied() const
	{
		return sizeInBytes;
	}
	~ArrayWithArrays()
	{
		delete[] data;
	}
protected:
	// format dat v jednom souvislem bloku:
	//   C1 c1[numC1+1];
	//   C2 c2[numC2];
	char* data;
	unsigned sizeInBytes;
	unsigned numC1;
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactor
//
// form faktor

enum
{
	BITS_FOR_VISIBILITY = 12,
	BITS_FOR_TRIANGLE_INDEX = sizeof(unsigned)*8-BITS_FOR_VISIBILITY,
};
/*
class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>=0);
		RR_ASSERT(_visibility<=1);
		RR_ASSERT(_destinationTriangle<(1<<BITS_FOR_TRIANGLE_INDEX));
		visibility = (unsigned)(_visibility*((1<<BITS_FOR_VISIBILITY)-1));
		RR_ASSERT(visibility);
		destinationTriangle = _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility * 2.3283064370807973754314699618685e-10f;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle;
	}
private:
	unsigned visibility          : BITS_FOR_VISIBILITY;
	unsigned destinationTriangle : BITS_FOR_TRIANGLE_INDEX;
};*/

class PackedFactor
{
public:
	void set(RRReal _visibility, unsigned _destinationTriangle)
	{
		RR_ASSERT(_visibility>=0);
		RR_ASSERT(_visibility<=1);
		visibility = _visibility;
		destinationTriangle = _destinationTriangle;
	}
	RRReal getVisibility() const
	{
		return visibility;
	}
	unsigned getDestinationTriangle() const
	{
		return destinationTriangle;
	}
private:
	float visibility;
	unsigned destinationTriangle;
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsThread
//
// form faktory pro celou jednu statickou scenu

class PackedFactorHeader
{
public:
	unsigned arrayOffset;
};

typedef ArrayWithArrays<PackedFactorHeader,PackedFactor> PackedFactorsThread;


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsProcess
//

class PackedFactorsProcess
{
public:
	PackedFactorsProcess()
	{
		numThreads = 0;
	}
	~PackedFactorsProcess()
	{
		while(numThreads)
			delete threads[--numThreads];
	}

	void push_back(PackedFactorsThread* th)
	{
		threads[numThreads++] = th;
	}
	unsigned getNumThreads()
	{
		return numThreads;
	}
	PackedFactorsThread* getThread(unsigned threadNum)
	{
		RR_ASSERT(threadNum<numThreads);
		return threads[threadNum];
	}
	unsigned getMemoryOccupied()
	{
		unsigned size = 0;
		for(unsigned i=0;i<numThreads;i++) size += threads[i]->getMemoryOccupied();
		return size;
	}
protected:
	unsigned numThreads;
	PackedFactorsThread* threads[256];
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedSmoothing

class PackedSmoothIvertex
{
public:
	unsigned arrayOffset;
};

class PackedSmoothTriangleWeight
{
public:
	unsigned triangleIndex;
	RRReal weight;
};

//! Precomputed data stored for each ivertex.
typedef ArrayWithArrays<PackedSmoothIvertex,PackedSmoothTriangleWeight> PackedIvertices;

//! Precomputed data stored for each triangle.
class PackedSmoothTriangle
{
public:
	unsigned ivertexIndex[3];
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedSolverFile

class PackedSolverFile
{
public:
	PackedSolverFile()
	{
		packedFactorsProcess = NULL;
		packedIvertices = NULL;
		packedSmoothTriangles = NULL;
		packedSmoothTrianglesBytes = 0;
	}
	unsigned getMemoryOccupied()
	{
		return packedFactorsProcess->getMemoryOccupied() + packedIvertices->getMemoryOccupied() + packedSmoothTrianglesBytes;
	}
	~PackedSolverFile()
	{
		delete packedFactorsProcess;
		delete packedIvertices;
		delete packedSmoothTriangles;
	}
	//class PackedFactors* packedFactors;
	PackedFactorsProcess* packedFactorsProcess;
	PackedIvertices* packedIvertices;
	PackedSmoothTriangle* packedSmoothTriangles;
	unsigned packedSmoothTrianglesBytes;
};


//////////////////////////////////////////////////////////////////////////////
//
// Scene

PackedSolverFile* Scene::packSolver(unsigned numThreads) const
{
	PackedSolverFile* packedSolverFile = new PackedSolverFile;
	RRReportInterval report(INF2,"Packing solver...\n");

	/////////////////////////////////////////////////////////////////////////
	//
	// pack factors

	// calculate size
	unsigned numFactors = 0;
	for(unsigned i=0;i<object->triangles;i++)
	{
		//!!! optimize, sort factors
		numFactors += object->triangle[i].shooter->factors();
	}

	// alloc
	packedSolverFile->packedFactorsProcess = new PackedFactorsProcess;

	// build threads
	for(unsigned threadNum=0;threadNum<numThreads;threadNum++)
	{
		// alloc thread
		PackedFactorsThread* packedFactorsBuilder = new PackedFactorsThread(object->triangles,numFactors);
		// fill thread
		for(unsigned i=0;i<object->triangles;i++)
		{
			// write factors
			packedFactorsBuilder->newC1(i);
			for(unsigned j=0;j<object->triangle[i].shooter->factors();j++)
			{
				const Factor* factor = object->triangle[i].shooter->get(j);
				RR_ASSERT(IS_TRIANGLE(factor->destination));
				unsigned destinationTriangle = (unsigned)( TRIANGLE(factor->destination)-object->triangle );
				RR_ASSERT(destinationTriangle<object->triangles);
				if((destinationTriangle%numThreads)==threadNum)
					packedFactorsBuilder->newC2()->set(factor->power,destinationTriangle);
			}
		}
		packedFactorsBuilder->newC1(object->triangles);
		// insert thread to process
		packedSolverFile->packedFactorsProcess->push_back(packedFactorsBuilder);
	}

	/////////////////////////////////////////////////////////////////////////
	//
	// pack smoothing

	// vstup: z trianglu vedou pointery na ivertexy (ktere nemaji zadne indexy)
	// vystup: z packed trianglu vedou indexy na packed ivertexy (ktere tedy maji indexy)
	// alg:
	// 1 pruchod pres triangly:
	//     ivertexu vynuluje poradove cislo
	// 2 pruchod pres triangly:
	//     do ivertexu zapise jeho poradove cislo
	//     ziska pocet ivertexu a corneru
	// 3 naalokuje packed
	// 4 pruchod pres triangly:
	//     do packed zapisuje ivertexy
	//     do packed zapisuje poradova cisla

	// 1 pruchod pres triangly:
	//     ivertexu vynuluje poradove cislo
	for(unsigned t=0;t<object->triangles;t++)
		for(unsigned v=0;v<3;v++)
		{
			IVertex* ivertex = object->triangle[t].topivertex[v];
			if(ivertex) ivertex->packedIndex = UINT_MAX;
		}

	// 2 pruchod pres triangly:
	//     do ivertexu zapise jeho poradove cislo
	//     ziska pocet ivertexu a corneru
	unsigned numIvertices = 0;
	unsigned numWeights = 0;
	for(unsigned t=0;t<object->triangles;t++)
		for(unsigned v=0;v<3;v++)
		{
			IVertex* ivertex = object->triangle[t].topivertex[v];
			if(ivertex && ivertex->packedIndex==UINT_MAX)
			{
				unsigned oldNumWeights = numWeights;
				for(unsigned k=0;k<ivertex->corners;k++)
					if(IS_TRIANGLE(ivertex->corner[k].node))
						numWeights++;
				if(numWeights>oldNumWeights)
				{
					ivertex->packedIndex = numIvertices++;
				}
			}
		}

	// 3 naalokuje packed
	packedSolverFile->packedSmoothTriangles = new PackedSmoothTriangle[object->triangles];
	packedSolverFile->packedSmoothTrianglesBytes = sizeof(PackedSmoothTriangle)*object->triangles;
	packedSolverFile->packedIvertices = new PackedIvertices(numIvertices,numWeights);

	// 4 pruchod pres triangly:
	//     do packed zapisuje ivertexy
	//     do packed zapisuje poradova cisla
	unsigned numIverticesPacked = 0;
	for(unsigned t=0;t<object->triangles;t++)
		for(unsigned v=0;v<3;v++)
		{
			const IVertex* ivertex = object->triangle[t].topivertex[v];
			if(ivertex)
			{
				if(ivertex->packedIndex==numIverticesPacked)
				{
					packedSolverFile->packedIvertices->newC1(numIverticesPacked++);
					for(unsigned k=0;k<ivertex->corners;k++)
						if(IS_TRIANGLE(ivertex->corner[k].node))
						{
							PackedSmoothTriangleWeight* triangleWeight = packedSolverFile->packedIvertices->newC2();
							triangleWeight->triangleIndex = (unsigned)(TRIANGLE(ivertex->corner[k].node)-object->triangle);
							triangleWeight->weight = ivertex->corner[k].power / ivertex->powerTopLevel;
						}
				}
				RR_ASSERT(ivertex->packedIndex<numIvertices);
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = ivertex->packedIndex;
			}
			else
			{
				// ivertex missing, we have no lighting data
				// set any value that won't crash it
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = 0;
			}
		}
	packedSolverFile->packedIvertices->newC1(numIverticesPacked);
	RR_ASSERT(numIverticesPacked==numIvertices);

	// return
	RRReporter::report(INF2,"Size: factors=%d smoothing=%d total=%d kB.\n",
		( packedSolverFile->packedFactorsProcess->getMemoryOccupied() )/1024,
		( packedSolverFile->packedIvertices->getMemoryOccupied()+packedSolverFile->packedSmoothTrianglesBytes )/1024,
		( packedSolverFile->getMemoryOccupied() )/1024
		);
	return packedSolverFile;
}


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
		real bestQ[BESTS];
//		bestQ[0] = -1; // first is always valid
		bestQ[BESTS-1] = -1; // last is always valid
		for(unsigned i=indexBegin;i<indexEnd;i++)
		{
			// calculate quality of distributor
			real q = sum(triangle[i].incidentFluxToDiffuse);

//#define BEST_RANGE 0.01f // best[last] musi byt aspon 0.01*best[0], jinak je lepsi ho zahodit
//			if(q>bestQ[BESTS-1] && q>bestQ[0]*BEST_RANGE)
			if(q>bestQ[BESTS-1])
			{
				// sort [q,node] into best cache, bestQ[0] is highest
				bests = MIN(BESTS,bests+1);
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
// PackedBestsThreaded
//
// struktura zodpovedna za vyber nejsilnejsich shooteru pro scatter
// multithreadova verze

class PackedBestsThreaded
{
public:
	PackedBestsThreaded(const PackedTriangle* triangles, unsigned numTriangles)
	{
		for(unsigned i=0;i<NUM_THREADS;i++)
			threads[i].init(triangles,i,numTriangles,NUM_THREADS);
	}

	void reset()
	{
		for(int i=0;i<NUM_THREADS;i++)
			threads[i].reset();
	}

	// m-threaded interface, to be run from 1 thread when other threads don't call getSelectedBest()
	//  returns number of triangles in selected group
	unsigned selectBests()
	{
		RRReportInterval report(INF2,"Finding bests400...\n");//!!!
		unsigned bests[NUM_THREADS];
#pragma omp parallel for
		for(int i=0;i<NUM_THREADS;i++)
			bests[i] = threads[i].selectBests();
		//!!! NUM_THREADS
		if(bests[0]!=bests[1])
			RRReporter::report(ERRO,"ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		return bests[0]+bests[1];
	}

	// m-threaded interface, to be run from many threads
	//  returns i-th triangle from group of selectBests() triangles
	unsigned getSelectedBest(unsigned i) const
	{
		return threads[i%NUM_THREADS].getSelectedBest(i/NUM_THREADS);
	}

	// 1-threaded interface
	//  returns best triangle
	unsigned getBest()
	{
		return threads[iterator++%NUM_THREADS].getBest();
	}
protected:
	enum
	{
		NUM_THREADS=2
	};
	PackedBests threads[NUM_THREADS];
	unsigned iterator;
};


//////////////////////////////////////////////////////////////////////////////
//
// RRPackedSolver

RRPackedSolver::RRPackedSolver(const RRObject* _object)
{
	object = _object;
	if(object)
	{
		const RRMesh* mesh = object->getCollider()->getMesh();
		numTriangles = mesh->getNumTriangles();
		triangles = new PackedTriangle[numTriangles];
		for(unsigned i=0;i<numTriangles;i++)
		{
			const RRMaterial* material = object->getTriangleMaterial(i);
			triangles[i].diffuseReflectance = material ? material->diffuseReflectance : RRVec3(0.5f);
			triangles[i].area = mesh->getTriangleArea(i);
		}
	}
	else
	{
		numTriangles = 0;
		triangles = NULL;
	}
	packedSolverFile = NULL;
	packedBests = NULL;
	ivertexIndirectIrradiance = NULL;
	triangleIrradianceIndirectDirty = true;
}

void RRPackedSolver::loadFile(PackedSolverFile* adopt_packedSolverFile)
{
	// remember precomputed file
	delete packedSolverFile;
	packedSolverFile = adopt_packedSolverFile;

	// update runtime computed data
	if(packedBests) packedBests->reset();
	delete[] ivertexIndirectIrradiance;
	ivertexIndirectIrradiance = packedSolverFile ? new RRVec3[packedSolverFile->packedIvertices->getNumC1()] : NULL;
	triangleIrradianceIndirectDirty = true;
}

void RRPackedSolver::illuminationReset()
{
	triangleIrradianceIndirectDirty = true;
	if(packedBests) packedBests->reset();
#pragma omp parallel for
	for(int t=0;(unsigned)t<numTriangles;t++)
	{
		object->getTriangleIllumination(t,RRRadiometricMeasure(0,0,1,1,1),triangles[t].incidentFluxDirect);
		triangles[t].incidentFluxToDiffuse = triangles[t].incidentFluxDirect;
		triangles[t].incidentFluxDiffused = RRVec3(0);
	}
}

void RRPackedSolver::illuminationImprove(bool endfunc(void *), void *context)
{
	triangleIrradianceIndirectDirty = true;
	PackedFactorsThread* thread0 = packedSolverFile->packedFactorsProcess->getThread(0);
	if(!packedBests) packedBests = new PackedBests; packedBests->init(triangles,0,numTriangles,1);
	do
	{
		if(packedSolverFile->packedFactorsProcess->getNumThreads()==1)
		{
			// 1-threaded propagation, s okamzitym zapojenim prijate energe do dalsiho strileni

			unsigned sourceTriangleIndex = packedBests->getBest();
			if(sourceTriangleIndex==UINT_MAX) break;
			RR_ASSERT(sourceTriangleIndex!=UINT_MAX);
			PackedTriangle* source = &triangles[sourceTriangleIndex];
			// incidentFluxDiffused = co uz vystrilel
			// incidentFluxToDiffuse = co ma jeste vystrilet
			Channels exitingFluxToDiffuse = source->incidentFluxToDiffuse * source->diffuseReflectance;
			source->incidentFluxDiffused += source->incidentFluxToDiffuse;
			source->incidentFluxToDiffuse = Channels(0);
			const PackedFactor* start = thread0->getC2(sourceTriangleIndex);
			const PackedFactor* stop  = thread0->getC2(sourceTriangleIndex+1);
			for(;start<stop;start++)
			{
				RR_ASSERT(start->getDestinationTriangle()<numTriangles);
				triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
					exitingFluxToDiffuse*start->getVisibility();
			}
		}
		else
		{
			// m-threaded propagation, s opozdenym zapojenim prijate energie do dalsiho strileni

			// select group of bests
			unsigned bests = packedBests->selectBests();

			// make copy of initial energies to diffuse
			// set shooters as if energy was already propagated
			Channels exitingFluxToDiffuse[BESTS*2]; //!!! packedFactorsProcess->getNumThreads() misto 2
			for(unsigned i=0;i<bests;i++)
			{
				PackedTriangle* source = &triangles[packedBests->getSelectedBest(i)];
				exitingFluxToDiffuse[i] = source->incidentFluxToDiffuse * source->diffuseReflectance;
				source->incidentFluxDiffused += source->incidentFluxToDiffuse;
				source->incidentFluxToDiffuse = Channels(0);
			}

			// propagate group in parallel
#ifdef _OPENMP
			#pragma omp parallel num_threads(packedSolverFile->packedFactorsProcess->getNumThreads())
			{
				unsigned numThreads = omp_get_num_threads();
				unsigned threadNum = omp_get_thread_num();
#else
			unsigned numThreads = packedSolverFile->packedFactorsProcess->getNumThreads();
			for(unsigned threadNum = 0;threadNum<numThreads;threadNum++)
			{
#endif
				PackedFactorsThread* packedFactorsThread = packedSolverFile->packedFactorsProcess->getThread(threadNum);
				for(unsigned i=0;i<bests;i++)
				{
					unsigned sourceTriangleIndex = packedBests->getSelectedBest(i);
					// incidentFluxDiffused = co uz vystrilel
					// incidentFluxToDiffuse = co ma jeste vystrilet
					const PackedFactor* start = packedFactorsThread->getC2(sourceTriangleIndex);
					const PackedFactor* stop  = packedFactorsThread->getC2(sourceTriangleIndex+1);
					for(;start<stop;start++)
					{
						RR_ASSERT(start->getDestinationTriangle()<numTriangles);
						triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
							exitingFluxToDiffuse[i] * start->getVisibility();
					}
				}
			}
		}
	}
	while(!endfunc(context));

}

RRVec3 RRPackedSolver::getTriangleExitance(unsigned triangle) const
{
	return triangles[triangle].getExitance();
}

void RRPackedSolver::getTriangleIrradianceIndirectUpdate()
{
	if(!triangleIrradianceIndirectDirty) return;
	triangleIrradianceIndirectDirty = false;
	if(!packedSolverFile)
	{
		RR_ASSERT(0);
		return;
	}
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
	if(!packedSolverFile)
	{
		RR_ASSERT(0);
		return NULL;
	}
	RR_ASSERT(vertex<3);
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
