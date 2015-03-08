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
// --------------------------------------------------------------------------

#include "Lightsprint/Ed/Ed.h"
#include "Lightsprint/GL/Program.h"
#include "Lightsprint/IO/IO.h"
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
	#include <crtdbg.h>
	#include <direct.h>
	#include <windows.h>
#endif // _WIN32

int main(int argc, char** argv)
{
#ifdef _MSC_VER
	// check that we don't have memory leaks
	_CrtSetDbgFlag( (_CrtSetDbgFlag( _CRTDBG_REPORT_FLAG )|_CRTDBG_LEAK_CHECK_DF)&~_CRTDBG_CHECK_CRT_DF );
	//_crtBreakAlloc = 14350;
#endif
#if defined(_DEBUG) || !defined(_WIN32)
	// Log messages to console
	rr::RRReporter* reporter = rr::RRReporter::createPrintfReporter();
#endif
	//rr::RRReporter::setFilter(true,3,true);
	//rr_gl::Program::logMessages(1);

	// Necessary for sceneViewer to use our file loaders.
	rr_io::registerLoaders(argc,argv);
	// Improves robustness by sandboxing scene loaders.
	rr_io::isolateSceneLoaders(argc,argv);

#ifdef _WIN32
	// change current directory to exe directory, necessary when opening custom scene using drag&drop
	// (it is only necessary because of several relative paths below, sceneViewer() does not care about current directory)
	char* exedir = _strdup(argv[0]);
	for (unsigned i=(unsigned)strlen(exedir);--i;) if (exedir[i]=='/' || exedir[i]=='\\') {exedir[i]=0;break;}
	_chdir(exedir);
	free(exedir);
#endif // _WIN32

	const char* sceneFilename = (argc>1)?argv[1]:"../../data/scenes/koupelna/koupelna4.dae";
#if 1
	// View scene _on disk_ in scene viewer.
	// Works only for scenes in registered file formats.
	// See how adapters in src/LightsprintIO/ImportXXX/RRObjectXXX.cpp register,
	// you can do the same for your own file format.
	rr_ed::SceneViewerState svs;
	rr_ed::sceneViewer(nullptr,sceneFilename,"../../data/maps/skybox/skybox_%s.jpg","../../data/",&svs,
#ifdef _DEBUG
		// debug frees everything and reports memory leaks
		true);
#else
		// release returns quickly without freeing resources
		false);
	return 0;
#endif
#else
	// View scene _in memory_ in scene viewer.

	// First, we have to load scene to memory.
	rr::RRScene scene(sceneFilename);
	// If you already have scene in memory in your own data structures, write adapter
	// (see "doc/Lightsprint.chm/Integration" page) and adapt it to RRObjects and RRLights here.

	// Send scene to GI solver
	rr::RRSolver* solver = new rr::RRSolver();
	solver->setColorSpace(rr::RRColorSpace::create_sRGB()); // switch inputs and outputs from HDR physical scale to RGB screenspace
	solver->setStaticObjects(scene.objects,nullptr);
	solver->setDynamicObjects(scene.objects);
	solver->setLights(scene.lights);
	solver->setEnvironment(rr::RRBuffer::loadCube("../../data/maps/skybox/skybox_ft.jpg"));

	// View solver in scene viewer
	rr_ed::sceneViewer(solver,sceneFilename,nullptr,"../../data/",nullptr,true);

	// Cleanup
	delete solver->getEnvironment();
	delete solver->getColorSpace();
	delete solver;
#endif

#if defined(_DEBUG) || !defined(_WIN32)
	delete reporter;
#endif

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
// WinMain() calls main(), for compatibility with Windows

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShow)
{
	int argc = 0;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argvw && argc)
	{
		// build argv from commandline
		char** argv = new char*[argc+1];
		for (int i=0;i<argc;i++)
		{
			argv[i] = (char*)malloc(wcslen(argvw[i])+1);
			if (argv[i])
				sprintf(argv[i], "%ws", argvw[i]);
		}
		argv[argc] = nullptr;
		return main(argc,argv);
	}
	else
	{
		// someone calls us with invalid arguments, but don't panic, build argv from module filename
		char szFileName[MAX_PATH];
		GetModuleFileNameA(nullptr,szFileName,MAX_PATH);
		char* argv[2] = {szFileName,nullptr};
		return main(1,argv);
	}
}
#endif
