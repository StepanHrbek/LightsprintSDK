// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifdef SUPPORT_SCENEVIEWER

#include "SVApp.h"
#include "SVFrame.h"

namespace rr_gl
{

// the only instance used by whole scene viewer
SceneViewerStateEx g_svs;

class SVApp: public wxApp
{
public:
	bool OnInit()
	{
		SVFrame::Create(g_svs);
		return true;
	}
};

static wxAppConsole *wxCreateApp()
{
	wxAppConsole::CheckBuildOptions(WX_BUILD_OPTIONS_SIGNATURE,"SceneViewer");
	return new SVApp;
}

void sceneViewer(rr::RRDynamicSolver* _inputSolver, const char* _inputFilename, const char* _skyboxFilename, const char* _pathToShaders, SceneViewerState* _svs, bool _releaseResources)
{
	// immediately abort if requested
	if (_inputSolver && _inputSolver->aborting) return;

	// set initial values (user may change them interactively in scene viewer)
	g_svs = SceneViewerStateEx();
	if (_svs) memcpy(&g_svs,_svs,sizeof(*_svs));
	if (_inputFilename)
	{
		g_svs.sceneFilename = _inputFilename;
	}
	if (_skyboxFilename)
	{
		g_svs.skyboxFilename = _skyboxFilename;
	}
	g_svs.initialInputSolver = _inputSolver;
	g_svs.pathToShaders = _pathToShaders;
	g_svs.releaseResources = _releaseResources;

	wxApp::SetInitializerFunction(wxCreateApp);
	wxEntry(0,NULL);
}
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER
