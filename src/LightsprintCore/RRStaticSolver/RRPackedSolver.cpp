#include <algorithm>
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include <vector>
#include "rrcore.h"
#include "RRPackedSolver.h"

namespace rr
{


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

class PackedFactorsThread
{
public:
	PackedFactorsThread(unsigned numTriangles, unsigned numFactors)
	{
		RR_ASSERT(numTriangles<(1<<BITS_FOR_TRIANGLE_INDEX));
		data = new char[(numTriangles+1)*sizeof(unsigned)+numFactors*sizeof(PackedFactor)];
	}
	PackedFactorsThread(char* _data, unsigned _sizeInBytes)
	{
		data = _data;
		sizeInBytes = _sizeInBytes;
	}
	static PackedFactorsThread* load(const char* filename)
	{
		FILE* f = fopen(filename,"rb");
		if(!f) return NULL;
		fseek(f,0,SEEK_END);
		unsigned sizeInBytes = ftell(f);
		char* data = new char[sizeInBytes];
		fseek(f,0,SEEK_SET);
		fread(data,1,sizeInBytes,f);
		fclose(f);
		return new PackedFactorsThread(data,sizeInBytes);
	}
	bool save(const char* filename) const
	{
		if(!data) return false;
		FILE* f = fopen(filename,"wb");
		if(!f) return false;
		fwrite(data,sizeInBytes,1,f);
		fclose(f);
		return true;
	}
	const PackedFactor* getFactors(unsigned sourceTriangleIndex) const
	{
		return (PackedFactor*)(data+((unsigned*)data)[sourceTriangleIndex]);
	}
	unsigned getSize() const
	{
		return sizeInBytes;
	}
	~PackedFactorsThread()
	{
		delete[] data;
	}
protected:
	// format dat v jednom souvislem bloku:
	//   unsigned offset[numTriangles+1] ... offsety (v bytech od zacatku souboru) na zacatek seznamu faktoru vychazejicich z trianglu
	//   PackedFactor factors[?] ... faktory
	char* data;
	unsigned sizeInBytes;
};


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
		for(unsigned i=0;i<numThreads;i++) size += threads[i]->getSize();
		return size;
	}
protected:
	unsigned numThreads;
	PackedFactorsThread* threads[256];
};


//////////////////////////////////////////////////////////////////////////////
//
// PackedFactorsBuilder
//
// used while building packed factors

class PackedFactorsBuilder : public PackedFactorsThread
{
public:
	PackedFactorsBuilder(unsigned numTriangles, unsigned numFactors)
		: PackedFactorsThread(numTriangles,numFactors)
	{
		sizeInBytes = sizeof(unsigned)*(numTriangles+1);
	}
	void allocFactors(unsigned sourceTriangleIndex)
	{
		((unsigned*)data)[sourceTriangleIndex] = sizeInBytes;
	}
	PackedFactor* allocFactor()
	{
		sizeInBytes += sizeof(PackedFactor);
		return (PackedFactor*)(data+sizeInBytes-sizeof(PackedFactor));
	}
};


//////////////////////////////////////////////////////////////////////////////
//
// Scene

// spakuje form faktory do souvisleho bloku pameti
PackedFactorsProcess* Scene::packFactors(unsigned numThreads) const
{
	// calculate size
	unsigned numFactors = 0;
	for(unsigned i=0;i<object->triangles;i++)
	{
		//!!! optimize, sort factors
		numFactors += object->triangle[i].shooter->factors();
	}
	// alloc
	PackedFactorsProcess* packedFactorsProcess = new PackedFactorsProcess;

	// build threads
	for(unsigned threadNum=0;threadNum<numThreads;threadNum++)
	{
		PackedFactorsBuilder* packedFactorsBuilder = new PackedFactorsBuilder(object->triangles,numFactors);
		// write
		for(unsigned i=0;i<object->triangles;i++)
		{
			// write factors
			packedFactorsBuilder->allocFactors(i);
			for(unsigned j=0;j<object->triangle[i].shooter->factors();j++)
			{
				const Factor* factor = object->triangle[i].shooter->get(j);
				RR_ASSERT(IS_TRIANGLE(factor->destination));
				unsigned destinationTriangle = (unsigned)( TRIANGLE(factor->destination)-object->triangle );
				RR_ASSERT(destinationTriangle<object->triangles);
				if((destinationTriangle%numThreads)==threadNum)
					packedFactorsBuilder->allocFactor()->set(factor->power,destinationTriangle);
			}
		}
		// write last offset
		packedFactorsBuilder->allocFactors(object->triangles);
		// insert thread to process
		packedFactorsProcess->push_back(packedFactorsBuilder);
	}

	RRReporter::report(INF2,"Created form factors (%d kB).\n",packedFactorsProcess->getMemoryOccupied()/1024);
	return packedFactorsProcess;
}


