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
// right mouse button ... look
// wheel              ... zoom
// F11                ... fullscreen
//
// Similar features are implemented also in RealtimeLights sample,
// its source code is more open for your experiments, customization.
//
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/IO/ImportScene.h"
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
	#include <crtdbg.h>
	#include <direct.h>
	#include <windows.h>
#endif // _WIN32

void error(const char* message, bool gfxRelated)
{
	rr::RRReporter::report(rr::ERRO,message);
	if (gfxRelated)
		rr::RRReporter::report(rr::ERRO,"\nPlease update your graphics card drivers.\nIf it doesn't help, contact us at support@lightsprint.com.\n\nSupported graphics cards:\n - GeForce 5200-9800, 100-295 (including GeForce Go)\n - Radeon 9500-9800, X300-1950, HD2300-5970 (including Mobility Radeon)\n - subset of FireGL and Quadro families");
	printf("\n\nHit enter to close...");
	fgetc(stdin);
	exit(0);
}

int main(int argc, char **argv)
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
		error("",false);
	}
	// Log messages to console
	rr::RRReporter::setReporter(rr::RRReporter::createPrintfReporter());
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

	const char* sceneFilename = (argc>1)?argv[1]:"../../data/scenes/koupelna/koupelna4.dae";

	const char* licError = rr::loadLicense("../../data/licence_number");
	if (licError)
		error(licError,false);

#if 1
	// View scene _on disk_ in scene viewer.
	// Works only for scenes in registered file formats.
	// See how adapters in src/LightsprintIO/ImportXXX/RRObjectXXX.cpp register,
	// you can do the same for your own file format.
	rr_gl::SceneViewerState svs;
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
	solver->setStaticObjects(scene.getObjects(),NULL);
	solver->setLights(scene.getLights());
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// View solver in scene viewer
	rr_gl::sceneViewer(solver,sceneFilename,NULL,"../../data/shaders/",NULL,true);

	// Cleanup
	delete solver->getEnvironment();
	delete solver->getScaler();
	delete solver;
#endif

	delete rr::RRReporter::getReporter();
	rr::RRReporter::setReporter(NULL);

	return 0;
}

// this is called only if you switch build setting from console to windowed
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	int argc;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	char** argv = new char*[argc+1];
	for (int i=0;i<argc;i++)
	{
		argv[i] = (char*)malloc(wcslen(argvw[i])+1);
		sprintf(argv[i], "%ws", argvw[i]);
	}
	argv[argc] = NULL;
	return main(argc,argv);
}
#endif
