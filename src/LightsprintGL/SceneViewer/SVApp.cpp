// --------------------------------------------------------------------------
// Scene viewer - entry point with wxWidgets application.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#include "SVFrame.h"
#include "SVApp.h"

namespace rr_gl
{

SceneViewerParameters g_params;

class SVApp: public wxApp
{
public:
	bool OnInit()
	{
		SVFrame::Create(g_params);
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

	g_params.solver = _solver;
	g_params.pathToShaders = _pathToShaders;
	g_params.svs = _svs ? *_svs : SceneViewerState();

	wxApp::SetInitializerFunction(wxCreateApp);
	wxEntry(0,NULL);
}
 
}; // namespace
