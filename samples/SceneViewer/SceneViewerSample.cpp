// --------------------------------------------------------------------------
// SceneViewer sample
//
// Quick simple scene viewer with
// - realtime GI: insert/delete/move/edit lights, skybox
// - offline GI: build/save/load lightmaps
//
// This is implemented in one step
// 1. call sceneViewer() function
//
// Use commandline argument or drag&drop or File/Open... to open custom scene.
//
// Controls:
// arrows or wsadqz   ... movement
// left mouse button  ... look
// middle mouse button... pan
// right mouse button ... inspect (rotate around)
// wheel              ... zoom
// F11                ... fullscreen
//
// Similar features are implemented also in RealtimeLights sample,
// its source code is more open for your experiments, customization.
//
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/IO/ImportScene.h"
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
	#include <crtdbg.h>
	#include <direct.h>
	#include <windows.h>
#endif // _WIN32

void error(const char* message)
{
	rr::RRReporter::report(rr::ERRO,message);
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char** argv)
{
#ifdef _MSC_VER
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 14350;
#endif

	// Check for version mismatch
	if (!RR_INTERFACE_OK)
	{
		printf(RR_INTERFACE_MISMATCH_MSG);
		error("");
	}
	// Log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
	//rr::RRReporter::setFilter(true,3,true);
	//rr_gl::Program::logMessages(1);

	// Necessary for sceneViewer to use our file loaders.
	rr_io::registerLoaders();

#ifdef _WIN32
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	_chdir(exedir);
	free(exedir);
#endif // _WIN32

	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError);

	const char* sceneFilename = (argc>1)?argv[1]:"../../data/scenes/koupelna/koupelna4.dae";
#if 1
	// View scene _on disk_ in scene viewer.
	// Works only for scenes in registered file formats.
	// See how adapters in src/LightsprintIO/ImportXXX/RRObjectXXX.cpp register,
	// you can do the same for your own file format.
	rr_gl::SceneViewerState svs;
	svs.openLogWindows = true;
#ifdef NDEBUG
	// release returns quickly without freeing resources
	rr_gl::sceneViewer(NULL,sceneFilename,"../../data/maps/skybox/skybox_%s.jpg","../../data/shaders/",&svs,false);
	return 0;
#endif
	// debug frees everything and reports memory leaks
	rr_gl::sceneViewer(NULL,sceneFilename,"../../data/maps/skybox/skybox_%s.jpg","../../data/shaders/",&svs,true);
#else
	// View scene _in memory_ in scene viewer.

	// First, we have to load scene to memory.
	rr::RRScene scene(sceneFilename);
	// If you already have scene in memory in your own data structures, write adapter
	// (see "doc/Lightsprint.chm/Integration" page) and adapt it to RRObjects and RRLights here.

	// Send scene to GI solver
	rr::RRDynamicSolver* solver = new rr::RRDynamicSolver();
	solver->setScaler(rr::RRScaler::createRgbScaler()); // switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setStaticObjects(scene.objects,NULL);
	solver->setLights(scene.lights);
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// View solver in scene viewer
	rr_gl::sceneViewer(solver,sceneFilename,NULL,"../../data/shaders/",NULL,true);

	// Cleanup
	delete solver->getEnvironment();
	delete solver->getScaler();
	delete solver;
#endif

	delete reporter;

	return 0;
}
