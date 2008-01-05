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

#include "FCollada.h"
#include "FCDocument/FCDocument.h"
#include "../ImportCollada/RRObjectCollada.h"

#include <cassert>
#include <cstdlib>


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

void calculate(rr::RRDynamicSolver* solver, int layerNumberLighting, int layerNumberBentNormals)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		rr::RRMesh* mesh = solver->getObject(i)->getCollider()->getMesh();
		if(i==SELECTED_OBJECT_NUMBER)
		{
			// allocate lightmaps for selected object
			unsigned res = 16;
			unsigned sizeFactor = 5; // 5 is ok for scenes with unwrap (20 is ok for scenes without unwrap)
			while(res<2048 && (float)res<sizeFactor*sqrtf((float)(mesh->getNumTriangles()))) res*=2;
			solver->getIllumination(i)->getLayer(layerNumberLighting) =
				rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,true,NULL);
			solver->getIllumination(i)->getLayer(layerNumberBentNormals) =
				rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGB,false,NULL);
		}
		else
		{
			// allocate vertex buffers for other objects
			solver->getIllumination(i)->getLayer(layerNumberLighting) =
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
			solver->getIllumination(i)->getLayer(layerNumberBentNormals) =
				rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,mesh->getNumVertices(),1,1,rr::BF_RGBF,false,NULL);
		}
	}

	// calculate lightmaps and bent normals
	rr::RRDynamicSolver::UpdateParameters params;
	params.quality = 1000;
	params.applyCurrentSolution = false;
	params.applyEnvironment = true;
	params.applyLights = true;
	solver->updateLightmaps(layerNumberLighting,layerNumberBentNormals,&params,&params,NULL); 
}

int main(int argc, char **argv)
{
	// this sample properly frees memory, no leaks are reported
	// (some other samples are stripped down, they don't free memory)
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
//	_crtBreakAlloc = 39137;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	rr::RRReporter::setReporter(reporter);
	rr::RRReporter::setFilter(true,3,true);

	// decrease priority, so that this task runs on background using only free CPU cycles
	// good for precalculating lightmaps on workstation
	SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);

	// init scene and solver
	if(rr::RRLicense::loadLicense("..\\..\\data\\licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	// switch inputs and outputs from HDR physical scale to RGB screenspace
	rr::RRScaler* scaler = rr::RRScaler::createRgbScaler();
	solver->setScaler(scaler);

	FCDocument* collada = FCollada::NewTopDocument();
	FUErrorSimpleHandler errorHandler;
	collada->LoadFromFile("../../data/scenes/koupelna/koupelna4-windows.dae");
	if(!errorHandler.IsSuccessful())
	{
		puts(errorHandler.GetErrorString());
		error("",false);
	}
	rr::RRObjects* objects = adaptObjectsFromFCollada( collada );
	solver->setStaticObjects( *objects, NULL );
	rr::RRLights* lights = adaptLightsFromFCollada( collada );
	solver->setLights( *lights );
	if(!solver->getMultiObjectCustom())
		error("No objects in scene.",false);

	{
		rr::RRReportInterval report(rr::INF1,"Calculating all ...\n");

		// calculate and save it
		calculate(solver,0,1);
	}

	// save GI lightmaps, bent normals
	solver->getStaticObjects().saveIllumination("../../data/export/",0);
	solver->getStaticObjects().saveIllumination("../../data/export/",1);

	// release memory
	delete solver;
	delete lights;
	delete objects;
	delete collada;
	delete scaler;
	rr::RRReporter::setReporter(NULL);
	delete reporter;

	printf("\nPress any key to close...");
	fgetc(stdin);
	return 0;
}
