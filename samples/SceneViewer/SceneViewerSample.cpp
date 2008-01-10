// --------------------------------------------------------------------------
// SceneViewer sample
//
// Quick simple scene viewer with
// - realtime GI: all lights freely movable
// - offline GI: build/save/load lightmaps
//
// This is implemented as
// - load Collada DAE scene
// - call sceneViewer() function
//
// Use commandline argument or drag&drop to open custom collada scene.
//
// Controls:
// right mouse button ... menu
// arrows or wsadqz   ... movement
// + -                ... brightness adjustment
//
// If it doesn't render your scene properly, please send us sample so we can fix it.
//
// Copyright (C) Lightsprint, Stepan Hrbek, 2007-2008
// --------------------------------------------------------------------------

#include "FCollada.h" // must be included before LightsprintGL because of fcollada SAFE_DELETE macro
#include "FCDocument/FCDocument.h"
#include "../../samples/ImportCollada/RRObjectCollada.h"

#include <cstdlib>
#include "Lightsprint/RRDynamicSolver.h"
#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/Texture.h"
#include "Lightsprint/GL/Program.h"

void error(const char* message, bool gfxRelated)
{
	printf(message);
	if(gfxRelated)
		printf("\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5xxx, 6xxx, 7xxx, 8xxx (including GeForce Go)\n - Radeon 9500-9800, Xxxx, X1xxx, HD2xxx, HD3xxx (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char **argv)
{
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 57861;

	// check for version mismatch
	if(!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("",false);
	}
	// log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
	//rr::RRReporter::setFilter(true,3,true);
	
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for(unsigned i=strlen(exedir);--i;) if(exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	SetCurrentDirectoryA(exedir);
	free(exedir);

	// init solver
	if(rr::RRLicense::loadLicense("../../data/licence_number")!=rr::RRLicense::VALID)
		error("Problem with licence number.\n", false);

	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace

	// init static objects
	FCDocument* collada = NULL;
	rr::RRObjects* adaptedObjects = NULL;
	{
		collada = FCollada::NewTopDocument();
		FUErrorSimpleHandler errorHandler;
		collada->LoadFromFile((argc>1)?argv[1]:"../../data/scenes/koupelna/koupelna4.dae");
		if(!errorHandler.IsSuccessful())
		{
			puts(errorHandler.GetErrorString());
			error("",false);
		}
		adaptedObjects = adaptObjectsFromFCollada(collada);
		solver->setStaticObjects(*adaptedObjects,NULL);
	}

	// init environment
	const char* cubeSideNames[6] = {"bk","ft","up","dn","rt","lf"};
	solver->setEnvironment(rr::RRBuffer::load("../../data/maps/skybox/skybox_%s.jpg",cubeSideNames,true,true));

	// init lights
	rr::RRLights* adaptedLights = adaptLightsFromFCollada(collada);
	solver->setLights(*adaptedLights);

	rr_gl::Program::logMessages(1);
	// run interactive scene viewer
	rr_gl::sceneViewer(solver,true,"../../data/shaders/",false);

	rr_gl::deleteAllTextures();
	delete solver->getEnvironment();
	delete solver->getScaler();
	delete solver;
	delete adaptedLights;
	delete adaptedObjects;
	delete collada;
	delete rr::RRReporter::getReporter();
	rr::RRReporter::setReporter(NULL);

	return 0;
}
