// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - entry point of wxWidgets based application.
// --------------------------------------------------------------------------

#include "SVApp.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;
#ifdef SUPPORT_OCULUS
	#include "OVR.h"
	#if defined(_M_X64) || defined(_LP64)
		#define RR_64 "64"
	#else
		#define RR_64
	#endif
	#ifdef NDEBUG
		#pragma comment(lib,"libovr" RR_64)
		#pragma comment(linker, "/NODEFAULTLIB:libcpmt.lib") // libovr is compiled with static crt, force it to use our dll crt
	#else
		#pragma comment(lib,"libovr" RR_64 "d")
		#pragma comment(linker, "/NODEFAULTLIB:libcpmtd.lib")
	#endif
	#pragma comment(lib,"ws2_32.lib")
#endif

	#define NORMALIZED(x) RR_PATH2RR(bf::system_complete(RR_RR2PATH(x)).make_preferred()) // make paths absolute

namespace rr_ed
{

/////////////////////////////////////////////////////////////////////////////
//
// DateTime

void tm_setNow(tm& a)
{
	time_t now = time(nullptr);
	a = *localtime(&now);
}

void tm_addSeconds(tm& a, double _seconds)
{
	//_seconds += a.tm_nsec*1e-9;
	//a.tm_nsec = fmod(_seconds,1)*1e9;
	unsigned seconds = (unsigned)_seconds;
	if (seconds)
	{
		seconds += a.tm_sec;
		a.tm_sec = seconds%60;
		unsigned minutes = seconds/60;
		if (minutes)
		{
			minutes += a.tm_min;
			a.tm_min = minutes%60;
			unsigned hours = minutes/60;
			if (hours)
			{
				hours += a.tm_hour;
				a.tm_hour = hours%24;
				unsigned days = hours/24;
				if (days)
				{
					// it becomes complicated here
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// SVApp

// the only instance used by whole scene viewer
static SceneViewerStateEx s_svs;
#ifdef __APPLE__
	static bf::path s_initPath;
#endif

class SVApp: public wxApp
{
public:
	SVFrame* svframe;
	SVApp()
	{
		svframe = nullptr;
	}
	bool OnInit()
	{
#ifdef __APPLE__
		// current dir is wrong here, fix it
		bf::current_path(s_initPath);
#endif
#ifdef SUPPORT_OCULUS
		ovr_Initialize();
#endif
		svframe = SVFrame::Create(s_svs);
		return true;
	}
	int FilterEvent(wxEvent& event)
	{
		if (event.GetEventType()!=wxEVT_KEY_DOWN || !svframe || !svframe->m_canvas)
			return Event_Skip;
		return svframe->m_canvas->FilterEvent((wxKeyEvent&)event);
	}
	virtual int OnExit()
	{
		svframe = nullptr;
#ifdef SUPPORT_OCULUS
		ovr_Shutdown();
#endif
		return 0;
	}
};

static wxAppConsole *wxCreateApp()
{
	wxAppConsole::CheckBuildOptions(WX_BUILD_OPTIONS_SIGNATURE,"SceneViewer");
#ifdef __APPLE__
	// this is the last place with current dir still ok, remember it
	s_initPath = bf::current_path();
#endif
	return new SVApp;
}

void sceneViewer(rr::RRSolver* _inputSolver, const rr::RRString& _inputFilename, const rr::RRString& _skyboxFilename, const rr::RRString& _pathToData, SceneViewerState* _svs, bool _releaseResources)
{
	// randomize all rand()s
	srand (time(nullptr));

	// immediately abort if requested
	if (_inputSolver && _inputSolver->aborting) return;

	// set initial values (user may change them interactively in scene viewer)
	s_svs = SceneViewerStateEx();
	if (_svs) s_svs.SceneViewerState::operator=(*_svs);
	if (!_inputFilename.empty())
	{
		s_svs.sceneFilename = NORMALIZED(_inputFilename);
	}
	if (!_skyboxFilename.empty())
	{
		s_svs.skyboxFilename = NORMALIZED(_skyboxFilename);
	}
	s_svs.initialInputSolver = _inputSolver;
	s_svs.pathToData = NORMALIZED(_pathToData);
	s_svs.pathToShaders = s_svs.pathToData+"shaders/";
	s_svs.pathToMaps = s_svs.pathToData+"maps/";
	s_svs.releaseResources = _releaseResources;

	wxApp::SetInitializerFunction(wxCreateApp);
	int argc = 0;
	char** argv = nullptr;
	wxEntry(argc,argv);
}
 
}; // namespace
