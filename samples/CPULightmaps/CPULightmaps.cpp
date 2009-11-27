// --------------------------------------------------------------------------
// CPU Lightmaps sample
//
// This is bare bone BuildLightmaps, it builds lightmaps in koupelna scene
// and saves them to disk, no GUI, everything is as simple as possible.
//
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets per-pixel lightmap, others get per-vertex

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>  // SetPriorityClass
#endif
#include <cstdlib>
#include <stdio.h>    // printf

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/IO/ImportScene.h"

void error(const char* message, bool gfxRelated)
{
	printf("%s",message);
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char **argv)
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
	rr::RRReporter::setReporter(reporter);
	//rr::RRReporter::setFilter(true,3,true);

	rr_io::registerLoaders();

	// init scene and solver
	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	rr::RRScaler* scaler = rr::RRScaler::createRgbScaler();
	solver->setScaler(scaler);

	// load scene
	rr::RRScene scene("../../data/scenes/koupelna/koupelna4-windows.dae");
	solver->setStaticObjects(scene.getObjects(), NULL);
	solver->setLights(scene.getLights());

	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for (unsigned i=0;i<solver->getStaticObjects().size();i++)
	{
		const rr::RRMesh* mesh = solver->getStaticObjects()[i]->getCollider()->getMesh();
		if (i==SELECTED_OBJECT_NUMBER)
		{
			// allocate lightmaps for selected object
			unsigned res = 16;
			unsigned sizeFactor = 5; // 5 is ok for scenes with unwrap (20 is ok for scenes without unwrap)
			while (res<2048 && (float)res<sizeFactor*sqrtf((float)(mesh->getNumTriangles()))) res*=2;
			for (unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getStaticObjects()[i]->illumination->getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
		}
		else
		{
			// allocate vertex buffers for other objects
			for (unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getStaticObjects()[i]->illumination->getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// calculate lightmaps(layer0), directional lightmaps(layer1,2,3), bent normals(layer4)
	rr::RRDynamicSolver::UpdateParameters params(1000);
	solver->updateLightmaps(0,1,4,&params,&params,NULL);

	// save GI lightmaps, bent normals
	for (unsigned layerNumber=0;layerNumber<5;layerNumber++)
		solver->getStaticObjects().saveLayer(layerNumber,"../../data/export/","png");

	// release memory
	delete solver;
	delete scaler;
	rr::RRReporter::setReporter(NULL);
	delete reporter;

	printf("\nPress any key to close...");
	fgetc(stdin);
	return 0;
}
