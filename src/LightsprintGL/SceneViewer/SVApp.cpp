// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVApp.h"
#include "SVFrame.h"
#include "SVObjectProperties.h"
#ifdef __APPLE__
	#include <boost/filesystem.hpp>
	namespace bf = boost::filesystem;
#endif

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// DateTime

DateTime::DateTime()
{
	setNow();
}

void DateTime::setNow()
{
	time_t now = time(NULL);
	*(tm*)this = *localtime(&now);
	tm_nsec = 0;
}

void DateTime::addSeconds(double _seconds)
{
	_seconds += tm_nsec*1e-9;
	tm_nsec = fmod(_seconds,1)*1e9;
	unsigned seconds = (unsigned)_seconds;
	if (seconds)
	{
		seconds += tm_sec;
		tm_sec = seconds%60;
		unsigned minutes = seconds/60;
		if (minutes)
		{
			minutes += tm_min;
			tm_min = minutes%60;
			unsigned hours = minutes/60;
			if (hours)
			{
				hours += tm_hour;
				tm_hour = hours%24;
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
	bool OnInit()
	{
#ifdef __APPLE__
		// current dir is wrong here, fix it
		bf::current_path(s_initPath);
#endif
		SVFrame::Create(s_svs);
		return true;
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

void sceneViewer(rr::RRDynamicSolver* _inputSolver, const char* _inputFilename, const char* _skyboxFilename, const char* _pathToData, SceneViewerState* _svs, bool _releaseResources)
{
	// randomize all rand()s
	srand (time(NULL));

	// immediately abort if requested
	if (_inputSolver && _inputSolver->aborting) return;

	// set initial values (user may change them interactively in scene viewer)
	s_svs = SceneViewerStateEx();
	if (_svs) memcpy(&s_svs,_svs,sizeof(*_svs));
	if (_inputFilename)
	{
		s_svs.sceneFilename = _inputFilename;
	}
	if (_skyboxFilename)
	{
		s_svs.skyboxFilename = _skyboxFilename;
	}
	s_svs.initialInputSolver = _inputSolver;
	s_svs.pathToData = _pathToData;
	s_svs.pathToShaders = _strdup(s_svs.pathToData+"shaders/");
	s_svs.pathToMaps = s_svs.pathToData+"maps/";
	s_svs.releaseResources = _releaseResources;

	wxApp::SetInitializerFunction(wxCreateApp);
	int argc = 0;
	char** argv = NULL;
	wxEntry(argc,argv);

	RR_SAFE_FREE(s_svs.pathToShaders);
}
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
