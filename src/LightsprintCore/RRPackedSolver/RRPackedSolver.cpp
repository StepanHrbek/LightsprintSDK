// --------------------------------------------------------------------------
// Fireball, lightning fast realtime GI solver.
// Copyright (c) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


//#define PARTIAL_SORT // best vybira pomoci partial_sort(), sponzu zpomali ze 103 na 83, z 65 na 49
//#define SHOW_CONVERGENCE
#define MAX_BESTS 1000

#include "RRPackedSolver.h"
#include "PackedSolverFile.h"
#include "Lightsprint/RRLight.h" // scaler
#ifdef _OPENMP
	#include <omp.h> // known error in msvc manifest code: needs omp.h even when using only pragmas
#endif
#include "../RRDynamicSolver/report.h"
#include "../RRMathPrivate.h"
	#include <boost/random/linear_congruential.hpp>

//#define SMALL_SHOOTERS_MORE_IMPORTANT

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// PackedColor - RRVec3 in 4 bytes

class PackedColor
{
public:
	void clear()
	{
		multiplier = 0;
	}
	void operator =(RRVec3 _a)
	{
		multiplier = _a.sum();
		if (multiplier)
		{
			r = (unsigned char)(255/multiplier*_a[0]);
			g = (unsigned char)(255/multiplier*_a[1]);
		}
	}
	// *this = a.get()*b;
	void setProduct(const PackedColor& _a,float _b)
	{
		multiplier = _a.multiplier*_b;
		r = _a.r;
		g = _a.g;
	}
	RRVec3 get() const
	{
		return RRVec3((RRReal)r,(RRReal)g,(RRReal)(255-r-g))*(multiplier/255);
	}
	RRReal sum() const
	{
		return multiplier;
	}

private:
	//unsigned char& r() {return ((unsigned char*)this)[2];}
	//unsigned char& g() {return ((unsigned char*)this)[3];}
	union
	{
		float multiplier;
		struct
		{
			//!!! not tested on powerpc, could need filler first
			//!!! not tested in gcc, could need some sacrifice to aliasing gods
			unsigned char r;
			unsigned char g;
			unsigned short filler;
		};
	};
};



//////////////////////////////////////////////////////////////////////////////
//
// PackedTriangle

class PackedTriangle
{
public:
	RRVec3 incidentFluxToDiffuse; // reset to direct illum, modified by improve
	PackedColor emissiveFluxToDiffuse; // reset to direct emissive flux, cleared by improve
	RRVec3 incidentFluxDiffused;  // reset to 0, modified by improve
	PackedColor diffuseEmittance; // set by setEmittance
	RRVec3 incidentFluxDirect;    // reset to direct illum
	RRReal area;
	RRVec3 incidentFluxSky;       // constructed to 0, modified by setEnvironment
	RRReal areaInv;               // area^-1
	const RRMaterial* material;   // may be replaced by RRVec3 diffuseReflectance,diffuseEmittance;

	// used by solver during calculation

	// reads diffuseReflectance from material (or possibly from our copy to make memory accesses localized)
	const RRVec3& getDiffuseReflectance() const
	{
		return material->diffuseReflectance.color;
	}
	// importance is amount of unshot energy. important triangles are processed first
	RRReal getImportance() const
	{
		// to save time, incident flux is not multiplied by diffuseReflectance
		// this makes importance slightly incorrect, but it probably still pays off
		// makes 2*brighter and 2*bigger shooters equally important
		return incidentFluxToDiffuse.sum() + emissiveFluxToDiffuse.sum();
	}

	// used from outside to read results

