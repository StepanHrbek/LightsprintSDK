// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVApp.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
#ifdef __APPLE__
	#include <boost/filesystem.hpp>
	namespace bf = boost::filesystem;
#endif
#ifdef SUPPORT_OCULUS
	#include "OVR.h"
	#ifdef NDEBUG
		#pragma comment(lib,"libovr." RR_LIB_COMPILER ".lib")
	#else
		#pragma comment(lib,"libovrd." RR_LIB_COMPILER ".lib")
	#endif
#endif

namespace rr_ed
{

/////////////////////////////////////////////////////////////////////////////
//
// DateTime

void tm_setNow(tm& a)
{
	time_t now = time(NULL);
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
		svframe = NULL;
	}
	bool OnInit()
	{
#ifdef __APPLE__
		// current dir is wrong here, fix it
		bf::current_path(s_initPath);
#endif
#ifdef SUPPORT_OCULUS
		OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
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
		svframe = NULL;
#ifdef SUPPORT_OCULUS
		OVR::System::Destroy();
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

void sceneViewer(rr::RRDynamicSolver* _inputSolver, const rr::RRString& _inputFilename, const rr::RRString& _skyboxFilename, const rr::RRString& _pathToData, SceneViewerState* _svs, bool _releaseResources)
{
	// randomize all rand()s
	srand (time(NULL));

	// immediately abort if requested
	if (_inputSolver && _inputSolver->aborting) return;

	// set initial values (user may change them interactively in scene viewer)
	s_svs = SceneViewerStateEx();
	if (_svs) s_svs.SceneViewerState::operator=(*_svs);
	if (!_inputFilename.empty())
	{
		s_svs.sceneFilename = RR_RR2WX(_inputFilename);
	}
	if (!_skyboxFilename.empty())
	{
		s_svs.skyboxFilename = _skyboxFilename;
	}
	s_svs.initialInputSolver = _inputSolver;
	s_svs.pathToData = RR_RR2WX(_pathToData);
	s_svs.pathToShaders = s_svs.pathToData+"shaders/";
	s_svs.pathToMaps = s_svs.pathToData+"maps/";
	s_svs.releaseResources = _releaseResources;

	wxApp::SetInitializerFunction(wxCreateApp);
	int argc = 0;
	char** argv = NULL;
	wxEntry(argc,argv);
}
 
}; // namespace