//////////////////////////////////////////////////////////////////////////////
//
// PackedBests
//
// struktura zodpovedna za vyber nejsilnejsich shooteru pro scatter

class PackedBests
{
public:
	void init(const std::vector<PackedTriangle>* _triangle, unsigned _indexBegin, unsigned _indexEnd)
	{
		triangle = _triangle;
		indexBegin = _indexBegin;
		indexEnd = _indexEnd;
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
		// search 200 shooters with most energy to diffuse
		reset();
		real bestQ[BESTS];
//		bestQ[0] = -1; // first is always valid
		bestQ[BESTS-1] = -1; // last is always valid
		for(unsigned i=indexBegin;i<indexEnd;i++) //!!! NUM_THREADS misto 2
		{
			// calculate quality of distributor
			real q = sum((*triangle)[i].incidentFluxToDiffuse);

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
		if(bests) RRReporter::report(INF3,"bestQ[0]=%f bestQ[%d]=%f\n",bestQ[0],bests-1,bestQ[bests-1]);//!!!
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
	unsigned bests;
	unsigned bestNode[BESTS]; // triangleIndex, indexBegin..indexEnd-1
	unsigned bestNodeIterator;
	const std::vector<PackedTriangle>* triangle; // our data: triangle[indexBegin]..triangle[indexEnd-1]
	unsigned indexBegin;
	unsigned indexEnd;
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
	PackedBestsThreaded(const std::vector<PackedTriangle>* triangles)
	{
		for(unsigned i=0;i<NUM_THREADS;i++)
			threads[i].init(triangles,i,(unsigned)triangles->size());
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
#pragma omp parallel for
		for(int i=0;i<NUM_THREADS;i++)
			bests[i] = threads[i].selectBests();
		//!!! NUM_THREADS
		if(bests[0]!=bests[1])
			printf("ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
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
	unsigned bests[NUM_THREADS];
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
		triangles.resize(mesh->getNumTriangles());
		for(unsigned i=0;i<triangles.size();i++)
		{
			const RRMaterial* material = object->getTriangleMaterial(i);
			triangles[i].diffuseReflectance = material ? material->diffuseReflectance : RRVec3(0.5f);
			triangles[i].area = mesh->getTriangleArea(i);
		}
	}
	packedFactorsProcess = NULL;
	packedBests = NULL;
}

void RRPackedSolver::loadFactors(PackedFactorsProcess* _packedFactorsProcess)
{
	delete packedFactorsProcess;
	packedFactorsProcess = _packedFactorsProcess;
	if(packedBests) packedBests->reset();
}

/*
// nahraje spakovane form faktory z disku
bool RRPackedSolver::loadPackedFactors(const char* filename)
{
	//!!!delete packedFactorsProcess;
	//!!!packedFactorsProcess = PackedFactorsProcess::load(filename);
	if(packedBests) packedBests->reset();
	return packedFactorsProcess!=NULL;
}
*/
void RRPackedSolver::illuminationReset()
{
	if(packedBests) packedBests->reset();
#pragma omp parallel for
	for(int t=0;(unsigned)t<triangles.size();t++)
	{
		object->getTriangleIllumination(t,RRRadiometricMeasure(0,0,1,1,1),triangles[t].incidentFluxDirect);
		triangles[t].incidentFluxToDiffuse = triangles[t].incidentFluxDirect;
		triangles[t].incidentFluxDiffused = RRVec3(0);
	}
}

void RRPackedSolver::illuminationImprove(bool endfunc(void *), void *context)
{
	unsigned factorsUsed[4] = {0,0,0,0};
	PackedFactorsThread* thread0 = packedFactorsProcess->getThread(0);
	if(!packedBests) packedBests = new PackedBests; packedBests->init(&triangles,0,(unsigned)triangles.size());
	do
	{
		if(packedFactorsProcess->getNumThreads()==1)
		{
			// 1-threaded propagation, s okamzitym zapojenim prijate energe do dalsiho strileni

			unsigned sourceTriangleIndex = packedBests->getBest();
			PackedTriangle* source = &triangles[sourceTriangleIndex];
			// incidentFluxDiffused = co uz vystrilel
			// incidentFluxToDiffuse = co ma jeste vystrilet
			Channels exitingFluxToDiffuse = source->incidentFluxToDiffuse * source->diffuseReflectance;
			source->incidentFluxDiffused += source->incidentFluxToDiffuse;
			source->incidentFluxToDiffuse = Channels(0);
			const PackedFactor* start = thread0->getFactors(sourceTriangleIndex);
			const PackedFactor* stop  = thread0->getFactors(sourceTriangleIndex+1);
			for(;start<stop;start++)
			{
				RR_ASSERT(start->getDestinationTriangle()<triangles.size());
				triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
					exitingFluxToDiffuse*start->getVisibility();
				factorsUsed[0]++;
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
			#pragma omp parallel num_threads(packedFactorsProcess->getNumThreads())
			{
				unsigned numThreads = omp_get_num_threads();
				unsigned threadNum = omp_get_thread_num();
#else
			unsigned numThreads = packedFactorsProcess->getNumThreads();
			for(unsigned threadNum = 0;threadNum<numThreads;threadNum++)
			{
#endif
				PackedFactorsThread* packedFactorsThread = packedFactorsProcess->getThread(threadNum);
				for(unsigned i=0;i<bests;i++)
				{
					unsigned sourceTriangleIndex = packedBests->getSelectedBest(i);
					// incidentFluxDiffused = co uz vystrilel
					// incidentFluxToDiffuse = co ma jeste vystrilet
					const PackedFactor* start = packedFactorsThread->getFactors(sourceTriangleIndex);
					const PackedFactor* stop  = packedFactorsThread->getFactors(sourceTriangleIndex+1);
					for(;start<stop;start++)
					{
						RR_ASSERT(start->getDestinationTriangle()<triangles.size());
						triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
							exitingFluxToDiffuse[i] * start->getVisibility();
						factorsUsed[threadNum]++;
					}
				}
			}
		}
	}
	while(!endfunc(context));

	// statistika
	RRReal unshot = 0;
	RRReal shot = 0;
	for(unsigned i=0;i<triangles.size();i++)
	{
		shot += sum(triangles[i].incidentFluxDiffused * triangles[i].diffuseReflectance);
		unshot += sum(triangles[i].incidentFluxToDiffuse * triangles[i].diffuseReflectance);
	}
	RRReporter::report(INF3,"Converged=%f, propagated factors=%d=%d+%d, packed ff size=%d kB\n",shot/(shot+unshot),factorsUsed[0]+factorsUsed[1],factorsUsed[0],factorsUsed[1],packedFactorsProcess->getMemoryOccupied()/1024);
}

RRVec3 RRPackedSolver::getTriangleExitance(unsigned triangle) const
{
	return triangles[triangle].getExitance();
}

RRVec3 RRPackedSolver::getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex, RRStaticSolver* staticSolver) const
{
	//!!! pouzivat vlastni spakovane cornery, pak nebudu potrebovat staticSolver
	return RRVec3(0);
}

RRPackedSolver::~RRPackedSolver()
{
	delete packedBests;
	delete packedFactorsProcess;
}

} // namespace
