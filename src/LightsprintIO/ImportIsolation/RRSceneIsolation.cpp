// --------------------------------------------------------------------------
// Lightsprint import isolation.
// Copyright (C) 2012 Stepan Hrbek, Lightsprint. All rights reserved.
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
		int exitCode = _wspawnl(_P_WAIT,thisProgramFilename,thisProgramFilename,L"-isolated-conversion",input.wstring().c_str(),output.wstring().c_str(),NULL);
		if (exitCode!=0) // 0=success
		{
			RRReporter::report(WARN,"Isolated conversion failed with exit code %d.\n",exitCode);
			return NULL;
		}
	}

	// load scene from temp
	RRScene* scene = new RRScene(RR_PATH2RR(output),textureLocator,aborting);

	// delete scene from temp
	bf::remove(output);

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
	RRScene::registerLoader("*.*",loadIsolated); // *.* overrides all scene loaders registered later
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
		RRReporter::createWindowedReporter(solver,"Importing scene...");
		RRFileLocator* textureLocator = RRFileLocator::create();
		textureLocator->setAttempt(0,""); // load without textures, to make it faster
		textureLocator->setAttempt(RRFileLocator::ATTEMPT_STUB,"yes"); // but with stubs, so that links to textures are not lost
		RRScene scene(argvw[2], textureLocator, &solver->aborting);
		bool saved = scene.save(argvw[3]);
		exit(saved?0:1); // 0=success
	}
	s_isolationEnabled = true;
}

#endif