// --------------------------------------------------------------------------
// Fireball, lightning fast realtime GI solver.
// Copyright (c) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


// Build PackedSolverFile, to be used by tools, not by runtime.

#include "PackedSolverFile.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRDynamicSolver.h"

#pragma warning(disable:4530) // unrelated std::vector in private.h complains about exceptions
#include "../RRCollider/cache.h"

#include "../RRDynamicSolver/private.h"

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// Scene

class NeverEnd : public RRStaticSolver::EndFunc
{
public:
	virtual bool requestsEnd() {return false;}
	virtual bool requestsRealtimeResponse() {return false;}
};

PackedSolverFile* Scene::packSolver(unsigned raysFromTriangle)
{
	if (object->triangles>PackedFactor::MAX_TRIANGLES)
	{
		RRReporter::report(WARN,"Fireball not created, max %d triangles per solver supported.\n",PackedFactor::MAX_TRIANGLES);
		return NULL;
	}


	/////////////////////////////////////////////////////////////////////////
	//
	// update factors (tri-tri GI factors, sky-tri direct factors)

	// allocate space for sky-tri factors
	skyPatchHitsForAllTriangles = new PackedSkyTriangleFactor::UnpackedFactor[object->triangles];

	RRReal sceneArea = 0;

	if (raysFromTriangle)
	{
		abortStaticImprovement();
		for (unsigned t=0;t<object->triangles;t++)
		{
			if (object->triangle[t].surface)
				sceneArea += object->triangle[t].area;
		}
		if (sceneArea)
		{
			// update factors - all triangles
			for (unsigned t=0;t<object->triangles;t++)
			{
				if (object->triangle[t].surface)
				{
					// enable update of sky-tri factors
					skyPatchHitsForCurrentTriangle = skyPatchHitsForAllTriangles+t;

					// update factors - one triangle
					NeverEnd neverEnd;
					unsigned shotsForFactors = RR_MAX(1,(unsigned)(raysFromTriangle*object->triangles*object->triangle[t].area/sceneArea));
					refreshFormFactorsFromUntil(&object->triangle[t],shotsForFactors,neverEnd);
				}
			}

			// stop updating sky-tri factors
			skyPatchHitsForCurrentTriangle = NULL;
		}
	}


	/////////////////////////////////////////////////////////////////////////
	//
	// pack tri-tri GI factors

	PackedSolverFile* packedSolverFile = new PackedSolverFile;

	// calculate size
	unsigned numFactors = 0;
	for (unsigned i=0;i<object->triangles;i++)
	{
		//!!! optimize, sort factors
		numFactors += object->triangle[i].factors.size();
	}


	// alloc thread
	packedSolverFile->packedFactors = new PackedFactorsThread(object->triangles,numFactors);
	// fill thread
	for (unsigned i=0;i<object->triangles;i++)
	{
		// write factors
		packedSolverFile->packedFactors->newC1(i);
		for (ChunkList<Factor>::const_iterator j=object->triangle[i].factors.begin(); *j; ++j)
		{
			unsigned destinationTriangle = (unsigned)( (*j)->destination-object->triangle );
			RR_ASSERT(destinationTriangle<object->triangles);
			packedSolverFile->packedFactors->newC2()->set((*j)->power,destinationTriangle);
		}
	}
	packedSolverFile->packedFactors->newC1(object->triangles);



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
	for (unsigned t=0;t<object->triangles;t++)
		for (unsigned v=0;v<3;v++)
		{
			IVertex* ivertex = object->triangle[t].topivertex[v];
			if (ivertex) ivertex->packedIndex = UINT_MAX;
		}

	// 2 pruchod pres triangly:
	//     do ivertexu zapise jeho poradove cislo
	//     ziska pocet ivertexu a corneru
	unsigned numIvertices = 0;
	unsigned numWeights = 0;
	for (unsigned t=0;t<object->triangles;t++)
		for (unsigned v=0;v<3;v++)
		{
			IVertex* ivertex = object->triangle[t].topivertex[v];
			if (ivertex && ivertex->packedIndex==UINT_MAX)
			{
				unsigned oldNumWeights = numWeights;
				for (unsigned k=0;k<ivertex->corners;k++)
					numWeights++;
				if (numWeights>oldNumWeights)
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
	for (unsigned t=0;t<object->triangles;t++)
		for (unsigned v=0;v<3;v++)
		{
			const IVertex* ivertex = object->triangle[t].topivertex[v];
			/* toto nema zadny efekt, z nejakeho duvodu pokud chybi 1 ivertex, chybi vsechny 3
			if (!ivertex)
			{
				// pokud chybi ivertex, zkus pouzit nejblizsi vertex tehoz trianglu
				// nepomuze to vsude (shluk vice jehel), ale tam kde ted potrebuji ano (ojedinele jehly)
				bool v1Closer =
					(*object->triangle[t].getVertex((v+1)%3)-*object->triangle[t].getVertex(v)).length2() <
					(*object->triangle[t].getVertex((v+2)%3)-*object->triangle[t].getVertex(v)).length2();
				ivertex = object->triangle[t].topivertex[(v+(v1Closer?1:2))%3];
				RRReporter::report(INF1,"t=%d v=%d->%d%s\n",t,v,v1Closer?v+1:v+2,ivertex?"":" !");
			}*/
			if (ivertex)
			{
				if (ivertex->packedIndex==numIverticesPacked)
				{
					packedSolverFile->packedIvertices->newC1(numIverticesPacked++);
					for (unsigned k=0;k<ivertex->corners;k++)
					{
						PackedSmoothTriangleWeight* triangleWeight = packedSolverFile->packedIvertices->newC2();
						triangleWeight->triangleIndex = (unsigned)(ivertex->getCorner(k).node-object->triangle);
						triangleWeight->weight = ivertex->getCorner(k).power / ivertex->powerTopLevel / ivertex->getCorner(k).node->area;
					}
				}
				RR_ASSERT(ivertex->packedIndex<numIvertices);
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = ivertex->packedIndex;
			}
			else
			{
				// ivertex missing, we have no lighting data
				// this is legal: needles and degenerates are catched here, both with surface=NULL
				// we must be ready for them even in runtime
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = UINT_MAX; // invalid value, will be displayed as pink
			}
		}
	packedSolverFile->packedIvertices->newC1(numIverticesPacked);
	RR_ASSERT(numIverticesPacked==numIvertices);


	/////////////////////////////////////////////////////////////////////////
	//
	// convert sky-tri direct factors to GI factors

	RRVec3* directIrradiancePhysicalRGB = new RRVec3[object->triangles];
	for (unsigned p=0;p<PackedSkyTriangleFactor::NUM_PATCHES;p++)
	{
		// reset solver to direct from sky patch
		for (unsigned t=0;t<object->triangles;t++)
		{
			unsigned shotsForFactors = RR_MAX(1,(unsigned)(raysFromTriangle*object->triangles*object->triangle[t].area/sceneArea));
			directIrradiancePhysicalRGB[t] = RRVec3(skyPatchHitsForAllTriangles[t].patches[p][0]/shotsForFactors);
		}
		resetStaticIllumination(false,true,NULL,NULL,directIrradiancePhysicalRGB);

		// calculate
		//!!! disable emissive surfaces
		distribute(0.001f);

		// convert direct from sky patch to GI from sky patch
		for (unsigned t=0;t<object->triangles;t++)
		{
			skyPatchHitsForAllTriangles[t].patches[p] = object->triangle[t].getTotalIrradiance();
		}
	}
	RR_SAFE_DELETE_ARRAY(directIrradiancePhysicalRGB);


	/////////////////////////////////////////////////////////////////////////
	//
	// pack sky-tri GI factors

#pragma omp parallel for
	for (int t=0;(unsigned)t<object->triangles;t++)
	{
		PackedFactorHeader* pfh = packedSolverFile->packedFactors->getC1((unsigned)t);
		pfh->packedSkyTriangleFactor.setSkyTriangleFactor(skyPatchHitsForAllTriangles[t],packedSolverFile->intensityTable);
	}
	RR_SAFE_DELETE_ARRAY(skyPatchHitsForAllTriangles);


	// return
	return packedSolverFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRStaticSolver

const PackedSolverFile* RRStaticSolver::buildFireball(unsigned raysPerTriangle)
{
	const PackedSolverFile* packedSolverFile = scene->packSolver(raysPerTriangle);
	return packedSolverFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolver

void getFireballFilename(const RRObject* object,char filename[1000])
{
	getFileName(filename,999,FIREBALL_FILENAME_VERSION,object ? object->getCollider()->getMesh() : NULL,NULL,".fireball");
}

bool RRDynamicSolver::buildFireball(unsigned raysPerTriangle, const char* filename)
{
	RRReportInterval report(INF1,"Building Fireball (triangles=%d)...\n",getMultiObjectCustom()?getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles():0);
	RR_SAFE_DELETE(priv->packedSolver); // delete packed solver if it already exists (we REbuild it)
	priv->preVertex2Ivertex.clear(); // clear also table that depends on packed solver
	calculateCore(0); // create static solver if not created yet
	if (!priv->scene)
	{
		RRReporter::report(WARN,"Fireball not built, empty scene.\n");
		return false;
	}
	const PackedSolverFile* packedSolverFile = priv->scene->buildFireball(raysPerTriangle);
	if (!packedSolverFile)
		return false;
	RRReporter::report(INF2,"Size: %d kB (factors=%d smoothing=%d)\n",
		( packedSolverFile->getMemoryOccupied() )/1024,
		( packedSolverFile->packedFactors->getMemoryOccupied() )/1024,
		( packedSolverFile->packedIvertices->getMemoryOccupied()+packedSolverFile->packedSmoothTrianglesBytes )/1024
		);

	char filenameauto[1000];
	if (!filename)
	{
		getFireballFilename(getMultiObjectCustom(),filenameauto);
		filename = filenameauto;
	}

	if (filename[0])
	{
		if (packedSolverFile->save(filename))
			RRReporter::report(INF2,"Saved to %s\n",filename);
		else
		{
			RRReporter::report(WARN,"Failed to save to %s\n",filename);
			RR_SAFE_DELETE(packedSolverFile); // to make it consistent, delete fireball when save fails
		}
	}
	priv->packedSolver = RRPackedSolver::create(getMultiObjectPhysical(),packedSolverFile);
	if (priv->packedSolver)
	{
		priv->packedSolver->setEnvironment(getEnvironment(),getScaler());
		updateVertexLookupTablePackedSolver();
		priv->dirtyMaterials = false; // packed solver defines materials & factors, they are safe now
		priv->dirtyCustomIrradiance = true; // request reload of direct illumination into solver
		RR_SAFE_DELETE(priv->scene);
	}
	return priv->packedSolver!=NULL;
}

bool RRDynamicSolver::loadFireball(const char* filename)
{
	RR_SAFE_DELETE(priv->packedSolver); // delete packed solver if it already exists (we REload it)
	priv->preVertex2Ivertex.clear(); // clear also table that depends on packed solver

	char filenameauto[1000];
	if (!filename)
	{
		getFireballFilename(getMultiObjectCustom(),filenameauto);
		filename = filenameauto;
	}

	priv->packedSolver = RRPackedSolver::create(getMultiObjectPhysical(),PackedSolverFile::load(filename));
	if (priv->packedSolver)
	{
		//RRReporter::report(INF2,"Loaded Fireball (triangles=%d)\n",getMultiObjectCustom()?getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles():0);
		priv->packedSolver->setEnvironment(getEnvironment(),getScaler());
		updateVertexLookupTablePackedSolver();
		priv->dirtyMaterials = false; // packed solver defines materials & factors, they are safe now
		priv->dirtyCustomIrradiance = true; // request reload of direct illumination into solver
		RR_SAFE_DELETE(priv->scene);
	}
	return priv->packedSolver!=NULL;
}

void RRDynamicSolver::leaveFireball()
{
	RR_SAFE_DELETE(priv->packedSolver);
	priv->preVertex2Ivertex.clear(); // clear also table that depends on packed solver
}

} // namespace

