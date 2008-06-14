// --------------------------------------------------------------------------
// CPU Lightmaps sample
//
// This sample computes lightmaps with global illumination on CPU.
// GPU is not required.
//
// It loads Collada 1.4 .DAE scene, calculates lightmaps/vertxcolors
// and saves them to disk.
//
// Lightsources considered are
// - point, spot and directional lights in scene
//   (see supported light features in RRObjectCollada.cpp)
// - skybox loaded from textures, separately from scene
//
// Remarks:
// - map quality depends on unwrap quality,
//   make sure you have good unwrap in your scenes
//   (save it as second TEXCOORD in Collada document, see RRObjectCollada.cpp)
// - idle time of all CPUs/cores is used, so other applications don't suffer
//   from lightmap precalculator running on the background
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets per-pixel lightmap, others get per-vertex

#include <cassert>
#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>  // SetPriorityClass
#endif
#include <cstdlib>
#include <stdio.h>    // printf

#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/IO/ImportScene.h"

/////////////////////////////////////////////////////////////////////////////
//
// termination with error message

void error(const char* message, bool gfxRelated)
{
	printf(message);
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

void calculate(rr::RRDynamicSolver* solver)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		const rr::RRMesh* mesh = solver->getObject(i)->getCollider()->getMesh();
		if(i==SELECTED_OBJECT_NUMBER)
		{
			// allocate lightmaps for selected object
			unsigned res = 16;
			unsigned sizeFactor = 5; // 5 is ok for scenes with unwrap (20 is ok for scenes without unwrap)
			while(res<2048 && (float)res<sizeFactor*sqrtf((float)(mesh->getNumTriangles()))) res*=2;
			for(unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getIllumination(i)->getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
		}
		else
		{
			// allocate vertex buffers for other objects
			for(unsigned layerNumber=0;layerNumber<5;layerNumber++)
				solver->getIllumination(i)->getLayer(layerNumber) =
					rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// calculate lightmaps(layer0), directional lightmaps(layer1,2,3), bent normals(layer4)
	rr::RRDynamicSolver::UpdateParameters params(1000);
	solver->updateLightmaps(0,1,4,&params,&params,NULL);

	// save GI lightmaps, bent normals
	for(unsigned layerNumber=0;layerNumber<5;layerNumber++)
		solver->getStaticObjects().saveIllumination("../../data/export/",layerNumber);
}

int main(int argc, char **argv)
{
#ifdef _MSVC
	// this sample properly frees memory, no leaks are reported
	// (some other samples are stripped down, they don't free memory)
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 30351;
#endif // _MSC_VER

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	rr::RRReporter::setReporter(reporter);
	//rr::RRReporter::setFilter(true,3,true);

	rr_io::setImageLoader();

#ifdef _MSC_VER
	// decrease priority, so that this task runs on background using only free CPU cycles
	// good for precalculating lightmaps on workstation
	SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#endif // _MSC_VER

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	rr::RRScaler* scaler = rr::RRScaler::createRgbScaler();
	solver->setScaler(scaler);

	// load scene
	rr_io::ImportScene scene("../../data/scenes/koupelna/koupelna4-windows.dae");
	solver->setStaticObjects(*scene.getObjects(), NULL);
	solver->setLights(*scene.getLights());

	// calculate and save results
	calculate(solver);

	// release memory
	delete solver;
	delete scaler;
	rr::RRReporter::setReporter(NULL);
	delete reporter;

	printf("\nPress any key to close...");
	fgetc(stdin);
	return 0;
}
