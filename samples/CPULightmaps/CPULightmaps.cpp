// --------------------------------------------------------------------------
// CPU Lightmaps sample
//
// This is bare bone BuildLightmaps, it builds lightmaps in koupelna scene
// and saves them to disk, no GUI, everything is as simple as possible.
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets per-pixel lightmap, others get per-vertex

#include <cstdlib>
#include <stdio.h>    // printf
#ifdef _MSC_VER
	#include <crtdbg.h>
#endif

#include "Lightsprint/RRSolver.h"
#include "Lightsprint/IO/IO.h"

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char** argv)
{
#ifdef _MSC_VER
	// check that we don't leak memory
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 30351;
#endif

	// check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	//rr::RRReporter::setFilter(true,3,true);

	rr_io::registerLoaders(argc,argv);

	// init scene and solver
	rr::RRSolver* solver = new rr::RRSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	rr::RRColorSpace* colorSpace = rr::RRColorSpace::create_sRGB();
	solver->setColorSpace(colorSpace);

	// load scene
	rr::RRScene scene("../../data/scenes/koupelna/koupelna4-windows.dae");
	solver->setStaticObjects(scene.objects, nullptr);
	solver->setLights(scene.lights);

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
	{
		const rr::RRMesh* mesh = solver->getStaticObjects()[i]->getCollider()->getMesh();
		if (i==SELECTED_OBJECT_NUMBER)
		{
			// allocate lightmaps for selected object
			// we guess reasonable lightmap size here, you may need to tweak this
			unsigned res = 16;
			while (res<2048 && (float)res<5*sqrtf((float)(mesh->getNumTriangles()))) res*=2;
			for (unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getStaticObjects()[i]->illumination.getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,nullptr);
		}
		else
		{
			// allocate vertex buffers for other objects
			for (unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getStaticObjects()[i]->illumination.getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,nullptr);
		}
	}

	// calculate lightmaps(layer0), directional lightmaps(layer1,2,3), bent normals(layer4)
	rr::RRSolver::UpdateParameters params(1000);
	params.aoIntensity = 1;
	params.aoSize = 1;
	solver->updateLightmaps(0,1,4,&params,nullptr);

	// save GI lightmaps, bent normals
	solver->getStaticObjects().saveLayer(0,"../../data/scenes/koupelna/koupelna4-windows_precalculated/","png");
	solver->getStaticObjects().saveLayer(1,"../../data/scenes/koupelna/koupelna4-windows_precalculated/","directional1.png");
	solver->getStaticObjects().saveLayer(2,"../../data/scenes/koupelna/koupelna4-windows_precalculated/","directional2.png");
	solver->getStaticObjects().saveLayer(3,"../../data/scenes/koupelna/koupelna4-windows_precalculated/","directional3.png");
	solver->getStaticObjects().saveLayer(4,"../../data/scenes/koupelna/koupelna4-windows_precalculated/","bentnormals.png");

	// release memory
	delete solver;
	delete colorSpace;
	delete reporter;

	printf("\nPress any key to close...");
	fgetc(stdin);
	return 0;
}
