// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "SVFrame.h"
#include "SVApp.h"

namespace rr_gl
{

// the only instance used by whole scene viewer
SceneViewerStateEx g_svse;

class SVApp: public wxApp
{
public:
	bool OnInit()
	{
		SVFrame::Create(g_svse);
		return true;
	}
};

static wxAppConsole *wxCreateApp()
{
	wxAppConsole::CheckBuildOptions(WX_BUILD_OPTIONS_SIGNATURE,"SceneViewer");
	return new SVApp;
}

void sceneViewer(rr::RRDynamicSolver* _solver, const char* _pathToShaders, SceneViewerState* _svs)
{
	// immediately abort if requested
	if (_solver && _solver->aborting) return;

	// set initial values (user may change them interactively in scene viewer)
	g_svse = SceneViewerStateEx();
	if (_svs) memcpy(&g_svse,_svs,sizeof(*_svs));
	g_svse.initialInputSolver = _solver;
	g_svse.manuallyOpenedScene = NULL;
	g_svse.pathToShaders = _pathToShaders;

	wxApp::SetInitializerFunction(wxCreateApp);
	wxEntry(0,NULL);
}
 
}; // namespace
