// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Lightsprint import/export isolation.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_ISOLATION

#include "RRSceneIsolation.h"

#include <string>

#include <filesystem>
namespace bf = std::filesystem;

#ifdef _WIN32
	#include <windows.h>
	#include <process.h> // _spawnl
#else
	#include <sys/types.h>
	#include <sys/wait.h> // waitpid
	#include <unistd.h> // execl
#endif

using namespace rr;


static bool s_isolationEnabled = false;
#ifndef _WIN32
	static const char* s_thisProgramFilename = nullptr;
#endif

static bool callSelf(bf::path input, bf::path output)
{
	RRReportInterval report(INF2,"Isolated process converts it to .rr3...\n");
#ifdef _WIN32
	wchar_t thisProgramFilename[MAX_PATH];
	GetModuleFileNameW(nullptr,thisProgramFilename,MAX_PATH);
	intptr_t exitCode = _wspawnl(_P_WAIT,
		thisProgramFilename,
		(std::wstring(L"\"")+thisProgramFilename+L"\"").c_str(), // add "", filenames with spaces need it
		L"-isolated-conversion",
		(std::wstring(L"\"")+input.wstring()+L"\"").c_str(),
		(std::wstring(L"\"")+output.wstring()+L"\"").c_str(),
		nullptr);
	bool success = exitCode==0;
	if (!success)
	{
		RRReporter::report(WARN,"Isolated conversion failed with exit code %d.\n",exitCode);
		return false;
	}
#else
	pid_t child_pid = fork();
	if (!child_pid)
	{
		// other option is to convert scene here, instead of execl. would it work?
    	execl(s_thisProgramFilename,
			s_thisProgramFilename, // don't add "", filenames with spaces already work fine
			"-isolated-conversion",
			input.string().c_str(),
			output.string().c_str(),
			nullptr);
		// child only gets here if exec fails
		RRReporter::report(WARN,"Isolated conversion failed execl().\n");
		exit(0);
	}
	int status = 0;
	pid_t waitResult = waitpid(child_pid,&status,0);
	int exitCode = WEXITSTATUS(status);
	bool success = waitResult>0 && WIFEXITED(status) && exitCode==0;
	if (!success)
	{
		RRReporter::report(WARN,"Isolated conversion failed with waitResult=%d exited=%d exitcode=%d.\n",(int)waitResult,WIFEXITED(status)?1:0,exitCode);
		return false;
	}
#endif
	return true;
}

RRScene* loadIsolated(const RRString& filename, RRFileLocator* textureLocator, bool* aborting)
{
	if (!s_isolationEnabled)
		return nullptr;

	// prepare filenames
	// temp.rr3 must be in the same directory, otherwise relative texture paths would fail
	bf::path input = RR_RR2PATH(filename);
	char name[15];
	sprintf(name,"temp%04d.rr3",rand()%10000);
	bf::path temp = input.parent_path() / name;

	// convert filename to temp.rr3 (call self to do the dirty job)
	callSelf(input,temp);

	// load temp.rr3
	RRScene* scene = new RRScene(RR_PATH2RR(temp),textureLocator,aborting);

	// delete temp.rr3
	std::error_code ec;
	bf::remove(temp,ec);

	// done
	return scene;
}

bool saveIsolated(const RRScene* scene, const RRString& filename)
{
	// prepare filenames
	// temp.rr3 must be in the same directory, otherwise relative texture paths would fail
	bf::path output = RR_RR2PATH(filename);
	char name[15];
	sprintf(name,"temp%04d.rr3",rand()%10000);
	bf::path temp = output.parent_path() / name;

	if (!scene || !s_isolationEnabled || (output.extension().string()==".rr3" || output.extension().string()==".RR3"))
	{
		// when non-isolated save to rr3 fails, isolated save is attempted
		// here we abort it immediately
		// [#14] we can't easily unregister rr3 from isolated save, : symbol matches everything
		return false;
	}

	// save temp.rr3
	scene->save(RR_PATH2RR(temp));

	// convert temp.rr3 to filename (call self to do the dirty job)
	callSelf(temp,output);

	// delete temp.rr3
	std::error_code ec;
	bf::remove(temp,ec);

	// done
	return scene;
}

// to be called after non-isolated loaders, before isolated ones
void registerIsolationStep1(int argc, char** argv)
{
#ifdef _WIN32
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	bool isolated = argc==4 && argvw && argvw[1] && argvw[2] && argvw[3] && std::wstring(argvw[1])==L"-isolated-conversion";
	LocalFree(argvw);
	if (isolated)
#else
	s_thisProgramFilename = argv[0];
	if (argc==4 && argv && argv[1] && argv[2] && argv[3] && std::string(argv[1])=="-isolated-conversion")
#endif
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
	RRScene::registerSaver(":",saveIsolated);
}

// to be called after isolated loaders
// (no loaders should be registered in future, our "*.*" would run isolated process, but future loaders are not registered yet, isolated process would fail to import here)
void registerIsolationStep2(int argc, char** argv)
{
#ifdef _WIN32
	LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc==4 && argvw && argvw[1] && argvw[2] && argvw[3] && std::wstring(argvw[1])==L"-isolated-conversion")
#else
	if (argc==4 && argv && argv[1] && argv[2] && argv[3] && std::string(argv[1])=="-isolated-conversion")
#endif
	{
		// we are isolated process, converting scene to rr3
		// other loaders are already registered
		RRSolver* solver = new RRSolver;
		RRReporter::createWindowedReporter(solver,"Isolated scene conversion...");

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
		RRFileLocator* textureLocator = RRFileLocator::create();
		textureLocator->setAttempt(RRFileLocator::ATTEMPT_STUB,"yes"); // load with stubs, so that paths to missing textures are not lost
		bool saved;
		{
#ifdef _WIN32
			RRScene scene(argvw[2], textureLocator, &solver->aborting);
			saved = scene.save(argvw[3]);
#else
			RRScene scene(argv[2], textureLocator, &solver->aborting);
			saved = scene.save(argv[3]);
#endif
			// free scene by leaving scope (otherwise leaks are reported)
		}
		exit(saved?0:1); // 0=success
	}
#ifdef _WIN32
	LocalFree(argvw);
#endif
	s_isolationEnabled = true;
}

#endif
