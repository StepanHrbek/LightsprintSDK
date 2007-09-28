// Build PackedSolverFile, to be used by tools, not by runtime.

#include "PackedSolverFile.h"
#include "../RRStaticSolver/rrcore.h"
#include "Lightsprint/RRDynamicSolver.h"

#pragma warning(disable:4530) // unrelated std::vector in private.h complains about exceptions
#include "../RRCollider/cache.h"

#include "../RRDynamicSolver/private.h"
#include <windows.h> // jen kvuli GetTempPath

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
				// this is legal: needles and degenerates are catched here, both with surface=NULL
				// we must be ready for them even in runtime
				packedSolverFile->packedSmoothTriangles[t].ivertexIndex[v] = UINT_MAX; // invalid value, will be displayed as pink
			}
		}
	packedSolverFile->packedIvertices->newC1(numIverticesPacked);
	RR_ASSERT(numIverticesPacked==numIvertices);

	// return
	return packedSolverFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRStaticSolver

const PackedSolverFile* RRStaticSolver::buildFireball(unsigned raysPerTriangle)
{
	scene->updateFactors(raysPerTriangle);
	const PackedSolverFile* packedSolverFile = scene->packSolver();
	return packedSolverFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// RRDynamicSolver

bool RRDynamicSolver::buildFireball(unsigned raysPerTriangle, const char* filename)
{
	RRReportInterval report(INF1,"Building Fireball (triangles=%d)...\n",getMultiObjectCustom()?getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles():0);
	calculateCore(0); // create static solver if not created yet
	const PackedSolverFile* packedSolverFile = priv->scene->buildFireball(raysPerTriangle);
	RRReporter::report(INF2,"Size: %d kB (factors=%d smoothing=%d)\n",
		( packedSolverFile->getMemoryOccupied() )/1024,
		( packedSolverFile->packedFactors->getMemoryOccupied() )/1024,
		( packedSolverFile->packedIvertices->getMemoryOccupied()+packedSolverFile->packedSmoothTrianglesBytes )/1024
		);
	if(filename)
	{
		if(packedSolverFile->save(filename))
			RRReporter::report(INF2,"Saved to %s\n",filename);
		else
		{
			RRReporter::report(WARN,"Failed to save to %s\n",filename);
			SAFE_DELETE(packedSolverFile); // to make it consistent, delete fireball when save fails
		}
	}
	priv->packedSolver = RRPackedSolver::create(getMultiObjectPhysicalWithIllumination(),packedSolverFile);
	if(priv->packedSolver)
	{
		updateVertexLookupTablePackedSolver();
		priv->dirtyMaterials = false; // packed solver defines materials & factors, they are safe now
		SAFE_DELETE(priv->scene);
	}
	return priv->packedSolver!=NULL;
}

bool RRDynamicSolver::loadFireball(const char* filename)
{
	SAFE_DELETE(priv->packedSolver);
	if(filename)
	{
		priv->packedSolver = RRPackedSolver::create(getMultiObjectPhysicalWithIllumination(),PackedSolverFile::load(filename));
		if(priv->packedSolver)
		{
			//RRReporter::report(INF2,"Loaded Fireball (triangles=%d)\n",getMultiObjectCustom()?getMultiObjectCustom()->getCollider()->getMesh()->getNumTriangles():0);
			updateVertexLookupTablePackedSolver();
			priv->dirtyMaterials = false; // packed solver defines materials & factors, they are safe now
			if(priv->scene)
				RRReporter::report(WARN,"Fireball set, but solver already exists. Save resources, don't call calculate() before loadFireball().\n");
		}
	}
	return priv->packedSolver!=NULL;
}

bool RRDynamicSolver::startFireball(unsigned raysPerTriangle)
{
	char filename[1000];

	char tmpPath[_MAX_PATH+1];
	GetTempPath(_MAX_PATH, tmpPath);
	#define IS_PATHSEP(x) (((x) == '\\') || ((x) == '/'))
	if(!IS_PATHSEP(tmpPath[strlen(tmpPath)-1])) strcat(tmpPath, "\\");

	getFileName(filename,999,FIREBALL_FILENAME_VERSION,getMultiObjectCustom()->getCollider()->getMesh(),tmpPath,".fireball");
	return loadFireball(filename) || buildFireball(raysPerTriangle,filename);
}

} // namespace
