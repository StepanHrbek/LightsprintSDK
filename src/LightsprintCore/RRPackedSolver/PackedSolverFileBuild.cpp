// Build PackedSolverFile, to be used by tools, not by runtime.

#include "PackedSolverFile.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRDynamicSolver.h"

#pragma warning(disable:4530) // unrelated std::vector in private.h complains about exceptions
#include "../RRDynamicSolver/private.h"

namespace rr
{


//////////////////////////////////////////////////////////////////////////////
//
// Scene

bool neverend(void *)
{
	return false;
}

void Scene::updateFactors(unsigned raysFromTriangle)
{
	RRReportInterval report(INF2,"Updating factors...\n");

	/////////////////////////////////////////////////////////////////////////
	//
	// update factors

	if(raysFromTriangle)
	{
		RR_ASSERT(object->subdivisionSpeed==0);
		abortStaticImprovement();
		RRReal sceneArea = 0;
		for(unsigned t=0;t<object->triangles;t++)
		{
			if(object->triangle[t].surface)
				sceneArea += object->triangle[t].area;
		}
		if(sceneArea)
		{
			for(unsigned t=0;t<object->triangles;t++)
			{
				if(object->triangle[t].surface)
					refreshFormFactorsFromUntil(&object->triangle[t],MAX(1,(unsigned)(raysFromTriangle*object->triangles*object->triangle[t].area/sceneArea)),neverend,NULL);
			}
		}
	}
}

PackedSolverFile* Scene::packSolver() const
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


	// alloc thread
	packedSolverFile->packedFactors = new PackedFactorsThread(object->triangles,numFactors);
	// fill thread
	for(unsigned i=0;i<object->triangles;i++)
	{
		// write factors
		packedSolverFile->packedFactors->newC1(i);
		for(unsigned j=0;j<object->triangle[i].shooter->factors();j++)
		{
			const Factor* factor = object->triangle[i].shooter->get(j);
			RR_ASSERT(IS_TRIANGLE(factor->destination));
			unsigned destinationTriangle = (unsigned)( TRIANGLE(factor->destination)-object->triangle );
			RR_ASSERT(destinationTriangle<object->triangles);
			packedSolverFile->packedFactors->newC2()->set(factor->power,destinationTriangle);
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
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = UINT_MAX; // invalid value, will be displayed as pink
			}
		}
	packedSolverFile->packedIvertices->newC1(numIverticesPacked);
	RR_ASSERT(numIverticesPacked==numIvertices);

	// return
	RRReporter::report(INF2,"Size: factors=%d smoothing=%d total=%d kB.\n",
		( packedSolverFile->packedFactors->getMemoryOccupied() )/1024,
		( packedSolverFile->packedIvertices->getMemoryOccupied()+packedSolverFile->packedSmoothTrianglesBytes )/1024,
		( packedSolverFile->getMemoryOccupied() )/1024
		);
	// save&load test
	//packedSolverFile->save("h:\\ab");
	//packedSolverFile = PackedSolverFile::load("h:\\ab");
	return packedSolverFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRStaticSolver

bool RRStaticSolver::buildPackedSolver(unsigned raysPerTriangle, const char* filename)
{
	scene->updateFactors(raysPerTriangle);
	PackedSolverFile* packedSolverFile = scene->packSolver();
	bool result = packedSolverFile->save(filename);
	delete packedSolverFile;
	return result;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolver

bool RRDynamicSolver::buildPackedSolver(unsigned raysPerTriangle, const char* filename)
{
	return getStaticSolver() && priv->scene && priv->scene->buildPackedSolver(raysPerTriangle,filename);
}

bool RRDynamicSolver::switchToPackedSolver(const char* filename)
{
	SAFE_DELETE(priv->packedSolver);
	if(filename)
	{
		priv->packedSolver = RRPackedSolver::create(getMultiObjectPhysicalWithIllumination(),PackedSolverFile::load(filename));
		if(priv->packedSolver)
			updateVertexLookupTablePackedSolver();
	}
	return priv->packedSolver!=NULL;
}

} // namespace
