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
// Copyright (C) Lightsprint, Stepan Hrbek, 2007
// --------------------------------------------------------------------------

#define SELECTED_OBJECT_NUMBER 0 // selected object gets lightmap, others get per-vertex

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

void calculatePerVertexAndSelectedPerPixel(rr::RRDynamicSolver* solver, int layerNumberLighting, int layerNumberBentNormals)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		solver->getIllumination(i)->getLayer(layerNumberLighting) =
			rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,NULL);
		solver->getIllumination(i)->getLayer(layerNumberBentNormals) =
			rr::RRBuffer::create(rr::BT_VERTEX_BUFFER,solver->getObject(i)->getCollider()->getMesh()->getNumVertices(),1,1,rr::BF_RGBF,NULL);
	}

	// calculate per vertex - all objects
	// it is faster and quality is good for some objects
	rr::RRDynamicSolver::UpdateParameters paramsDirect;
	paramsDirect.measure = RM_IRRADIANCE_PHYSICAL; // get vertex colors in HDR
	paramsDirect.quality = 40;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = true;
	paramsDirect.applyLights = true;
	rr::RRDynamicSolver::UpdateParameters paramsIndirect;
	paramsIndirect.measure = RM_IRRADIANCE_PHYSICAL; // get vertex colors in HDR
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = true;
	paramsIndirect.applyLights = true;
	solver->updateLightmaps(layerNumberLighting,layerNumberBentNormals,&paramsDirect,&paramsIndirect,NULL); 

	// calculate per pixel - selected objects
	// it is slower, but some objects need it
	rr::RRDynamicSolver::UpdateParameters paramsDirectPixel;
	paramsDirectPixel.measure = RM_IRRADIANCE_CUSTOM; // get maps in sRGB
	paramsDirectPixel.quality = 50;
	paramsDirectPixel.applyEnvironment = true;
	paramsDirectPixel.applyLights = true;
	unsigned objectNumbers[] = {SELECTED_OBJECT_NUMBER};
	for(unsigned i=0;i<sizeof(objectNumbers)/sizeof(objectNumbers[0]);i++)
	{
		unsigned objectNumber = objectNumbers[i];
		if(solver->getObject(objectNumber))
		{
			rr::RRBuffer* lightmap = (layerNumberLighting<0) ? NULL :
				(solver->getIllumination(objectNumber)->getLayer(layerNumberLighting) = rr::RRBuffer::create(rr::BT_2D_TEXTURE,256,256,1,rr::BF_RGB,NULL));
			rr::RRBuffer* bentNormals = (layerNumberBentNormals<0) ? NULL :
				(solver->getIllumination(objectNumber)->getLayer(layerNumberBentNormals) = rr::RRBuffer::create(rr::BT_2D_TEXTURE,256,256,1,rr::BF_RGB,NULL));
			solver->updateLightmap(objectNumber,lightmap,bentNormals,&paramsDirectPixel);
		}
	}
}

void calculatePerPixel(rr::RRDynamicSolver* solver, int layerNumberLighting, int layerNumberBentNormals)
{
	// create buffers for computed GI
	// (select types, formats, resolutions, don't create buffers for objects that don't need GI)
	for(unsigned i=0;i<solver->getNumObjects();i++)
	{
		unsigned res = 16;
		unsigned sizeFactor = 5; // higher factor = higher map resolution
		while(res<2048 && res<sizeFactor*sqrtf((float)solver->getObject(i)->getCollider()->getMesh()->getNumTriangles())) res*=2;
		solver->getIllumination(i)->getLayer(layerNumberLighting) =
			rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBF,NULL);
		solver->getIllumination(i)->getLayer(layerNumberBentNormals) =
			rr::RRBuffer::create(rr::BT_2D_TEXTURE,res,res,1,rr::BF_RGBF,NULL);
	}

	// calculate per pixel - all objects
	rr::RRDynamicSolver::UpdateParameters paramsDirect;
	paramsDirect.measure = RM_IRRADIANCE_CUSTOM; // get maps in sRGB
	paramsDirect.quality = 1000;
	paramsDirect.applyCurrentSolution = false;
	paramsDirect.applyEnvironment = true;
	paramsDirect.applyLights = true;
	rr::RRDynamicSolver::UpdateParameters paramsIndirect;
	paramsIndirect.applyCurrentSolution = false;
	paramsIndirect.applyEnvironment = true;
	paramsIndirect.applyLights = true;
	solver->updateLightmaps(layerNumberLighting,layerNumberBentNormals,&paramsDirect,&paramsIndirect,NULL); 
}

int main(int argc, char **argv)
{
	// this sample properly frees memory, no leaks are reported
	// (some other samples are stripped down, they don't free memory)
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
//	_crtBreakAlloc = 65549;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	rr::RRReporter::setReporter(reporter);

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
		calculatePerVertexAndSelectedPerPixel(solver,0,1); // calculatePerPixel(solver,0,1);
	}

	solver->saveIllumination("../../data/export/",0); // save GI lightmaps
	solver->saveIllumination("../../data/export/",1); // save bent normals

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