	// for dynamic objects (point material)
	RRVec3 getIrradiance() const
	{
		RR_ASSERT(IS_VEC3(((incidentFluxDiffused+incidentFluxToDiffuse+incidentFluxSky)*areaInv)));
		return (incidentFluxDiffused+incidentFluxToDiffuse+incidentFluxSky)*areaInv;
	}
	// for dynamic objects (per-tri material), includes emissivity
	RRVec3 getExitance() const
	{
		RR_ASSERT(IS_VEC3((getIrradiance()*getDiffuseReflectance())));
		return getIrradiance()*getDiffuseReflectance()+diffuseEmittance.get();
	}
	// for static objects, includes skylight
	RRVec3 getIncidentFluxIndirect() const
	{
		return incidentFluxDiffused+incidentFluxToDiffuse+incidentFluxSky-incidentFluxDirect;
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
	unsigned selectBests(unsigned maxBests)
	{
		//RRReportInterval report(INF2,"Finding bests200...\n");//!!!
		reset();


		// search 200 shooters with most energy to diffuse
		RRReal bestQ[MAX_BESTS];
		bestQ[maxBests-1] = 0; // last is always valid
		for (unsigned i=indexBegin;i<indexEnd;i++)
		{
			// calculate quality of distributor
			RRReal q = triangle[i].getImportance();

			if (q>bestQ[maxBests-1])
			{
				// sort [q,node] into best cache, bestQ[0] is highest
				if (bests<maxBests) bests++;
				unsigned pos = bests-1;
				for (; pos>0 && q>bestQ[pos-1]; pos--)
				{
					// probublavam mezi spravne velkymi besty smerem k lepsim
					bestNode[pos] = bestNode[pos-1];
					bestQ[pos] = bestQ[pos-1];
				}
				bestNode[pos] = i;
				bestQ[pos] = q;
			}
		}
	//if (bests) RRReporter::report(INF2,"bestQ[0]=%f bestQ[%d]=%f\n",bestQ[0],bests-1,bestQ[bests-1]);//!!!


		highestFluxToDistribute = bests ? bestQ[0] : 0;
		return bests;
	}

	// m-threaded interface, to be run from many threads
	//  returns i-th triangle from group of selectBests() triangles
	unsigned getSelectedBest(unsigned i) const
	{
		RR_ASSERT(i<bests);
		return bestNode[i];
	}

	// valid only after selectBests()
	RRReal getHighestFluxToDistribute() const
	{
		return highestFluxToDistribute;
	}


protected:
	unsigned bests; ///< Number of elements in bestNode[] array.
	unsigned bestNode[MAX_BESTS]; ///< Runtime selected triangle indices in range <indexBegin..indexEnd-1>.
	unsigned bestNodeIterator; ///< Iterator used by 1threaded getBest(), index into bestNode[] array.
	const PackedTriangle* triangle; ///< Pointer to our data: triangle[indexBegin]..triangle[indexEnd-1].
	unsigned indexBegin; ///< Index into triangle array, first of our triangles.
	unsigned indexEnd; ///< Index into triangle array, last+1 of our triangles.
	unsigned indexStep; ///< Step used in triangle array, we access only triangles: indexBegin, indexBegin+indexStep, indexBegin+2*indexStep, ...
	RRReal highestFluxToDistribute;
};






//////////////////////////////////////////////////////////////////////////////
//
// RRPackedSolver

RRPackedSolver::RRPackedSolver(const RRObject* _object, const PackedSolverFile* _adopt_packedSolverFile)
{
	// constant precomputed data
	packedSolverFile = _adopt_packedSolverFile;

	// constant realtime acquired data
	object = _object;
	const RRMesh* mesh = object->getCollider()->getMesh();
	numTriangles = mesh->getNumTriangles();
	triangles = new PackedTriangle[numTriangles];
	defaultMaterial.reset(false);
	for (unsigned t=0;t<numTriangles;t++)
	{
		const RRMaterial* material = object->getTriangleMaterial(t,NULL,NULL);
		triangles[t].material = material ? material : &defaultMaterial;
		triangles[t].area = mesh->getTriangleArea(t);
		triangles[t].areaInv = triangles[t].area ? 1/triangles[t].area : 1; // so we don't return INF exitance from degenerated triangle (now we mostly return 0)
		RR_ASSERT(_finite(triangles[t].area));
		RR_ASSERT(_finite(triangles[t].areaInv));
		// reset at least once, all future resets may be incremental
		triangles[t].incidentFluxDirect = RRVec3(0);
		triangles[t].incidentFluxToDiffuse = RRVec3(0);
		triangles[t].incidentFluxDiffused = RRVec3(0);
		triangles[t].incidentFluxSky = RRVec3(0);
		triangles[t].diffuseEmittance.clear();
		triangles[t].emissiveFluxToDiffuse.clear();
	}

	// varying data
	packedBests = new PackedBests; packedBests->init(triangles,0,numTriangles,1);
	ivertexIndirectIrradiance = new RRVec3[packedSolverFile->packedIvertices->getNumC1()];
	currentVersionInVertices = 0;
	currentVersionInTriangles = 1;
	currentQuality = 0;
	terminalFluxToDistribute = 0;
	// environment caching
	memset(environmentExitancePhysical,0,sizeof(environmentExitancePhysical));
	environmentVersion[0] = 0;
	environmentVersion[1] = 0;
	environmentBlendFactor = 0;
	// material emittance caching
	materialEmittanceVersionSum[0] = 0;
	materialEmittanceVersionSum[1] = 0;
	materialEmittanceMultiplier = 0;
	materialEmittanceStaticQuality = 0;
	materialEmittanceVideoQuality = 0;
	materialEmittanceUsePointMaterials = false;
	numSamplePoints = 0;
	samplePoints = NULL;
}

RRPackedSolver* RRPackedSolver::create(const RRObject* object, const PackedSolverFile* adopt_packedSolverFile)
{
	if (object && adopt_packedSolverFile && adopt_packedSolverFile->isCompatible(object))
		return new RRPackedSolver(object,adopt_packedSolverFile);
	return NULL;
}

//! Converts environment to physical exitances, one per patch.
//! inoutVersion serves as a cache.
//! Optimized for simplicity, can be made faster later.
//! Returns 0 if exitance did not change, 1 if it did.
static unsigned getSkyExitancePhysical(const RRBuffer* inSky, unsigned inStaticQuality, unsigned inVideoQuality, const RRScaler* inScaler, RRVec3 inoutPatchExitancesPhysical[PackedSkyTriangleFactor::NUM_PATCHES], unsigned& inoutSkyVersion)
{
	bool isVideo = inSky && inSky->getDuration();
	unsigned quality = isVideo ? inVideoQuality : inStaticQuality;
	// 0 = do no work
	if (quality==0)
		return 0;
	// use cached result if possible
	unsigned version = (inSky?inSky->version:0)+quality*357;
	if (inoutSkyVersion==version)
		return 0;
	inoutSkyVersion = version;
	// init to zeroes
	unsigned numSamplesGathered[PackedSkyTriangleFactor::NUM_PATCHES];
	for (unsigned p=0;p<PackedSkyTriangleFactor::NUM_PATCHES;p++)
	{
		inoutPatchExitancesPhysical[p] = RRVec3(0);
		numSamplesGathered[p] = 0;
	}
	if (!inSky)
		return 1;
	// do we have to scale sky to physical? no -> null scaler
	if (!inSky->getScaled())
		inScaler = NULL;
	// select random generator
	boost::rand48 rnd; // seed is reset, lower noise
	const unsigned RMAX = rnd.max();
	// accumulate sky exitances
	unsigned numSamples = inSky->getDuration()?inVideoQuality:inStaticQuality;
	for (unsigned i=0;i<numSamples;i++)
	{
		// direction is not perfect, directions to corners are taken slightly more often, we save time by not rejecting corners
		RRVec3 direction((RRReal)((int)rnd()-(int)(RMAX/2)),(RRReal)((int)rnd()-(int)(RMAX/2)),(RRReal)((int)rnd()-(int)(RMAX/2)));
		unsigned patchIndex = PackedSkyTriangleFactor::getPatchIndex(direction);
		RRVec3 exitance = inSky->getElementAtDirection(direction);
		if (inScaler)
			inScaler->getPhysicalScale(exitance);
		inoutPatchExitancesPhysical[patchIndex] += exitance;
		numSamplesGathered[patchIndex]++;
	}
	// average
	for (unsigned p=0;p<PackedSkyTriangleFactor::NUM_PATCHES;p++)
	{
		if (numSamplesGathered[p])
			inoutPatchExitancesPhysical[p] /= (RRReal)numSamplesGathered[p];
	}
	return 1;
}

bool RRPackedSolver::setEnvironment(const RRBuffer* _environment0, const RRBuffer* _environment1, unsigned _environmentStaticQuality, unsigned _environmentVideoQuality, float _environmentBlendFactor, const RRScaler* _scaler)
{
	// convert environment to 13 patches, remember them
	unsigned numChanges = 
		getSkyExitancePhysical(_environment0,_environmentStaticQuality,_environmentVideoQuality,_scaler,environmentExitancePhysical[0],environmentVersion[0]) +
		getSkyExitancePhysical(_environment1,_environmentStaticQuality,_environmentVideoQuality,_scaler,environmentExitancePhysical[1],environmentVersion[1]);

	// caching, quit if things did not change
	if (!numChanges && environmentBlendFactor==_environmentBlendFactor)
		return false;

	// gamma correct blend of sky exitances
	environmentBlendFactor = _environmentBlendFactor;
	PackedSkyTriangleFactor::UnpackedFactor skyExitancePhysicalBlend;
	for (unsigned i=0;i<PackedSkyTriangleFactor::NUM_PATCHES;i++)
		skyExitancePhysicalBlend.patches[i] = environmentExitancePhysical[0][i]*(1-environmentBlendFactor)+environmentExitancePhysical[1][i]*environmentBlendFactor;

	// add environment GI to solution
	for (unsigned t=0;t<numTriangles;t++)
	{
		RRVec3 triangleIrradiancePhysical = packedSolverFile->packedFactors->getC1(t)->packedSkyTriangleFactor.getTriangleIrradiancePhysical(skyExitancePhysicalBlend,packedSolverFile->intensityTable);
		triangles[t].incidentFluxSky = triangleIrradiancePhysical * triangles[t].area;
	}
	currentVersionInTriangles++;
	return true;
}

bool RRPackedSolver::setMaterialEmittance(bool _materialEmittanceForceReload, float _materialEmittanceMultiplier, unsigned _materialEmittanceStaticQuality, unsigned _materialEmittanceVideoQuality, bool _materialEmittanceUsePointMaterials, const RRScaler* _scaler)
{
	// 0 = do no work
	if (!_materialEmittanceStaticQuality && !_materialEmittanceVideoQuality)
		return false;

	// caching, return false if things did not change
	unsigned versionSum[2] = {0,0}; // 0=static, 1=video
	for (unsigned g=0;object && g<object->faceGroups.size();g++)
	{
		const rr::RRMaterial* material = object->faceGroups[g].material;
		if (material && material->diffuseEmittance.texture)
			versionSum[material->diffuseEmittance.texture->getDuration()?1:0] += material->diffuseEmittance.texture->version;
	}
	if (!_materialEmittanceForceReload
		&& (materialEmittanceVersionSum[0]==versionSum[0] || !_materialEmittanceStaticQuality)
		&& (materialEmittanceVersionSum[1]==versionSum[1] || !_materialEmittanceVideoQuality)
		&& materialEmittanceMultiplier==_materialEmittanceMultiplier
		&& materialEmittanceStaticQuality==_materialEmittanceStaticQuality
		&& materialEmittanceVideoQuality==_materialEmittanceVideoQuality
		&& materialEmittanceUsePointMaterials==_materialEmittanceUsePointMaterials)
		return false;

	//RRReportInterval report(INF2,"setEmittance(%d)\n",quality);
	// deterministically generate points in triangle space 0,0 1,0 0,1
	if (numSamplePoints<RR_MAX(_materialEmittanceStaticQuality,_materialEmittanceVideoQuality))
	{
		numSamplePoints = RR_MAX(_materialEmittanceStaticQuality,_materialEmittanceVideoQuality);
		delete[] samplePoints;
		samplePoints = new RRVec2[numSamplePoints];
		static const RRReal dir[4][3]={{0,0,-0.5f},{0.333333f,-0.166666f,0.5f},{-0.166666f,0.333333f,0.5f},{-0.166666f,-0.166666f,0.5f}};
		for (unsigned i=0;i<numSamplePoints;i++)
		{
			RRReal dist=1;
			RRVec2 uv(0.333333f);
			unsigned n = i;
			while (n)
			{
				uv[0] += dist*dir[n&3][0];
				uv[1] += dist*dir[n&3][1];
				dist *= dir[n&3][2];
				n >>= 2;
			}
			samplePoints[i] = uv;
		}
	}

	materialEmittanceVersionSum[0] = versionSum[0];
	materialEmittanceVersionSum[1] = versionSum[1];
	materialEmittanceMultiplier = _materialEmittanceMultiplier;
	materialEmittanceStaticQuality = _materialEmittanceStaticQuality;
	materialEmittanceVideoQuality = _materialEmittanceVideoQuality;
	materialEmittanceUsePointMaterials = _materialEmittanceUsePointMaterials;

	// prepsat na cykl pres fgrupy (typicky usetri spoustu prace tim ze zahodi cely fgrupy bez emisivity), zmerit co je rychlejsi
	#pragma omp parallel for
	for (int t=0;(unsigned)t<numTriangles;t++)
	{
		// removed optimization: very uniform materials (minimalQualityForPointMaterials>=quality*1000) used material color, not texture
		// helped in extremely rare case: scene with mat1.emis = video, mat2.emis = nearly flat texture
		// caused troubles in usual case: user sets emis video and forgets to set emis color, decrease minimalQualityForPointMaterials -> flat emis color (black) is used, no emission visible

		if (_materialEmittanceUsePointMaterials // custom getPointMaterial() does not necessarily depend on diffuseEmittance.texture
			|| triangles[t].material->diffuseEmittance.texture) // direct access depends on diffuseEmittance.texture
		{
			bool isVideo = triangles[t].material->diffuseEmittance.texture && triangles[t].material->diffuseEmittance.texture->getDuration();
			unsigned quality = isVideo?_materialEmittanceVideoQuality:_materialEmittanceStaticQuality;
			if (!quality)
				continue;
			unsigned numSamples = quality-1;
			if (numSamples) // 0 samples = use material averages
			{
				if (!_materialEmittanceUsePointMaterials)
				{
					// fast direct texture access
					// (quality=1 never gets here)
					const RRMaterial::Property& diffuseEmittance = triangles[t].material->diffuseEmittance;
					RRVec3 sum(0);
					for (unsigned i=0;i<numSamples;i++)
					{
						RRMesh::TriangleMapping triangleMapping;
						object->getCollider()->getMesh()->getTriangleMapping(t,triangleMapping,diffuseEmittance.texcoord);
						RRVec2 materialUv = triangleMapping.uv[0]*(1-samplePoints[i][0]-samplePoints[i][1]) + triangleMapping.uv[1]*samplePoints[i][0] + triangleMapping.uv[2]*samplePoints[i][1];
						RRVec3 color = diffuseEmittance.texture->getElementAtPosition(RRVec3(materialUv[0],materialUv[1],0));
						if (_scaler)
						{
							_scaler->getPhysicalScale(color);
						}
						sum += color;
					}
					triangles[t].diffuseEmittance = sum * (_materialEmittanceMultiplier/numSamples);
				}
				else
				{
					// slow point materials
					RRVec3 sum(0);
					for (unsigned i=0;i<numSamples;i++)
					{
						RRPointMaterial material;
						object->getPointMaterial((unsigned)t,samplePoints[i],material);
						sum += material.diffuseEmittance.color;
					}
					triangles[t].diffuseEmittance = sum * (_materialEmittanceMultiplier/numSamples);
				}
				continue;
			}
		}
		// super-fast per-material averages (numSamples = 0)
		triangles[t].diffuseEmittance = triangles[t].material->diffuseEmittance.color * _materialEmittanceMultiplier;
	}

	return true;
}

void RRPackedSolver::illuminationReset(const unsigned* customDirectIrradiance, const RRReal* customToPhysical)
{
	packedBests->reset();
	if (customDirectIrradiance)
	{
		#pragma omp parallel for schedule(static)
		for (int t=0;(unsigned)t<numTriangles;t++)
		{
			unsigned color = customDirectIrradiance[t];
			triangles[t].incidentFluxDiffused = RRVec3(0);
			triangles[t].incidentFluxToDiffuse = triangles[t].incidentFluxDirect = RRVec3(customToPhysical[(color>>24)&255],customToPhysical[(color>>16)&255],customToPhysical[(color>>8)&255]) * triangles[t].area;
			triangles[t].emissiveFluxToDiffuse.setProduct(triangles[t].diffuseEmittance,triangles[t].area);
		}
	}
	else
	{
		for (int t=0;(unsigned)t<numTriangles;t++)
		{
			triangles[t].incidentFluxDiffused = RRVec3(0);
			triangles[t].incidentFluxToDiffuse = triangles[t].incidentFluxDirect = RRVec3(0);
			triangles[t].emissiveFluxToDiffuse.setProduct(triangles[t].diffuseEmittance,triangles[t].area);
		}
	}
	currentVersionInTriangles++;
	currentQuality = 0;
	minimalTopFlux = 1e10f;
	numConsecutiveRoundsThatBeatMinimalTopFlux = 0;
}

void RRPackedSolver::illuminationImprove(unsigned qualityDynamic, unsigned qualityStatic)
{
	if (currentQuality>=qualityStatic) return; // improving in static scene (without reset) is more and more expensive, stop it after n improves
	//RRReportInterval report(INF2,"Improving...\n");


	// adjust pack size to scene size
	// numTriangles->maxBests 16k->200 70k->400 800k->800
	unsigned maxBests = (unsigned)(pow((float)numTriangles,0.35f)*6.5f);
	RR_CLAMP(maxBests,100,MAX_BESTS);

	// 1-threaded propagation, s okamzitym zapojenim prijate energe do dalsiho strileni
	PackedFactorsThread* thread0 = packedSolverFile->packedFactors;
	for (unsigned group=0;group<qualityDynamic;group++)
	{
		unsigned bests = packedBests->selectBests((currentQuality+1==qualityStatic)?maxBests/2:maxBests); // shorten last set of bests
		if (bests)
		{
			RRReal q = packedBests->getHighestFluxToDistribute();
			if (q<minimalTopFlux)
			{
				minimalTopFlux = q;
				numConsecutiveRoundsThatBeatMinimalTopFlux = 0;
			}
			else
			if (numConsecutiveRoundsThatBeatMinimalTopFlux++>20)
			{
				RRReporter::report(WARN,"Diverging solution. Please rebuild Fireball. This could happen if materials change but Fireball is not rebuilt.\n");
				currentQuality = qualityStatic; // don't improve next time
				return; // don't improve now
			}

			//RRReporter::report(INF1,"%f\n",q);
			if (currentQuality==0)
			{
				// this is first improve, set termination criteria
				terminalFluxToDistribute = q/RR_MAX(10000,packedSolverFile->packedIvertices->getNumC1());
			}
			else
			if (q<terminalFluxToDistribute) // terminate
			{
				currentQuality = qualityStatic; // don't improve next time
				return; // don't improve now
			}
		}
		else
		{
			// there's no light to bounce (happens in scenes lit by skylight only)
			currentQuality = qualityStatic; // don't improve next time
			return; // don't improve now
		}
		currentVersionInTriangles += bests;
		for (unsigned i=0;i<bests;i++)
		{
			unsigned sourceTriangleIndex = packedBests->getSelectedBest(i);
			RR_ASSERT(sourceTriangleIndex!=UINT_MAX);
			PackedTriangle* source = &triangles[sourceTriangleIndex];
			RRVec3 exitingFluxToDiffuse = source->incidentFluxToDiffuse * source->getDiffuseReflectance() + source->emissiveFluxToDiffuse.get();
			source->emissiveFluxToDiffuse.clear();
			source->incidentFluxDiffused += source->incidentFluxToDiffuse;
			source->incidentFluxToDiffuse = RRVec3(0);
			const PackedFactor* start = thread0->getC2(sourceTriangleIndex);
			const PackedFactor* stop  = thread0->getC2(sourceTriangleIndex+1);
			for (;start<stop;start++)
			{
				RR_ASSERT(start->getDestinationTriangle()<numTriangles);
				triangles[start->getDestinationTriangle()].incidentFluxToDiffuse +=
					exitingFluxToDiffuse*start->getVisibility();
			}
		}
		currentQuality++;
	}


}

RRVec3 RRPackedSolver::getTriangleIrradiance(unsigned triangle) const
{
	RR_ASSERT(triangle<numTriangles);
	return triangles[triangle].getIrradiance();
}

RRVec3 RRPackedSolver::getTriangleExitance(unsigned triangle) const
{
	RR_ASSERT(triangle<numTriangles);
	return triangles[triangle].getExitance();
}

void RRPackedSolver::getTriangleIrradianceIndirectUpdate()
{
	if (currentVersionInVertices==currentVersionInTriangles) return;
	currentVersionInVertices = currentVersionInTriangles;
	PackedIvertices* packedIvertices = packedSolverFile->packedIvertices;
	int numIvertices = (int)packedIvertices->getNumC1();
#pragma omp parallel for schedule(static)
	for (int i=0;i<numIvertices;i++)
	{
		RRVec3 irrad(0);
		const PackedSmoothTriangleWeight* begin = packedIvertices->getC2(i);
		const PackedSmoothTriangleWeight* end = packedIvertices->getC2(i+1);
		for (;begin<end;begin++)
		{
			irrad += triangles[begin->triangleIndex].getIncidentFluxIndirect() * begin->weight;
		}
		ivertexIndirectIrradiance[i] = irrad;
	}	
}

const RRVec3* RRPackedSolver::getTriangleIrradianceIndirect(unsigned triangle, unsigned vertex) const
{
	if (triangle>=0x3fffffff || vertex>=3 // wrong inputs, shouldn't happen (btw, it's not ffff, UNDEFINED is clamped to 30 + 2 bits)
		|| packedSolverFile->packedSmoothTriangles[triangle].ivertexIndex[vertex]>=packedSolverFile->packedIvertices->getNumC1()) // ffff in packed file -> triangle is degen or needle
	{
		return NULL;
	}
	return &ivertexIndirectIrradiance[packedSolverFile->packedSmoothTriangles[triangle].ivertexIndex[vertex]];
}

bool RRPackedSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRVec3& out) const
{
	RRVec3 irrad;

	if (triangle>=numTriangles)
	{
		RR_ASSERT(0);
		goto zero;
	}

	// enhanced by smoothing
	if (vertex<3 && measure.smoothed)
	{
		if (!measure.direct && measure.indirect)
		{
			const RRVec3* ptr = getTriangleIrradianceIndirect(triangle,vertex);
			if (!ptr) goto zero;
			irrad = *ptr;
		}
		else
		{
			RRReporter::report(WARN,"getTriangleMeasure: fireball doesn't support this measure.\n");
		}
	}
	else
	// basic, fast
	if (!measure.direct && !measure.indirect) 
	{
		irrad = RRVec3(0);
	}
	else
	if (measure.direct && !measure.indirect) 
	{
		irrad = triangles[triangle].incidentFluxDirect*triangles[triangle].areaInv;
	}
	else
	if (measure.direct && measure.indirect) 
	{
		irrad = (triangles[triangle].incidentFluxDiffused+triangles[triangle].incidentFluxToDiffuse)*triangles[triangle].areaInv;
	}
	else
	{
		irrad = (triangles[triangle].incidentFluxDiffused+triangles[triangle].incidentFluxToDiffuse-triangles[triangle].incidentFluxDirect)*triangles[triangle].areaInv;
	}

	if (measure.exiting)
	{
		// diffuse applied on physical scale, not custom scale
		irrad = irrad * triangles[triangle].getDiffuseReflectance() + triangles[triangle].diffuseEmittance.get();
	}
	if (measure.scaled)
	{
		if (scaler)
		{
			// scaler applied on density, not flux
			scaler->getCustomScale(irrad);
		}
		else
		{
			// scaling requested but not available
			RR_ASSERT(0);
		}
	}
	if (measure.flux)
	{
		irrad *= triangles[triangle].area;
	}
	out = irrad;
	return true;
zero:
	out = RRVec3(0);
	return false;
}

RRPackedSolver::~RRPackedSolver()
{
	delete[] samplePoints;
	delete[] triangles;
	delete[] ivertexIndirectIrradiance;
	delete packedBests;
	delete packedSolverFile;
}

} // namespace

