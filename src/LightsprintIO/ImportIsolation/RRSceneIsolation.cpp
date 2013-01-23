// --------------------------------------------------------------------------
// Lightsprint import isolation.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_ISOLATION

#include "RRSceneIsolation.h"

#include <string>
#include <process.h>

#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

#include <windows.h>

using namespace rr;


static bool s_isolationEnabled = false;

RRScene* loadIsolated(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
{
	if (!s_isolationEnabled)
		return NULL;

	bf::path input = RR_RR2PATH(filename);
	
	// convert scene to temp
	// must be in the same directory, otherwise relative texture paths would fail
	char name[15];
	sprintf(name,"temp%04d.rr3",rand()%10000);
	bf::path output = input.parent_path() / name;

	// find out our filename
	wchar_t thisProgramFilename[MAX_PATH];
	GetModuleFileNameW(NULL,thisProgramFilename,MAX_PATH);

	{
		RRReportInterval report(INF2,"Isolated process converts it to .rr3...\n");
		int exitCode = _wspawnl(_P_WAIT,
			thisProgramFilename,
			(std::wstring(L"\"")+thisProgramFilename+L"\"").c_str(),
			L"-isolated-conversion",
			(std::wstring(L"\"")+input.wstring()+L"\"").c_str(),
			(std::wstring(L"\"")+output.wstring()+L"\"").c_str(),
			NULL);
		if (exitCode!=0) // 0=success
		{
			RRReporter::report(WARN,"Isolated conversion failed with exit code %d.\n",exitCode);
			return NULL;
		}
	}

	// load scene from temp
	RRScene* scene = new RRScene(RR_PATH2RR(output),textureLocator,aborting);

	// delete scene from temp
	boost::system::error_code ec;
	bf::remove(output,ec);

	// done
	return scene;
}

// to be called after non-isolated loaders, before isolated ones
void registerLoaderIsolationStep1()
{
	int argc = 0;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc==4 && argvw && argvw[1] && argvw[2] && argvw[3] && std::wstring(argvw[1])==L"-isolated-conversion")
	{
		// we are isolated process, converting scene to rr3
		// but other loaders are not registered yet, let's wait until step2
		return;
	}
	// we still don't know whether user wants isolation
	// but we must register it now (after .rr3, before the rest)
	// it won't work until someone sets s_isolationEnabled
	// [#14] : is our symbol that matches all files, it overrides all scene loaders registered later
	// *.* would work too, but fileselector with "All fileformats|*.rr3;*.*;*.dae...." would show all files,
	// we had to replace *.* with something readable for us, unreadable for fileselector
	RRScene::registerLoader(":",loadIsolated);
}

// to be called after isolated loaders
// (no loaders should be registered in future, our "*.*" would run isolated process, but future loaders are not registered yet, isolated process would fail to import here)
void registerLoaderIsolationStep2()
{
	int argc = 0;
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc==4 && argvw && argvw[1] && argvw[2] && argvw[3] && std::wstring(argvw[1])==L"-isolated-conversion")
	{
		// we are isolated process, converting scene to rr3
		// other loaders are already registered
		RRDynamicSolver* solver = new RRDynamicSolver;
		RRReporter::createWindowedReporter(solver,"Isolated scene import...");
		
		// a) load without textures - faster
		//  texture paths are stored in stub buffers and forwarded into rr3, so in most cases everything works
		//  problem is "transparency in diffuse texture". we can't decide whether to copy diffuse channel into transparency channel
		//  without loading diffuse texture first. all major scene formats do it, so we need nearly all textures loaded.
		//  copying to transparency in .rr3 loader would make it impossible to delete such transparency texture, it would return back at next load.
		//RRFileLocator* textureLocator = RRFileLocator::create();
		//textureLocator->setAttempt(0,""); // load without textures, to make it faster
		//textureLocator->setAttempt(RRFileLocator::ATTEMPT_STUB,"yes"); // but with stubs, so that links to textures are not lost
		//RRScene scene(argvw[2], textureLocator, &solver->aborting);

		// b) load with textures - slower
		RRScene scene(argvw[2], NULL, &solver->aborting);

		bool saved = scene.save(argvw[3]);
		exit(saved?0:1); // 0=success
	}
	s_isolationEnabled = true;
}

#endif
